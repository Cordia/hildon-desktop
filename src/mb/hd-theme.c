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
#include "hd-decor.h"

#include <matchbox/theme-engines/mb-wm-theme-png.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>
#include <X11/Xft/Xft.h>
#include <gtk/gtk.h>

#define BACK_BUTTON_TIMEOUT 2000

static MBWMDecor * hd_theme_create_decor (MBWMTheme             *theme,
					  MBWindowManagerClient *client,
					  MBWMDecorType          type);

static void hd_theme_simple_paint_button (MBWMTheme *theme,
					  MBWMDecorButton *button);

static void hd_theme_paint_decor (MBWMTheme *theme, MBWMDecor *decor);

static void
hd_theme_simple_get_button_size (MBWMTheme             *theme,
				 MBWMDecor             *decor,
				 MBWMDecorButtonType    type,
				 int                   *width,
				 int                   *height);

static void
hd_theme_paint_decor (MBWMTheme *theme, MBWMDecor *decor);

static void
hd_theme_class_init (MBWMObjectClass *klass)
{
  MBWMThemeClass *t_class = MB_WM_THEME_CLASS (klass);

  t_class->create_decor = hd_theme_create_decor;
  t_class->paint_decor = hd_theme_paint_decor;

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

  c = bd->button->decor->parent_client;

  bd->timeout_handled = TRUE;
  bd->timeout_id = 0;
  /*
   * The button might be unrealized while we were waiting for the timeout.
   */
  if (!bd->button->realized)
    goto finalize;
  /*
   * We protect ourselves against the non-app windows.
   */
  if (!HD_IS_APP (c))
    {
      g_warning ("Custom button on a something other than App.");
      goto finalize;
    }

  /*
   * We have to check if the button is still pressed. The user might released
   * the stylus outside the button.
   */
  if (bd->button->state == MBWMDecorButtonStatePressed)
    hd_app_close_followers (HD_APP(c));

finalize:
  mb_wm_object_unref (MB_WM_OBJECT(bd->button));
  return FALSE;
}

static void
back_button_press_handler (MBWindowManager   *wm,
			   MBWMDecorButton   *button,
			   void              *userdata)
{
  BackButtonData *bd = userdata;

  mb_wm_object_ref (MB_WM_OBJECT(button));
  /*
   * The user might released outside the back button and pressed the button
   * again.
   */
  if (bd->timeout_id != 0) {
    /*
     * We unref the button on behalf the timeout function.
     */
    g_source_remove (bd->timeout_id);
    mb_wm_object_unref (MB_WM_OBJECT(bd->button));
  }

  bd->timeout_id =
    g_timeout_add_full (G_PRIORITY_HIGH_IDLE,
			BACK_BUTTON_TIMEOUT, back_button_timeout, bd, NULL);

  bd->timeout_handled = FALSE;
}

/*
 * Called when the pointer button is released over the back button. If the user
 * moves the pointer outside the back button this function will not be called.
 */
