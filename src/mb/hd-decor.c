/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author: Thomas Thurman <thomas.thurman@collabora.co.uk>
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
#include "hd-comp-mgr.h"
#include "hd-app.h"

#include "hd-render-manager.h"
#include "hd-title-bar.h"
#include "hd-clutter-cache.h"
#include "hd-decor-button.h"

#include <matchbox/theme-engines/mb-wm-theme-png.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>

#define HD_DECOR_TITLE_MARGIN 24


static void
hd_decor_remove_actors(HdDecor   *decor);

static void
hd_decor_class_init (MBWMObjectClass *klass)
{
  /* MBWMDecorClass *d_class = MB_WM_DECOR_CLASS (klass); */

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdDecor";
#endif
}

static void
hd_decor_destroy (MBWMObject *obj)
{
  hd_decor_remove_actors(HD_DECOR(obj));
}

static int
hd_decor_init (MBWMObject *obj, va_list vap)
{
  HdDecor *d = HD_DECOR (obj);

  d->progress_timeline = 0;
  d->progress_actor = 0;
  d->title_bar_actor = 0;
  d->title_actor = 0;

  return 1;
}

HdDecor*
hd_decor_new (MBWindowManager      *wm,
              MBWMDecorType         type)
{
  MBWMObject *decor;

  decor = mb_wm_object_new (HD_TYPE_DECOR,
                            MBWMObjectPropWm,               wm,
                            MBWMObjectPropDecorType,        type,
                            NULL);

  return HD_DECOR(decor);
}

int
hd_decor_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdDecorClass),
	sizeof (HdDecor),
	hd_decor_init,
	hd_decor_destroy,
	hd_decor_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_DECOR, 0);
    }

  return type;
}

static gboolean
hd_decor_window_is_waiting (MBWindowManager *wm, Window w)
{
  HdCompMgr *hmgr = HD_COMP_MGR (wm->comp_mgr);
  Atom progress_indicator =
      hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_PROGRESS_INDICATOR);
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

static void
on_decor_progress_timeline_new_frame(ClutterTimeline *timeline,
                                     gint frame_num, HdDecor *decor)
{
  if (decor->progress_actor)
    {
      float amt = (float)clutter_timeline_get_progress(timeline);

      clutter_actor_set_rotation(
          decor->progress_actor, CLUTTER_Z_AXIS,
          amt*360, 0, 0, 0);
      /* FIXME: We really want to set this to queue damage with an area -
       * like we do for windows. Otherwise we end up updating the whole
       * screen for this. */
    }
}

static
MBWindowManagerClient *hd_decor_get_client(HdDecor   *decor)
{
  if (!decor)
    return NULL;

  return MB_WM_DECOR(decor)->parent_client;
}

ClutterActor *
hd_decor_get_actor(HdDecor   *decor)
{
  MBWindowManagerClient* client = hd_decor_get_client(decor);
  if (!client || !client->cm_client)
    return NULL;
  return mb_wm_comp_mgr_clutter_client_get_actor(
            MB_WM_COMP_MGR_CLUTTER_CLIENT(client->cm_client));
}

static gint
hd_decor_get_start_x(HdDecor *decor)
{
  MBWMDecor         *mb_decor = MB_WM_DECOR (decor);
  MBWindowManagerClient  *client = mb_decor->parent_client;
  MBWMClientType          c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  ClutterActor *status_area = hd_render_manager_get_status_area();

  /* because only apps are fullscreen and care about the status area */
  if (c_type != MBWMClientTypeApp)
    return 0;

  gint buttons_width = HD_COMP_MGR_TOP_LEFT_BTN_WIDTH;
  if (status_area)
    buttons_width += clutter_actor_get_width(status_area);

  return buttons_width;
}

static void
hd_decor_remove_actors(HdDecor   *decor)
{
  if (decor->progress_timeline)
    {
      clutter_timeline_stop(decor->progress_timeline);
      g_object_unref(decor->progress_timeline);
      decor->progress_timeline = 0;
    }
  if (decor->progress_actor)
    {
      clutter_actor_destroy(decor->progress_actor);
      decor->progress_actor = 0;
    }
  if (decor->title_bar_actor)
    {
      clutter_actor_destroy(decor->title_bar_actor);
      decor->title_bar_actor = 0;
    }
  if (decor->title_actor)
    {
      clutter_actor_destroy(decor->title_actor);
      decor->title_actor = 0;
    }
}

/* Fill the ClutterActor for the given decor with the correct
 * clutter actors to display the title */
