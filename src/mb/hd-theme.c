/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author: Tomas Frydrych <tf@o-hand.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "hd-theme.h"
#include "hd-comp-mgr.h"
#include "hd-app.h"

#include <matchbox/theme-engines/mb-wm-theme-xml.h>

#define BACK_BUTTON_TIMEOUT 2000

static MBWMDecor * hd_theme_create_decor (MBWMTheme             *theme,
					  MBWindowManagerClient *client,
					  MBWMDecorType          type);

static void hd_theme_simple_paint_button (MBWMTheme *theme,
					  MBWMDecorButton *button);

static void
hd_theme_simple_get_button_size (MBWMTheme             *theme,
				 MBWMDecor             *decor,
				 MBWMDecorButtonType    type,
				 int                   *width,
				 int                   *height);

static void
hd_theme_class_init (MBWMObjectClass *klass)
{
  MBWMThemeClass *t_class = MB_WM_THEME_CLASS (klass);

  t_class->create_decor = hd_theme_create_decor;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdTheme";
#endif
}

static void
hd_theme_destroy (MBWMObject *obj)
{
}

static int
hd_theme_init (MBWMObject *obj, va_list vap)
{
  return 1;
}

int
hd_theme_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdThemeClass),
	sizeof (HdTheme),
	hd_theme_init,
	hd_theme_destroy,
	hd_theme_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_THEME_PNG, 0);
    }

  return type;
}

typedef struct _BackButtonData
{
  MBWMDecorButton *button;

  guint            timeout_id;
  gboolean         timeout_handled : 1;
} BackButtonData;

static void
back_button_data_free (BackButtonData *bd)
{
  g_free (bd);
}

static void
back_button_data_destroy (MBWMDecorButton *button, void *bd)
{
  back_button_data_free (bd);
}

static gboolean
back_button_timeout (gpointer data)
{
  BackButtonData        *bd = data;
  MBWindowManagerClient *c;
  MBWindowManagerClient *leader;
  MBWindowManager       *wm;
  HdApp                 *app;

  c = bd->button->decor->parent_client;
  wm = c->wmref;

  bd->timeout_handled = TRUE;
  bd->timeout_id = 0;

  /* TODO -- kill all windows in this group other than the primary */
  if (!HD_IS_APP (c))
    {
      g_warning ("Custom button on a something other than App.");
      return FALSE;
    }

  app = HD_APP (c);

  leader = hd_app_close_followers (app);
  mb_wm_activate_client (wm, leader);

  return FALSE;
}

static void
back_button_press_handler (MBWindowManager   *wm,
			   MBWMDecorButton   *button,
			   void              *userdata)
{
  BackButtonData *bd = userdata;

  bd->timeout_id =
    g_timeout_add_full (G_PRIORITY_HIGH_IDLE,
			BACK_BUTTON_TIMEOUT, back_button_timeout, bd, NULL);

  bd->timeout_handled = FALSE;
}

static void
back_button_release_handler (MBWindowManager   *wm,
			     MBWMDecorButton   *button,
			     void              *userdata)
{
  BackButtonData        *bd = userdata;
  MBWindowManagerClient *c;
  MBWindowManagerClient *prev;
  HdApp                 *app;

  if (bd->timeout_handled)
    return;

  g_source_remove (bd->timeout_id);
  bd->timeout_id = 0;

  /* TODO -- switch to previous window in group */
  c = button->decor->parent_client;

  if (!HD_IS_APP (c))
    {
      g_warning ("Custom button on a something other than App.");
      return;
    }

  app = HD_APP (c);

  prev = hd_app_get_prev_group_member (app);
  mb_wm_activate_client (wm, prev);
}