static void
back_button_release_handler (MBWindowManager   *wm,
			     MBWMDecorButton   *button,
			     void              *userdata)
{
  BackButtonData        *bd = userdata;
  MBWindowManagerClient *c;

  if (!bd || bd->timeout_handled)
    return;

  g_source_remove (bd->timeout_id);
  bd->timeout_id = 0;

  c = button->decor->parent_client;
  mb_wm_client_deliver_delete (c);

  mb_wm_object_unref (MB_WM_OBJECT(button));
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
      g_debug("%s: theme", __FUNCTION__);
      while (l)
	{
	  MBWMXmlButton * b = l->data;

	  /* Back button only for group followers */
	  if (b->type == HdHomeThemeButtonBack && !is_leader &&
	      MB_WM_CLIENT_CLIENT_TYPE (client) == MBWMClientTypeApp)
	    {
	      BackButtonData *bd;

              g_debug("%s: back button", __FUNCTION__);
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
	  else if (b->type == MBWMDecorButtonClose && is_leader)
	    {
              g_debug("%s: close button", __FUNCTION__);
	      button = mb_wm_decor_button_stock_new (wm,
						     b->type,
						     b->packing,
						     decor,
						     0);
	    }
	  else if (b->type != HdHomeThemeButtonBack &&
	      b->type != MBWMDecorButtonClose)
	    {
              g_debug("%s: other button", __FUNCTION__);
              /* do not install press/release handler */
	      button = mb_wm_decor_button_stock_new (wm,
						     b->type,
						     b->packing,
						     decor,
	                        MB_WM_DECOR_BUTTON_NOHANDLERS);
	    }
	  else
	    {
              g_debug("%s: no button", __FUNCTION__);
	      button = NULL;
	    }

	  /*
	   * Consider throwing an error here
	   * if the button has w/h of 0x0
	   */

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
      g_debug("%s: no theme, only close/back button", __FUNCTION__);
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
	  decor = MB_WM_DECOR (hd_decor_new (wm, type));

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
 * Copied from mb-wm-theme-png.c; must be kept in sync (this is for debuggin
 * only anyway).
 */
#include <X11/Xft/Xft.h>
struct DecorData
{
  Pixmap    xpix;
  Pixmap    shape_mask;
  GC        gc_mask;
  XftDraw  *xftdraw;
  XftColor  clr;
#if USE_PANGO
  PangoFont *font;
#else
  XftFont  *font;
#endif
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

#if !USE_PANGO
static XftFont *
xft_load_font(MBWMDecor * decor, MBWMXmlDecor *d)
{
  char desc[512];
  XftFont *font;
  Display * xdpy = decor->parent_client->wmref->xdpy;
  int       xscreen = decor->parent_client->wmref->xscreen;
  int       font_size = d->font_size ? d->font_size : 18;

  if (d->font_units == MBWMXmlFontUnitsPixels)
    {
      font_size = mb_wm_util_pixels_to_points (decor->parent_client->wmref,
					       font_size);
    }

  snprintf (desc, sizeof (desc), "%s-%i",
	    d->font_family ? d->font_family : "Sans",
	    font_size);

  font = XftFontOpenName (xdpy, xscreen, desc);

  return font;
}
#endif

static void
decordata_free (MBWMDecor * decor, void *data)
{
  struct DecorData * dd = data;
  Display * xdpy = decor->parent_client->wmref->xdpy;

  XFreePixmap (xdpy, dd->xpix);

  if (dd->shape_mask)
    XFreePixmap (xdpy, dd->shape_mask);

  if (dd->gc_mask)
    XFreeGC (xdpy, dd->gc_mask);

  XftDrawDestroy (dd->xftdraw);

#if USE_PANGO
  if (dd->font)
    g_object_unref (dd->font);
#else
  if (dd->font)
    XftFontClose (xdpy, dd->font);
#endif

  free (dd);
}

static gboolean
window_is_waiting (MBWindowManager *wm, Window w)
{
  HdCompMgr *hmgr = HD_COMP_MGR (wm->comp_mgr);
  Atom progress_indicator = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_PROGRESS_INDICATOR);
  Atom actual_type_return;
  int actual_format_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  unsigned char* prop_return = NULL;

  XGetWindowProperty (wm->xdpy, w,
		      progress_indicator,
		      0, G_MAXLONG,
		      False,
		      AnyPropertyType,
		      &actual_type_return,
		      &actual_format_return,
		      &nitems_return,
		      &bytes_after_return,
		      &prop_return);

  if (prop_return)
    {
      int result = prop_return[0];

      XFree (prop_return);

      return result;
    }
  else
    return 0;
}

struct ProgressIndicatorData
{
   gint x_position;
   Display *xdpy;
   Picture source;
   Drawable dest;
   gint8 wait_cycle;
};

static gboolean
progress_indicator_cb (gpointer data)
{
  HdDecor *decor = data;
  ProgressIndicatorData *pro;
  XWindowAttributes attr;
  XRenderPictFormat *format;
  XRenderPictureAttributes pa;
  const int pages_in_cycle = 8;
  const int page_width = 56;
  const int page_height = 53;
  const int source_x = 0;
  const int source_y = 222;

  if (!HD_IS_DECOR(data))
    return FALSE;

  pro = decor->progress;

  XGetWindowAttributes( pro->xdpy, pro->dest, &attr );
  format = XRenderFindVisualFormat( pro->xdpy, attr.visual );
  pa.subwindow_mode = IncludeInferiors;

  Picture target = XRenderCreatePicture (pro->xdpy, pro->dest, format,
		  CPSubwindowMode, &pa );

  XRenderComposite (pro->xdpy, PictOpOver,
                    pro->source, None,
		    target,
		    source_x + page_width*pro->wait_cycle, source_y,
		    0, 0,
		    pro->x_position, 0, /* always drawn at the top */
		    page_width, page_height);
  XSync(pro->xdpy, FALSE);
  pro->wait_cycle = (pro->wait_cycle+1) % pages_in_cycle;

  return TRUE; /* we always want to go round again */
}

static void
hd_theme_paint_decor (MBWMTheme *theme, MBWMDecor *decor)
{
  HdDecor                * hd_decor = HD_DECOR (decor);
  MBWMThemePng           * p_theme = MB_WM_THEME_PNG (theme);
  MBWindowManagerClient  * client = decor->parent_client;
  MBWMClientType           c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMXmlClient          * c;
  MBWMXmlDecor           * d;
  Display		 * xdpy    = theme->wm->xdpy;
  int			   xscreen = theme->wm->xscreen;
  struct DecorData	 * data = mb_wm_decor_get_theme_data (decor);
  const char		 * title;
  int			   x, y;
  int			   operator = PictOpSrc;
  int                      titlebar_width;
  int                      pack_end_x = mb_wm_decor_get_pack_end_x (decor);
  unsigned int             l_padding = left_padding - 8; /* fudge, not sure why it's necessary */

  if (!((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
        (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type))))
    return;

  titlebar_width = decor->geom.width - l_padding;

#if 0
  /* Uncomment this to make the titlebar make room for the buttons on the right */
  titlebar_width -= (decor->geom.width - pack_end_x);
#endif

#ifdef HAVE_XEXT
  shaped = theme->shaped && c->shaped && !mb_wm_client_is_argb32 (client);
#endif

  if (data && (mb_wm_decor_get_dirty_state (decor) & MBWMDecorDirtyTitle))
    {
      /*
       * If the decor title is dirty, and we already have the data,
       * free it and recreate (since the old title is already composited
       * in the cached image).
       */
      mb_wm_decor_set_theme_data (decor, NULL, NULL);
      data = NULL;
    }

  if (!data)
    {
      XRenderColor rclr;

      data = mb_wm_util_malloc0 (sizeof (struct DecorData));
      data->xpix = XCreatePixmap(xdpy, decor->xwin,
				 decor->geom.width, decor->geom.height,
				 DefaultDepth(xdpy, xscreen));


#ifdef HAVE_XEXT
      if (shaped)
	{
	  data->shape_mask =
	    XCreatePixmap(xdpy, decor->xwin,
			  decor->geom.width, decor->geom.height, 1);

	  data->gc_mask = XCreateGC (xdpy, data->shape_mask, 0, NULL);
	}
#endif
      data->xftdraw = XftDrawCreate (xdpy, data->xpix,
				     DefaultVisual (xdpy, xscreen),
				     DefaultColormap (xdpy, xscreen));

      /*
       * If the background color is set, we fill the pixmaps with it,
       * and then overlay the the PNG image over (this allows a theme
       * to provide a monochromatic PNG that can be toned, e.g., Sato)
       */
      if (d->clr_bg.set)
	{
	  XRenderColor rclr2;

	  operator = PictOpOver;

	  rclr2.red   = (int)(d->clr_bg.r * (double)0xffff);
	  rclr2.green = (int)(d->clr_bg.g * (double)0xffff);
	  rclr2.blue  = (int)(d->clr_bg.b * (double)0xffff);

	  XRenderFillRectangle (xdpy, PictOpSrc,
				XftDrawPicture (data->xftdraw), &rclr2,
				0, 0,
				decor->geom.width, decor->geom.height);
	}

      rclr.red = 0;
      rclr.green = 0;
      rclr.blue  = 0;
      rclr.alpha = 0xffff;

      if (d->clr_fg.set)
	{
	  rclr.red   = (int)(d->clr_fg.r * (double)0xffff);
	  rclr.green = (int)(d->clr_fg.g * (double)0xffff);
	  rclr.blue  = (int)(d->clr_fg.b * (double)0xffff);
	}

      XftColorAllocValue (xdpy, DefaultVisual (xdpy, xscreen),
			  DefaultColormap (xdpy, xscreen),
			  &rclr, &data->clr);

#if USE_PANGO
      {
	PangoFontDescription * pdesc;
	char desc[512];

	snprintf (desc, sizeof (desc), "%s %i%s",
		  d->font_family ? d->font_family : "Sans",
		  d->font_size ? d->font_size : 18,
		  d->font_units == MBWMXmlFontUnitsPoints ? "" : "px");

	pdesc = pango_font_description_from_string (desc);

	data->font = pango_font_map_load_font (p_theme->fontmap,
					       p_theme->context,
					       pdesc);

	pango_font_description_free (pdesc);
      }
#else
      data->font = xft_load_font (decor, d);
#endif
      XSetWindowBackgroundPixmap(xdpy, decor->xwin, data->xpix);

      mb_wm_decor_set_theme_data (decor, data, decordata_free);
    }

  /*
   * Since we want to support things like rounded corners, but still
   * have the decor resizable, we need to paint it in stages
   *
   * We assume that the decor image is exact in it's major axis,
   * i.e., North and South decors provide image of the exactly correct
   * height, and West and East of width.
   */
  if (decor->type == MBWMDecorTypeNorth ||
      decor->type == MBWMDecorTypeSouth)
    {
      if (titlebar_width < d->width)
	{
	  /* The decor is smaller than the template, cut bit from the
	   * midle
	   */
	  int width1 = titlebar_width / 2;
	  int width2 = titlebar_width - width1;
	  int x2     = d->x + d->width - width2;

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0, l_padding, 0,
			   width1, d->height);

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   x2 , d->y, 0, 0,
			   l_padding+width1, 0,
			   width2, d->height);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y, width1, d->height, 0, 0);
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 x2, d->y, width2, d->height, width1, 0);
	    }
#endif
	}
      else if (titlebar_width == d->width)
	{
	  /* Exact match */
	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0,
			   0, 0, d->width, d->height);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y, d->width, d->height, 0, 0);
	    }
