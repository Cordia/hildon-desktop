/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author: Gordon Williams <gordon.williams@collabora.co.uk>
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

#include "hd-decor.h"
#include "hd-decor-button.h"
#include "hd-render-manager.h"
#include "hd-title-bar.h"
#include "hd-clutter-cache.h"
#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-wm-util.h>
#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>

static void
hd_decor_button_class_init (MBWMObjectClass *klass)
{

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdDecorButton";
#endif
}

static void
hd_decor_button_destroy (MBWMObject *obj)
{
  HdDecorButton *button = HD_DECOR_BUTTON(obj);

  hd_decor_button_remove_actors(button);
}

static int
hd_decor_button_init (MBWMObject *obj, va_list vap)
{
  HdDecorButton *d = HD_DECOR_BUTTON (obj);
  MBWMObjectProp               prop;

  d->active = 0;
  d->inactive = 0;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
        {
        case MBWMObjectPropWm:
          d->wm = va_arg(vap, MBWindowManager *);
          break;
        default:
          MBWMO_PROP_EAT (vap, prop);
        }
      prop = va_arg(vap, MBWMObjectProp);
    }

  if (!d->wm)
    return 0;

  return 1;
}

HdDecorButton* hd_decor_button_new (MBWindowManager               *wm,
                                    MBWMDecorButtonType            type,
                                    MBWMDecorButtonPack            pack,
                                    HdDecor                       *decor,
                                    MBWMDecorButtonPressedFunc     press,
                                    MBWMDecorButtonReleasedFunc    release,
                                    MBWMDecorButtonFlags           flags)
{
  MBWMObject *decorbutton;

  decorbutton = mb_wm_object_new (HD_TYPE_DECOR_BUTTON,
                            MBWMObjectPropWm,                      wm,
                            MBWMObjectPropDecorButtonType,         type,
                            MBWMObjectPropDecorButtonPack,         pack,
                            MBWMObjectPropDecor,                   decor,
                            MBWMObjectPropDecorButtonPressedFunc,  press,
                            MBWMObjectPropDecorButtonReleasedFunc, release,
                            MBWMObjectPropDecorButtonFlags,        flags,
                            NULL);

  return HD_DECOR_BUTTON(decorbutton);
}

int
hd_decor_button_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdDecorButtonClass),
	sizeof (HdDecorButton),
	hd_decor_button_init,
	hd_decor_button_destroy,
	hd_decor_button_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_DECOR_BUTTON, 0);
    }

  return type;
}

void
hd_decor_button_sync_actors(HdDecorButton *button)
{
  MBWMDecorButton *mbbutton = MB_WM_DECOR_BUTTON(button);
  gboolean vis_active=FALSE, vis_inactive=FALSE;
  gboolean visible = TRUE;

  if (!button->active || !button->inactive)
    hd_decor_button_create_actors(button);

  /* Check for a fullscreen app (if so, we don't draw the buttons)*/
  if (mb_wm_client_window_is_state_set (
      mbbutton->decor->parent_client->window,
      MBWMClientWindowEWMHStateFullscreen))
    visible = FALSE;

  /* if this failed then just return */
  if (!button->active || !button->inactive)
    return;
  /* set positions */
  clutter_actor_set_position(button->active,
      mbbutton->geom.x, mbbutton->geom.y);
  clutter_actor_set_position(button->inactive,
      mbbutton->geom.x, mbbutton->geom.y);
  /* set visibility */
  if (mbbutton->visible && visible)
    {
      if (mbbutton->state == MBWMDecorButtonStateInactive)
        vis_inactive = TRUE;
      else
        vis_active = TRUE;
    }

  if (vis_active)
    {
      clutter_actor_show(button->active);
      clutter_actor_raise_top(button->active);
    }
  else
    {
      clutter_actor_hide(button->active);
      /* for some reason hide doesn't seem to work? so lowering... */
      clutter_actor_lower_bottom(button->active);
    }
  if (vis_inactive)
    {
      clutter_actor_show(button->inactive);
      clutter_actor_raise_top(button->inactive);
    }
  else
    {
      clutter_actor_hide(button->inactive);
      /* for some reason hide doesn't seem to work? so lowering... */
      clutter_actor_lower_bottom(button->inactive);
    }
}

void
hd_decor_button_remove_actors(HdDecorButton *button)
{
  if (button->active)
    {
      clutter_actor_destroy(button->active);
      button->active = 0;
    }
  if (button->inactive)
    {
      clutter_actor_destroy(button->inactive);
      button->inactive = 0;
    }
}

void
hd_decor_button_create_actors(HdDecorButton *button)
{
  MBWMTheme         *theme;
  MBWMXmlClient     *c;
  MBWMXmlDecor      *d;
  MBWMXmlButton     *b = 0;
  MBWMList          *b_it;
  MBWMDecorButton *mbbutton = MB_WM_DECOR_BUTTON(button);
  MBWindowManagerClient  *client = mbbutton->decor->parent_client;
  MBWMClientType          c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  ClutterGeometry geo_active, geo_inactive;
  ClutterActor *actor;
  HdTitleBar *bar;
  char buffer[64];
  char *name_active, *name_inactive;

  bar = HD_TITLE_BAR(hd_render_manager_get_title_bar());
  actor = hd_decor_get_actor(HD_DECOR(mbbutton->decor));
  if (!actor)
    return;

  if (!((theme = button->wm->theme) &&
        (c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
        (d = mb_wm_xml_decor_find_by_type (c->decors, mbbutton->decor->type))))
    return;

  /* find our button */
  for (b_it = d->buttons; b_it; b_it = b_it->next)
    {
      b = (MBWMXmlButton*)(b_it->data);
      if (b->type == mbbutton->type)
        break;
    }

  if (!b)
    return;

  geo_active.width = geo_inactive.width = b->width;
  geo_active.height = geo_inactive.height = b->height;
  geo_active.x = b->active_x;
  geo_active.y = b->active_y;
  geo_inactive.x = b->inactive_x;
  geo_inactive.y = b->inactive_y;
  button->active =
      CLUTTER_ACTOR(hd_clutter_cache_get_sub_texture(
          theme->image_filename, &geo_active));
  button->inactive =
      CLUTTER_ACTOR(hd_clutter_cache_get_sub_texture(
          theme->image_filename, &geo_inactive));

  sprintf(buffer,"%d_active", mbbutton->type);
  name_active = g_strdup(buffer);
  sprintf(buffer,"%d_inactive", mbbutton->type);
  name_inactive = g_strdup(buffer);

  clutter_actor_set_name(button->active, name_active);
  clutter_actor_set_name(button->inactive, name_inactive);

  clutter_container_add_actor(CLUTTER_CONTAINER(actor), button->active);
  clutter_container_add_actor(CLUTTER_CONTAINER(actor), button->inactive);
}