static void
construct_buttons (MBWMTheme *theme, MBWMDecor *decor, MBWMXmlDecor *d)
{
  MBWindowManagerClient *client = decor->parent_client;
  MBWindowManager       *wm     = client->wmref;
  MBWMDecorButton       *button = NULL;
  gboolean               is_leader = TRUE;

  if (MB_WM_CLIENT_CLIENT_TYPE (client) == MBWMClientTypeApp)
    {
      HdApp *app = HD_APP (client);

      is_leader = !app->secondary_window;
    }

  if (d)
    {
      MBWMList * l = d->buttons;
      while (l)
	{
	  MBWMXmlButton * b = l->data;

	  /* Back button only for group followers */
	  if (b->type == HdHomeThemeButtonBack && !is_leader)
	    {
	      BackButtonData *bd;

	      button = mb_wm_decor_button_new (wm,
					       b->type,
					       b->packing,
					       decor,
					       back_button_press_handler,
					       back_button_release_handler,
					       0);

	      bd = g_new0 (BackButtonData, 1);
	      bd->button = button;

	      mb_wm_decor_button_set_user_data (button, bd,
						back_button_data_destroy);
	    }
	  /* No close button for group followers */
	  else if (b->type != MBWMDecorButtonClose || is_leader)
	    {
	      button = mb_wm_decor_button_stock_new (wm,
						     b->type,
						     b->packing,
						     decor,
						     0);
	    }
	  else
	    button = NULL;

	  if (button)
	    {
	      mb_wm_decor_button_show (button);
	      mb_wm_object_unref (MB_WM_OBJECT (button));
	    }

	  l = l->next;
	}
    }
  else
    {
      if (is_leader)
	{
	  button = mb_wm_decor_button_stock_new (wm,
						 MBWMDecorButtonClose,
						 MBWMDecorButtonPackEnd,
						 decor,
						 0);
	}
      else
	{
	  BackButtonData *bd;

	  button = mb_wm_decor_button_new (wm,
					   HdHomeThemeButtonBack,
					   MBWMDecorButtonPackEnd,
					   decor,
					   back_button_press_handler,
					   back_button_release_handler,
					   0);

	  bd = g_new0 (BackButtonData, 1);
	  mb_wm_decor_button_set_user_data (button, bd,
					    back_button_data_destroy);
	}

      mb_wm_decor_button_show (button);
      mb_wm_object_unref (MB_WM_OBJECT (button));
    }
}

static MBWMDecor *
hd_theme_create_decor (MBWMTheme             *theme,
		       MBWindowManagerClient *client,
		       MBWMDecorType          type)
{
  MBWMClientType   c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMDecor       *decor = NULL;
  MBWindowManager *wm = client->wmref;
  MBWMXmlClient   *c;

  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)))
    {
      MBWMXmlDecor *d;

      d = mb_wm_xml_decor_find_by_type (c->decors, type);

      if (d)
	{
	  decor = mb_wm_decor_new (wm, type);

	  decor->absolute_packing =
	    MB_WM_OBJECT_TYPE (theme) == HD_TYPE_THEME_SIMPLE ? False : True;

	  mb_wm_decor_attach (decor, client);
	  construct_buttons (theme, decor, d);
	}
    }

  if (!decor)
    {
      switch (c_type)
	{
	case MBWMClientTypeApp:
	  switch (type)
	    {
	    case MBWMDecorTypeNorth:
	      decor = mb_wm_decor_new (wm, type);
	      mb_wm_decor_attach (decor, client);
	      construct_buttons (theme, decor, NULL);
	      break;
	    default:
	      decor = mb_wm_decor_new (wm, type);
	      mb_wm_decor_attach (decor, client);
	    }
	  break;

	case MBWMClientTypeDialog:
	  decor = mb_wm_decor_new (wm, type);
	  mb_wm_decor_attach (decor, client);
	  break;

	case MBWMClientTypePanel:
	case MBWMClientTypeDesktop:
	case MBWMClientTypeInput:
	default:
	  decor = mb_wm_decor_new (wm, type);
	  mb_wm_decor_attach (decor, client);
	}
    }

  return decor;
}

static void
hd_theme_simple_class_init (MBWMObjectClass *klass)
{
  MBWMThemeClass *t_class = MB_WM_THEME_CLASS (klass);

  t_class->create_decor = hd_theme_create_decor;
  t_class->paint_button = hd_theme_simple_paint_button;
  t_class->button_size  = hd_theme_simple_get_button_size;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdThemeSimple";
#endif
}

static void
hd_theme_simple_destroy (MBWMObject *obj)
{
}

static int
hd_theme_simple_init (MBWMObject *obj, va_list vap)
{
  return 1;
}

int
hd_theme_simple_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdThemeSimpleClass),
	sizeof (HdThemeSimple),
	hd_theme_simple_init,
	hd_theme_simple_destroy,
	hd_theme_simple_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_THEME, 0);
    }

  return type;
}