#endif
	}
      else
	{
	  /* The decor is bigger than the template, draw extra bit from
	   * the middle
	   */
	  int pad_offset = d->pad_offset;
	  int pad_length = d->pad_length;
	  int gap_length = titlebar_width - d->width;

	  if (!pad_length)
	    {
	      pad_length =
		titlebar_width > 30 ? 10 : titlebar_width / 4 + 1;
	      pad_offset = (d->width / 2) - (pad_length / 2);
	    }

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0,
			   0, 0,
			   pad_offset, d->height);

	  /* TODO: can we do this as one scaled operation? */
	  for (x = pad_offset; x < pad_offset + gap_length; x += pad_length)
	    XRenderComposite(xdpy, operator,
			     p_theme->xpic,
			     None,
			     XftDrawPicture (data->xftdraw),
			     d->x + pad_offset, d->y, 0, 0,
			     x, 0,
			     pad_length,
			     d->height);

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x + pad_offset, d->y, 0, 0,
			   pad_offset + gap_length, 0,
			   d->width - pad_offset, d->height);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y,
			 pad_offset, d->height,
			 0, 0);

	      for (x = pad_offset; x < pad_offset + gap_length; x += pad_length)
		XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			   data->gc_mask,
			   d->x + pad_offset, d->y,
			   d->width - pad_offset, d->height,
			   x, 0);

	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x + pad_offset, d->y,
			 d->width - pad_offset, d->height,
			 pad_offset + gap_length, 0);
	    }
