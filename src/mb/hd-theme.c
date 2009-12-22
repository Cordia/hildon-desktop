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
#include "hd-decor-button.h"
#include "hd-clutter-cache.h"
#include "hd-render-manager.h"
#include "hd-theme-config.h"
#include "tidy/tidy-style.h"
#include "hd-home.h"
#include "hd-transition.h"

#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>
#include <X11/Xft/Xft.h>
#include <gtk/gtk.h>

#define BACK_BUTTON_TIMEOUT 2000

static MBWMDecor * hd_theme_create_decor (MBWMTheme             *theme,
					  MBWindowManagerClient *client,
					  MBWMDecorType          type);

static void
hd_theme_simple_get_button_size (MBWMTheme             *theme,
				 MBWMDecor             *decor,
				 MBWMDecorButtonType    type,
				 int                   *width,
				 int                   *height);

static void
hd_theme_paint_decor (MBWMTheme *theme, MBWMDecor *decor);
static void
hd_theme_paint_decor_button (MBWMTheme *theme, MBWMDecorButton *decor);

static void
hd_theme_get_title_xy (MBWMTheme *theme, int *x, int *y);

static void
hd_theme_class_init (MBWMObjectClass *klass)
{
  MBWMThemeClass *t_class = MB_WM_THEME_CLASS (klass);

  t_class->create_decor = hd_theme_create_decor;
  t_class->paint_decor  = hd_theme_paint_decor;
  t_class->paint_button = hd_theme_paint_decor_button;
  t_class->button_size  = hd_theme_simple_get_button_size;
  t_class->get_title_xy = hd_theme_get_title_xy;

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
  TidyStyle *style;
  ClutterColor col = { 255, 255, 255, 255 };
  GValue value;
  extern MBWindowManager *hd_mb_wm;

  hd_theme_config_get ();  

  hd_clutter_cache_theme_changed();
  /* transitions.ini could be loaded from the theme, so we must
   * reload it just in case. */
  hd_transition_set_file_changed();

  /* Update tidy-style (for scrollbars) */
  style = tidy_style_get_default();

  memset(&value, 0, sizeof(GValue));
  g_value_init(&value, CLUTTER_TYPE_COLOR);

  hd_theme_config_get_color (HD_2TXT_COLOR, &col);
  g_value_set_boxed (&value, &col);
  tidy_style_set_property(style, TIDY_ACTIVE_COLOR, &value);

  hd_theme_config_get_color (HD_BG_COLOR, &col);
  g_value_set_boxed (&value, &col);
  tidy_style_set_property(style, TIDY_BACKGROUND_COLOR, &value);

  g_value_unset (&value);

  /* Update home theme */
  if (hd_mb_wm && hd_mb_wm->comp_mgr)
    {
      ClutterActor *home = hd_comp_mgr_get_home (HD_COMP_MGR (hd_mb_wm->comp_mgr));

      if (home)
        hd_home_theme_changed (HD_HOME (home));
    }

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

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_THEME, 0);
    }

  return type;
}

typedef struct _BackButtonData
{
  MBWMDecorButton *button;

  guint            timeout_id;
  gboolean         timeout_handled : 1;
} BackButtonData;

static gboolean
back_button_timeout (gpointer data)
{
  BackButtonData        *bd = data;
  MBWindowManagerClient *c = NULL;

  bd->timeout_handled = TRUE;
  bd->timeout_id = 0;

  if (!bd->button)
    return FALSE;

  /*
   * The button might be unrealized while we were waiting for the timeout.
   */
  if (!bd->button->realized)
    goto finalize;
  /*
   * We protect ourselves against the non-app windows.
   * NOTE: we don't have reference for decor or parent_client so this
   * relies on setting the parent_client NULL when it's destroyed.
   */
  if (bd->button->decor && (c = bd->button->decor->parent_client) &&
      !HD_IS_APP (c))
    {
      g_warning ("Custom button on a something other than App.");
      goto finalize;
    }

  /*
   * We have to check if the button is still pressed. The user might released
   * the stylus outside the button.
   */
  if (c && bd->button->state == MBWMDecorButtonStatePressed)
    hd_app_close_followers (HD_APP(c));

finalize:
  mb_wm_object_unref (MB_WM_OBJECT(bd->button));
  return FALSE;
}

static void
back_button_data_free (BackButtonData *bd)
{
  g_free (bd);
}

static void
back_button_data_destroy (MBWMDecorButton *button, void *data)
{
  BackButtonData        *bd = data;

  /* Kill our timeout if we had one, but we need to
   * call it anyway */
  if (bd->timeout_id)
    {
      g_source_remove( bd->timeout_id );
      bd->timeout_id = 0;
      /* the button is unreferenced here */
      back_button_timeout(data);
    }

  back_button_data_free (bd);
}

static void
back_button_press_handler (MBWindowManager   *wm,
			   MBWMDecorButton   *button,
			   void              *userdata)
{
  BackButtonData *bd = userdata;

  /*
   * The user might have released outside the back button and pressed
   * the button again.
   */
  if (bd->timeout_id != 0)
    g_source_remove (bd->timeout_id);
  else
    /* reference for the timeout handler */
    mb_wm_object_ref (MB_WM_OBJECT(bd->button));

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
  if (c)
    mb_wm_client_deliver_delete (c);

  /* the button was referenced when timeout was installed */
  mb_wm_object_unref (MB_WM_OBJECT(button));
}