static void
hd_decor_create_actors(HdDecor *decor)
{
  MBWMDecor         *mb_decor = MB_WM_DECOR (decor);
  ClutterActor      *actor = hd_decor_get_actor(decor);
  MBWindowManagerClient  *client = mb_decor->parent_client;
  MBWMClientType          c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMXmlClient     *c;
  MBWMXmlDecor      *d;
  ClutterGeometry   geo, area;
  gint              x_start;
  gboolean          is_waiting;

  if (!client)
    return;

  if (!((c = mb_wm_xml_client_find_by_type
                      (client->wmref->theme->xml_clients, c_type)) &&
        (d = mb_wm_xml_decor_find_by_type (c->decors, mb_decor->type))))
    return;

  is_waiting = hd_decor_window_is_waiting(client->wmref,
                                          client->window->xwindow);
  x_start = hd_decor_get_start_x(decor);

  geo.x = d->x;
  geo.y = d->y;
  geo.width = d->width;
  geo.height = d->height;
  area.x = 0;
  area.y = 0;
  area.width = mb_decor->geom.width;
  area.height = mb_decor->geom.height;

  decor->title_bar_actor = hd_clutter_cache_get_sub_texture_for_area(
      client->wmref->theme->image_filename,
      &geo,
      &area);
  clutter_container_add_actor(CLUTTER_CONTAINER(actor), decor->title_bar_actor);

  /* add the title */
  if (d->show_title)
    {
      const char* title = mb_wm_client_get_name (client);
      if (title && strlen(title)) {
        ClutterLabel *bar_title;
        ClutterColor white = { 0xFF, 0xFF, 0xFF, 0xFF };
        char font_name[512];
        gint x;
        guint w,h;
        gboolean centre_title;

        centre_title = c_type != MBWMClientTypeApp;

        bar_title = CLUTTER_LABEL(clutter_label_new());
        clutter_label_set_color(bar_title, &white);
        clutter_label_set_use_markup(bar_title, TRUE);

        decor->title_actor = CLUTTER_ACTOR(bar_title);
        clutter_container_add_actor(CLUTTER_CONTAINER(actor),
                                    decor->title_actor);

        if (!title) title = "";
        snprintf (font_name, sizeof (font_name), "%s %i%s",
                  d->font_family ? d->font_family : "Sans",
                  d->font_size ? d->font_size : 18,
                  d->font_units == MBWMXmlFontUnitsPoints ? "" : "px");
        clutter_label_set_font_name(bar_title, font_name);
        clutter_label_set_text(bar_title, title);

        clutter_actor_get_size(CLUTTER_ACTOR(bar_title), &w, &h);
        x = centre_title ? ((mb_decor->geom.width-w)/2) :
                           (x_start+HD_DECOR_TITLE_MARGIN);
        clutter_actor_set_position(CLUTTER_ACTOR(bar_title),
                                   x,
                                   (mb_decor->geom.height-h)/2);
      }
    }

  /* Add the progress indicator if required */
  if (is_waiting)
    {
      /* Get the actor we're going to rotate and put it on the right-hand
       * side of the window*/
      ClutterGeometry progress_geo = {672, 0, 60, 60};
      decor->progress_actor = hd_clutter_cache_get_sub_texture(
          client->wmref->theme->image_filename,
          &progress_geo);
      clutter_container_add_actor(CLUTTER_CONTAINER(actor),
                                  decor->progress_actor);
      clutter_actor_set_anchor_point_from_gravity(decor->progress_actor,
                                                  CLUTTER_GRAVITY_CENTER);
      clutter_actor_set_position(decor->progress_actor,
          mb_decor->geom.width - (mb_decor->geom.height/2),
          mb_decor->geom.height/2);
      /* Get the timeline and set it running */
      decor->progress_timeline = g_object_ref(
          clutter_timeline_new(8, 5));
      clutter_timeline_set_loop(decor->progress_timeline, TRUE);
      g_signal_connect (decor->progress_timeline, "new-frame",
                        G_CALLBACK (on_decor_progress_timeline_new_frame), decor);
      clutter_timeline_start(decor->progress_timeline);
    }
}

void hd_decor_paint_decor(HdDecor   *decor,
                          MBWMTheme *theme)
{
  MBWindowManagerClient  *client = MB_WM_DECOR(decor)->parent_client;
  ClutterActor *actor = hd_decor_get_actor(decor);
  ClutterGeometry geom;
  gboolean fullscreen = FALSE;
  MBWMList *l;

  if (!actor) return;
  clutter_actor_get_geometry(actor, &geom);

  if (client && mb_wm_client_window_is_state_set (
        client->window, MBWMClientWindowEWMHStateFullscreen))
    fullscreen = TRUE;

  hd_decor_remove_actors(decor);
  if (MB_WM_DECOR(decor)->geom.width > 0 &&
      MB_WM_DECOR(decor)->geom.height > 0 &&
      !fullscreen)
    {
      /* For dialogs, etc. We need to fill our clutter group with
       * all the actors needed to draw it. */
      hd_decor_create_actors(decor);
    }

  /* Handle the buttons */
  l = MB_WM_DECOR(decor)->buttons;
  while (l)
    {
      HdDecorButton * button = l->data;
      hd_decor_button_sync_actors(button);

      l = l->next;
    }
}