#endif
	}
    }
  else
    {
      if (decor->geom.height < d->height)
	{
	  /* The decor is smaller than the template, cut bit from the
	   * midle
	   */
	  int height1 = decor->geom.height / 2;
	  int height2 = decor->geom.height - height1;
	  int y2      = d->y + d->height - height2;

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0,
			   l_padding, 0,
			   d->width, height1);

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x , y2, 0, 0,
			   l_padding, height1,
			   d->width, height2);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y, d->width, height1, l_padding, 0);
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, y2, d->width, height2, l_padding, height1);
	    }
#endif
	}
      else if (decor->geom.height == d->height)
	{
	  /* Exact match */
	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0,
			   l_padding, 0,
			   d->width, d->height);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y, d->width, d->height, l_padding, 0);
	    }
#endif
	}
      else
	{
	  /* The decor is bigger than the template, draw extra bit from
	   * the middle
	   */
	  int pad_offset = d->pad_offset;
	  int pad_length = d->pad_length;
	  int gap_length = decor->geom.height - d->height;

	  if (!pad_length)
	    {
	      pad_length =
		decor->geom.height > 30 ? 10 : decor->geom.height / 4 + 1;
	      pad_offset = (d->height / 2) - (pad_length / 2);
	    }

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0, l_padding, 0,
			   d->width, pad_offset);

	  /* TODO: can we do this as one scaled operation? */
	  for (y = pad_offset; y < pad_offset + gap_length; y += pad_length)
	    XRenderComposite(xdpy, operator,
			     p_theme->xpic,
			     None,
			     XftDrawPicture (data->xftdraw),
			     d->x, d->y + pad_offset, 0, 0, l_padding, y,
			     d->width,
			     pad_length);

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x , d->y + pad_offset, 0, 0,
			   l_padding, pad_offset + gap_length,
			   d->width, d->height - pad_offset);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y,
			 d->width, pad_offset,
			 l_padding, 0);

	      for (y = pad_offset; y < pad_offset + gap_length; y += pad_length)
		XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			   data->gc_mask,
			   d->x, d->y + pad_offset,
			   d->width, pad_length,
			   l_padding, y);

	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y + pad_offset,
			 d->width, d->height - pad_offset,
			 l_padding, pad_offset + gap_length);
	    }