static void
construct_buttons (MBWMTheme *theme, HdDecor *decor, MBWMXmlDecor *d)
{
  MBWindowManagerClient *client = MB_WM_DECOR(decor)->parent_client;
  MBWindowManager       *wm     = client->wmref;
  HdDecorButton         *button = NULL;
  int                    stack_i = -1;
  HdApp                 *app = NULL;

  if (MB_WM_CLIENT_CLIENT_TYPE (client) == MBWMClientTypeApp)
    {
      app = HD_APP (client);
      stack_i = app->stack_index;
    }

  if (d && app)
    {
      MBWMList * l = d->buttons;

      while (l)
	{
	  MBWMXmlButton * b = l->data;

	  /* Back button only for group followers */
	  if (b->type == HdHomeThemeButtonBack && stack_i > 0
	      && app && app->leader != app)
	    {
	      BackButtonData *bd;

	      button = hd_decor_button_new(wm,
                                           b->type,
                                           b->packing,
                                           decor,
                                           back_button_press_handler,
                                           back_button_release_handler,
                                           0);

	      bd = g_new0 (BackButtonData, 1);
	      bd->button = MB_WM_DECOR_BUTTON(button);

	      mb_wm_decor_button_set_user_data (MB_WM_DECOR_BUTTON(button), bd,
						back_button_data_destroy);
	    }
	  /* No close button for group followers */
	  else if (b->type == MBWMDecorButtonClose &&
		   (stack_i < 0 || app->leader == app))
	    {
	      button = hd_decor_button_new(wm,
                                           b->type,
                                           b->packing,
                                           decor,
                                           0,0,
                                           0);
	    }
	  else if (b->type != HdHomeThemeButtonBack &&
	           b->type != MBWMDecorButtonClose)
	    {
              /* do not install press/release handler */
	      button = hd_decor_button_new(wm,
                                           b->type,
                                           b->packing,
                                           decor,
                                           0,0,
                                           MB_WM_DECOR_BUTTON_NOHANDLERS);
	    }
	  else
	    {
	      button = NULL;
	    }

	  /*
	   * Consider throwing an error here
	   * if the button has w/h of 0x0
	   */

	  if (button)
	    {
	      mb_wm_decor_button_show (MB_WM_DECOR_BUTTON (button));
	      mb_wm_object_unref (MB_WM_OBJECT (button));
	    }

	  l = l->next;
	}
    }
  else if (!d)
    {
      if (stack_i < 0 || (app && app->leader == app))
	{
          /* non-stackable or stack leader */
	  button = hd_decor_button_new(wm,
                                       MBWMDecorButtonClose,
                                       MBWMDecorButtonPackEnd,
                                       decor,
                                       0,0,
                                       0);
	}
      else  /* stack secondary */
	{
	  BackButtonData *bd;

	  button = hd_decor_button_new(wm,
                                       HdHomeThemeButtonBack,
                                       MBWMDecorButtonPackEnd,
                                       decor,
                                       back_button_press_handler,
                                       back_button_release_handler,
                                       0);

	  bd = g_new0 (BackButtonData, 1);

	  mb_wm_decor_button_set_user_data (MB_WM_DECOR_BUTTON(button), bd,
					    back_button_data_destroy);
	}

      mb_wm_decor_button_show (MB_WM_DECOR_BUTTON (button));
      mb_wm_object_unref (MB_WM_OBJECT (button));
    }
}

static MBWMDecor *
hd_theme_create_decor (MBWMTheme             *theme,
		       MBWindowManagerClient *client,
		       MBWMDecorType          type)
{
  MBWMClientType   c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  HdDecor         *decor = NULL;
  MBWindowManager *wm = client->wmref;
  MBWMXmlClient   *c;

  if (client->window->undecorated)
    return NULL;

  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)))
    {
      MBWMXmlDecor *d;

      d = mb_wm_xml_decor_find_by_type (c->decors, type);

      if (d)
	{
	  decor = hd_decor_new (wm, type);

	  mb_wm_decor_attach (MB_WM_DECOR(decor), client);
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
	      decor = hd_decor_new (wm, type);
	      mb_wm_decor_attach (MB_WM_DECOR(decor), client);
	      construct_buttons (theme, decor, NULL);
	      break;
	    default:
	      decor = hd_decor_new (wm, type);
	      mb_wm_decor_attach (MB_WM_DECOR(decor), client);
	    }
	  break;

	case MBWMClientTypeDialog:
	  decor = hd_decor_new (wm, type);
	  mb_wm_decor_attach (MB_WM_DECOR(decor), client);
	  break;

	case MBWMClientTypePanel:
	case MBWMClientTypeDesktop:
	case MBWMClientTypeInput:
	default:
	  decor = hd_decor_new (wm, type);
	  mb_wm_decor_attach (MB_WM_DECOR(decor), client);
	}
    }

  return MB_WM_DECOR(decor);
}

static void
hd_theme_simple_class_init (MBWMObjectClass *klass)
{
  MBWMThemeClass *t_class = MB_WM_THEME_CLASS (klass);

  t_class->create_decor = hd_theme_create_decor;
  t_class->paint_button = 0;
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

static void
hd_theme_paint_decor (MBWMTheme *theme, MBWMDecor *decor)
{
  hd_decor_sync( HD_DECOR (decor) );
}

static void
hd_theme_paint_decor_button (MBWMTheme *theme, MBWMDecorButton *decor)
{
  hd_decor_button_sync( HD_DECOR_BUTTON(decor) );
}

static void
hd_theme_get_title_xy (MBWMTheme *theme, int *x, int *y)
{
  hd_render_manager_get_title_xy (x, y);
}
