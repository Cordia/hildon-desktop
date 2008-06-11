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

static MBWMDecor * hd_theme_create_decor (MBWMTheme             *theme,
					  MBWindowManagerClient *client,
					  MBWMDecorType          type);

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

static void
button_press_handler (MBWindowManager   *wm,
		      MBWMDecorButton   *button,
		      void              *userdata)
{
  g_debug ("Back group button press.");
}

static void
button_release_handler (MBWindowManager   *wm,
			MBWMDecorButton   *button,
			void              *userdata)
{
  g_debug ("Back group button.");
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
	      button = mb_wm_decor_button_new (wm,
					       b->type,
					       b->packing,
					       decor,
					       button_press_handler,
					       button_release_handler,
					       0);
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
	  button = mb_wm_decor_button_new (wm,
					   HdHomeThemeButtonBack,
					   MBWMDecorButtonPackEnd,
					   decor,
					   button_press_handler,
					   button_release_handler,
					   0);
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