#endif
	}
    }

  if (d->show_title &&
      (title = mb_wm_client_get_name (client)) &&
      data->font)
    {
      XRectangle rec;

      int west_width = mb_wm_client_frame_west_width (client);
      int y, ascent, descent;
      int len = strlen (title);
      int is_secondary_dialog;
      int centering_padding = 0;
      gboolean is_waiting_window = window_is_waiting (theme->wm, client->window->xwindow);
#if USE_PANGO
      PangoRectangle font_extents;
#else
      XGlyphInfo font_extents;
#endif

      HdApp *this_app = HD_APP (client);

      is_secondary_dialog = this_app->secondary_window;

#if USE_PANGO
      PangoFontMetrics * mtx;
      PangoGlyphString * glyphs;
      GList            * items, *l;
      PangoRectangle     rect;
      int                xoff = 0;

      mtx = pango_font_get_metrics (data->font, NULL);

      ascent  = PANGO_PIXELS (pango_font_metrics_get_ascent (mtx));
      descent = PANGO_PIXELS (pango_font_metrics_get_descent (mtx));

      pango_font_metrics_unref (mtx);
#else
      ascent  = data->font->ascent;
      descent = data->font->descent;
#endif

      y = (decor->geom.height - (ascent + descent)) / 2 + ascent;

      rec.x = l_padding;
      rec.y = 0;
      rec.width = pack_end_x - l_padding;
      rec.height = d->height;

      XftDrawSetClipRectangles (data->xftdraw, 0, 0, &rec, 1);

#if USE_PANGO
      glyphs = pango_glyph_string_new ();

      /*
       * Run the pango rendering pipeline on this text and draw with
       * the xft backend (why Pango does not provide a convenience
       * API for something as common as drawing a string escapes me).
       */
      items = pango_itemize (p_theme->context, title, 0, len, NULL, NULL);

      l = items;
      while (l)
	{
	  PangoItem * item = l->data;

	  item->analysis.font = data->font;

	  pango_shape (title, len, &item->analysis, glyphs);

	  if (is_secondary_dialog || is_waiting_window)
	    {
	      pango_glyph_string_extents (glyphs,
					  data->font,
					  NULL,
					  &font_extents);
	      pango_extents_to_pixels (NULL, &font_extents);

	      if (is_secondary_dialog)
                centering_padding = (rec.width - font_extents.width) / 2;

	    }

	  pango_xft_render (data->xftdraw,
			    &data->clr,
			    data->font,
			    glyphs,
			    xoff + west_width + centering_padding? centering_padding: l_padding,
			    y);

	  /* Advance position */
	  pango_glyph_string_extents (glyphs, data->font, NULL, &rect);
	  xoff += PANGO_PIXELS (rect.width);

	  l = l->next;
	}

      if (glyphs)
	pango_glyph_string_free (glyphs);

      g_list_free (items);
#else

      if (is_secondary_dialog || is_waiting_window)
	{

	  XftTextExtentsUtf8 (xdpy,
			  data->font,
			  (const guchar*)title, len,
			  &font_extents);

	  if (is_secondary_dialog)
	    centering_padding = (rec.width - font_extents.width) / 2;

	}

      XftDrawStringUtf8(data->xftdraw,
			&data->clr,
			data->font,
			centering_padding?
			west_width + centering_padding:
			west_width + l_padding,
			y,
			(const guchar*)title, len);
#endif

      if (is_waiting_window)
        {
	   if (hd_decor->progress==NULL)
	     {
               hd_decor->progress = g_malloc (sizeof (ProgressIndicatorData));

	       hd_decor->progress->x_position = west_width + centering_padding? centering_padding: l_padding + font_extents.width;
	       hd_decor->progress->xdpy = xdpy;
	       hd_decor->progress->source = p_theme->xpic;
	       hd_decor->progress->dest = decor->xwin;
	       hd_decor->progress->wait_cycle = 0;
	       g_timeout_add (100,
			      progress_indicator_cb,
			      hd_decor);
	    }
        }
      else
        {
          g_source_remove_by_user_data (hd_decor);
	  hd_decor->progress = NULL;
	}

      /* Unset the clipping rectangle */
      rec.width = decor->geom.width;
      rec.height = decor->geom.height;

      XftDrawSetClipRectangles (data->xftdraw, 0, 0, &rec, 1);
    }

#ifdef HAVE_XEXT
  if (shaped)
    {
      XShapeCombineMask (xdpy, decor->xwin,
			 ShapeBounding, 0, 0,
			 data->shape_mask, ShapeSet);

      XShapeCombineShape (xdpy,
			  client->xwin_frame,
			  ShapeBounding, decor->geom.x, decor->geom.y,
			  decor->xwin,
			  ShapeBounding, ShapeUnion);
    }
#endif
  XClearWindow (xdpy, decor->xwin);
}