MBWMTheme *
hd_theme_alloc_func (int theme_type, ...)
{
  MBWMTheme       *theme;
  char            *path = NULL;
  MBWMList        *xml_clients = NULL;
  char            *img = NULL;
  MBWMColor       *clr_lowlight = NULL;
  MBWMColor       *clr_shadow = NULL;
  Bool             compositing = False;
  Bool             shaped = False;
  MBWindowManager *wm = NULL;
  MBWMObjectProp   prop;
  va_list          vap;
  MBWMCompMgrShadowType shadow_type = MBWM_COMP_MGR_SHADOW_NONE;

  va_start (vap, theme_type);

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropWm:
	  wm = va_arg (vap, MBWindowManager *);
	  break;
	case MBWMObjectPropThemePath:
	  path = va_arg (vap, char *);
	  break;
	case MBWMObjectPropThemeImg:
	  img = va_arg (vap, char *);
	  break;
	case MBWMObjectPropThemeXmlClients:
	  xml_clients = va_arg (vap, MBWMList *);
	  break;
	case MBWMObjectPropThemeColorLowlight:
	  clr_lowlight = va_arg (vap, MBWMColor *);
	  break;
	case MBWMObjectPropThemeColorShadow:
	  clr_shadow = va_arg (vap, MBWMColor *);
	  break;
	case MBWMObjectPropThemeShadowType:
	  shadow_type = va_arg (vap, MBWMCompMgrShadowType);
	  break;
	case MBWMObjectPropThemeCompositing:
	  compositing = va_arg (vap, Bool);
	  break;
	case MBWMObjectPropThemeShaped:
	  shaped = va_arg (vap, Bool);
	  break;

	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  va_end(vap);

  /* If some other theme than our own was requested,
   * fallback on the simple one
   */
  if (theme_type != HD_TYPE_THEME)
    {
      theme = MB_WM_THEME (mb_wm_object_new (HD_TYPE_THEME_SIMPLE,
			   MBWMObjectPropWm,                  wm,
			   MBWMObjectPropThemePath,           path,
			   MBWMObjectPropThemeImg,            img,
			   MBWMObjectPropThemeXmlClients,     xml_clients,
			   MBWMObjectPropThemeColorLowlight,  clr_lowlight,
			   MBWMObjectPropThemeColorShadow,    clr_shadow,
			   MBWMObjectPropThemeShadowType,     shadow_type,
			   MBWMObjectPropThemeCompositing,    compositing,
			   MBWMObjectPropThemeShaped,         shaped,
			   NULL));
    }
  else
    {
      theme = MB_WM_THEME (mb_wm_object_new (HD_TYPE_THEME,
			   MBWMObjectPropWm,                  wm,
			   MBWMObjectPropThemePath,           path,
			   MBWMObjectPropThemeImg,            img,
			   MBWMObjectPropThemeXmlClients,     xml_clients,
			   MBWMObjectPropThemeColorLowlight,  clr_lowlight,
			   MBWMObjectPropThemeColorShadow,    clr_shadow,
			   MBWMObjectPropThemeShadowType,     shadow_type,
			   MBWMObjectPropThemeCompositing,    compositing,
			   MBWMObjectPropThemeShaped,         shaped,
			   NULL));
    }

  return theme;
}

/*
 * Copied from mb-wm-theme.c; must be kept in sync (this is for debuggin
 * only anyway).
 */
#include <X11/Xft/Xft.h>
struct DecorData
{
  Pixmap            xpix;
  XftDraw          *xftdraw;
  XftColor          clr;
  XftFont          *font;
};

static unsigned long
pixel_from_clr (Display * dpy, int screen, MBWMColor * clr)
{
  XColor xcol;

  xcol.red   = (int)(clr->r * (double)0xffff);
  xcol.green = (int)(clr->g * (double)0xffff);
  xcol.blue  = (int)(clr->b * (double)0xffff);
  xcol.flags = DoRed|DoGreen|DoBlue;

  XAllocColor (dpy, DefaultColormap (dpy, screen), &xcol);

  return xcol.pixel;
}

static void
hd_theme_simple_paint_back_button (MBWMTheme *theme, MBWMDecorButton *button)
{
  MBWMDecor             *decor;
  MBWindowManagerClient *client;
  Window                 xwin;
  MBWindowManager       *wm = theme->wm;
  int                    x, y, w, h;
  MBWMColor              clr_bg;
  MBWMColor              clr_fg;
  MBWMClientType         c_type;
  MBWMXmlClient         *c = NULL;
  MBWMXmlDecor          *d = NULL;
  MBWMXmlButton         *b = NULL;
  struct DecorData * dd;
  GC                     gc;
  Display               *xdpy = wm->xdpy;
  int                    xscreen = wm->xscreen;

  clr_fg.r = 1.0;
  clr_fg.g = 1.0;
  clr_fg.b = 1.0;

  clr_bg.r = 0.0;
  clr_bg.g = 0.0;
  clr_bg.b = 0.0;

  decor = button->decor;
  client = mb_wm_decor_get_parent (decor);
  xwin = decor->xwin;
  dd = mb_wm_decor_get_theme_data (decor);

  if (client == NULL || xwin == None || dd->xpix == None)
    return;

  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
      (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type)) &&
      (b = mb_wm_xml_button_find_by_type (d->buttons, MBWMDecorButtonClose)))
    {
      clr_fg.r = b->clr_fg.r;
      clr_fg.g = b->clr_fg.g;
      clr_fg.b = b->clr_fg.b;

      clr_bg.r = b->clr_bg.r;
      clr_bg.g = b->clr_bg.g;
      clr_bg.b = b->clr_bg.b;
    }

  w = button->geom.width;
  h = button->geom.height;
  x = button->geom.x;
  y = button->geom.y;

  gc = XCreateGC (xdpy, dd->xpix, 0, NULL);

  XSetLineAttributes (xdpy, gc, 1, LineSolid, CapRound, JoinRound);



  if (button->state == MBWMDecorButtonStateInactive)
    {
      XSetForeground (xdpy, gc, pixel_from_clr (xdpy, xscreen, &clr_bg));
    }
  else
    {
      /* FIXME -- think of a better way of doing this */
      MBWMColor clr;
      clr.r = clr_bg.r + 0.2;
      clr.g = clr_bg.g + 0.2;
      clr.b = clr_bg.b + 0.2;

      XSetForeground (xdpy, gc, pixel_from_clr (xdpy, xscreen, &clr));
    }

  XFillRectangle (xdpy, dd->xpix, gc, x, y, w+1, h+1);

  XSetLineAttributes (xdpy, gc, 3, LineSolid, CapRound, JoinRound);
  XSetForeground (xdpy, gc, pixel_from_clr (xdpy, xscreen, &clr_fg));

  if (button->type == HdHomeThemeButtonBack)
    {
      XSetLineAttributes (xdpy, gc, 3, LineSolid, CapRound, JoinMiter);
      XDrawLine (xdpy, dd->xpix, gc, x + w - 3, y + 3, x + 3, y + h/2);
      XDrawLine (xdpy, dd->xpix, gc, x + w - 3, y + h - 3, x + 3, y + h/2);
    }

  XFreeGC (xdpy, gc);

  XClearWindow (wm->xdpy, xwin);
}

static void
hd_theme_simple_paint_button (MBWMTheme *theme, MBWMDecorButton *button)
{
  MBWMThemeClass *parent_klass;

  if (!theme)
    return;

  if (button->type >= HdHomeThemeButtonBack)
    {
      hd_theme_simple_paint_back_button (theme, button);
      return;
    }

  parent_klass = MB_WM_THEME_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS (theme));

  if (parent_klass->paint_button)
    parent_klass->paint_button (theme, button);
}

static void
hd_theme_simple_get_button_size (MBWMTheme             *theme,
				 MBWMDecor             *decor,
				 MBWMDecorButtonType    type,
				 int                   *width,
				 int                   *height)
{
  MBWindowManagerClient * client = decor->parent_client;
  MBWMClientType  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMXmlClient * c;
  MBWMXmlDecor  * d;

  if (!theme)
    return;

  if (type >= HdHomeThemeButtonBack)
    {
      if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
	  (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type)))
	{
	  MBWMXmlButton * b = mb_wm_xml_button_find_by_type (d->buttons, type);

	  if (!b)
	    b = mb_wm_xml_button_find_by_type (d->buttons,
					       MBWMDecorButtonClose);
	  if (b)
	    {
	      if (width)
		*width = b->width;

	      if (height)
		*height = b->height;

	      return;
	    }
	}
    }
  else
    {
      MBWMThemeClass *parent_klass;

      parent_klass = MB_WM_THEME_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS (theme));

      if (parent_klass->button_size)
	parent_klass->button_size (theme, decor, type, width, height);
    }
}
