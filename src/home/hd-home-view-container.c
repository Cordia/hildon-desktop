/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hd-home-view-container.h"
#include "hd-home-view.h"
#include "hd-home.h"
#include "hd-comp-mgr.h"
#include "hd-render-manager.h"
#include "hd-transition.h"

#include <glib/gstdio.h>

#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>

#include <matchbox/core/mb-wm.h>

#include <string.h>
#include <math.h>

#define MAX_HOME_VIEWS 4

#define HD_GCONF_DIR_VIEWS         "/apps/osso/hildon-desktop/views"
#define HD_GCONF_KEY_VIEWS_ACTIVE  HD_GCONF_DIR_VIEWS "/active"
#define HD_GCONF_KEY_VIEWS_CURRENT HD_GCONF_DIR_VIEWS "/current"

#define BACKGROUNDS_DIR ".backgrounds"

struct _HdHomeViewContainerPrivate
{
  ClutterActor *views[MAX_HOME_VIEWS];
  gboolean active_views [MAX_HOME_VIEWS];

  guint current_view;
  guint previous_view;
  guint next_view;

  int offset;

  HdHome *home;
  HdCompMgr *comp_mgr;

  /* animation */
  ClutterTimeline *timeline;
  int timeline_offset;
  guint frames;
  gboolean animation_overshoot;

  /* GConf */
  GConfClient *gconf_client;

  GnomeVFSMonitorHandle *backgrounds_dir_monitor;

  guint views_active_notify;
};

enum
{
  PROP_0,
  PROP_COMP_MGR,
  PROP_HOME,
  PROP_CURRENT_VIEW,
  PROP_OFFSET
};

G_DEFINE_TYPE (HdHomeViewContainer, hd_home_view_container, CLUTTER_TYPE_GROUP);

static void
hd_home_view_container_update_previous_and_next_view (HdHomeViewContainer *self)
{
  HdHomeViewContainerPrivate *priv = self->priv;
  guint previous_view, next_view;

  previous_view = next_view = priv->current_view;

  do
    previous_view = (previous_view + MAX_HOME_VIEWS - 1) % MAX_HOME_VIEWS;
  while (previous_view != priv->current_view && !priv->active_views[previous_view]);

  do
    next_view = (next_view + 1) % MAX_HOME_VIEWS;
  while (next_view != priv->current_view && !priv->active_views[next_view]);

  priv->previous_view = previous_view;
  priv->next_view = next_view;
}

static void
backgrounds_dir_changed (GnomeVFSMonitorHandle    *handler,
                         const gchar              *monitor_uri,
                         const gchar              *info_uri,
                         GnomeVFSMonitorEventType  event_type,
                         HdHomeViewContainer      *view_container)
{
  HdHomeViewContainerPrivate *priv = view_container->priv;

  if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED ||
      event_type == GNOME_VFS_MONITOR_EVENT_CHANGED)
    {
      gchar *filename, *basename;

      g_debug ("%s. %s %s.",
               __FUNCTION__,
               info_uri,
               event_type == GNOME_VFS_MONITOR_EVENT_CREATED ?
                             "created" : "changed");

      filename = g_filename_from_uri (info_uri, NULL, NULL);
      basename = g_path_get_basename (filename);

      if (g_str_has_prefix (basename, "background-") &&
          (g_str_has_suffix (basename, ".pvr") ||
           g_str_has_suffix (basename, ".png")))
        {
          guint id;

          id = atoi (basename + 11) - 1; /* id is from 0..MAX_HOME_VIEWS - 1 */

          if (id < MAX_HOME_VIEWS && priv->active_views[id])
            {
              g_debug ("%s. Reload background %s for view %u.", __FUNCTION__,
                       info_uri, id + 1);
              hd_home_view_load_background (HD_HOME_VIEW (priv->views[id]));
            }
        }

      g_free (filename);
      g_free (basename);
    }
}

static void
hd_home_view_container_update_active_views (HdHomeViewContainer *self,
                                            gboolean             constructed)
{
  HdHomeViewContainerPrivate *priv = self->priv;
  GSList *list;
  guint active_views[MAX_HOME_VIEWS] = { 0, };
  gboolean none_active = TRUE;
  guint i;
  guint current_view;
  GError *error = NULL;

  /* Read active views from GConf */
  list = gconf_client_get_list (priv->gconf_client,
                                HD_GCONF_KEY_VIEWS_ACTIVE,
                                GCONF_VALUE_INT,
                                &error);

  if (!error)
    {
      GSList *l;

      for (l = list; l; l = l->next)
        {
          gint id = GPOINTER_TO_INT (l->data);

          /* Stored in GConf 1..MAX_HOME_VIEWS */

          if (id > 0 && id <= MAX_HOME_VIEWS)
            {
              active_views[id - 1] = TRUE;
              none_active = FALSE;
            }
        }
      g_slist_free (list);
    }
  else
    {
      /* Error */
      g_warning ("Error reading active views from GConf. %s", error->message);
      error = (g_error_free (error), NULL);
    }

  /* Check if there is an view active */
  if (none_active)
    {
      g_warning ("No active views. Make first view active");
      active_views[0] = TRUE;
    }

  /* Read current view from GConf on construction */
  if (constructed)
    {
      current_view = (guint) gconf_client_get_int (priv->gconf_client,
                                                   HD_GCONF_KEY_VIEWS_CURRENT,
                                                   &error);
      current_view--;
      if (error)
        {
          g_warning ("Error reading current view from GConf. %s", error->message);
          error = (g_error_free (error), NULL);
          current_view = 0;
        }

      /* Clamp to valid values */
      current_view = current_view > MAX_HOME_VIEWS ? 0 : current_view;
    }
  else
    {
      current_view = priv->current_view;
    }

  /* Set current view to an active view*/
  i = current_view;
  while (!active_views[i % MAX_HOME_VIEWS] && i - current_view < MAX_HOME_VIEWS)
    i++;
  current_view = (i % MAX_HOME_VIEWS);

  /* DEBUG */
  g_debug ("%s Active views:", __FUNCTION__);
  for (i = 0; i < MAX_HOME_VIEWS; i++)
    {
      g_debug ("%s %u %s", __FUNCTION__, i, active_views[i] ? "active" : "not active");
    }
  g_debug ("%s Current view: %u", __FUNCTION__, current_view);

  if (constructed)
    {
      memcpy (priv->active_views, active_views, sizeof (gboolean) * MAX_HOME_VIEWS);
      for (i = 0; i < MAX_HOME_VIEWS; i++)
        {
          priv->active_views[i] = active_views[i];
          if (active_views[i])
            clutter_actor_show (priv->views[i]);
          else
            clutter_actor_hide (priv->views[i]);
        }

      hd_home_view_container_set_current_view (self, current_view);

      /* Load backgrounds for active views */
      for (i = 0; i < MAX_HOME_VIEWS; i++)
        hd_home_view_load_background (HD_HOME_VIEW (priv->views[i]));
    }
  else
    {
      for (i = 0; i < MAX_HOME_VIEWS; i++)
        {
          if (active_views[i] && !priv->active_views[i])
            {
              hd_home_view_load_background (HD_HOME_VIEW (priv->views[i]));
              priv->active_views[i] = active_views[i];
              clutter_actor_show (priv->views[i]);
              g_object_notify (G_OBJECT (priv->views[i]), "active");
            }
        }

      for (i = 0; i < MAX_HOME_VIEWS; i++)
        {
          if (!active_views[i] && priv->active_views[i])
            {
              priv->active_views[i] = active_views[i];
              clutter_actor_hide (priv->views[i]);
              hd_home_view_close_all_applets (HD_HOME_VIEW (priv->views[i]));
              g_object_notify (G_OBJECT (priv->views[i]), "active");
            }
        }

      /* FIXME scroll to new view and do not switch */
      if (current_view != priv->current_view)
        {
          hd_home_view_container_set_current_view (self, current_view);
        }
      else
        {
          hd_home_view_container_update_previous_and_next_view (self);
        }
    }
}

static void
views_active_notify_func (GConfClient *client,
                          guint        cnxn_id,
                          GConfEntry  *entry,
                          gpointer     user_data)
{
  hd_home_view_container_update_active_views (HD_HOME_VIEW_CONTAINER (user_data),
                                              FALSE);
}

static void
hd_home_view_container_constructed (GObject *self)
{
  HdHomeViewContainer *container = HD_HOME_VIEW_CONTAINER (self);
  HdHomeViewContainerPrivate *priv = container->priv;
  guint i;
  MBWindowManager *wm;
  long propvalue[1];
  gchar *backgrounds_dir, *backgrounds_dir_uri;
  GnomeVFSResult result;
  GError *error = NULL;

  if (G_OBJECT_CLASS (hd_home_view_container_parent_class)->constructed)
    G_OBJECT_CLASS (hd_home_view_container_parent_class)->constructed (self);

  /* Create home views */
  for (i = 0; i < MAX_HOME_VIEWS; i++)
    {
      priv->views[i] = g_object_new (HD_TYPE_HOME_VIEW,
                                     "comp-mgr", priv->comp_mgr,
                                     "home", priv->home,
                                     "id", i,
                                     "view-container", container,
                                     NULL);
      clutter_container_add_actor (CLUTTER_CONTAINER (self),
                                   priv->views[i]);
    }

  priv->gconf_client = gconf_client_get_default ();

  gconf_client_add_dir (priv->gconf_client,
                        HD_GCONF_DIR_VIEWS,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        &error);
  if (G_UNLIKELY (error))
    {
      g_warning ("Could not watch GConf dir %s. %s", HD_GCONF_DIR_VIEWS, error->message);
      error = (g_error_free (error), NULL);
    }

  priv->views_active_notify = gconf_client_notify_add (priv->gconf_client,
                                                       HD_GCONF_KEY_VIEWS_ACTIVE,
                                                       views_active_notify_func,
                                                       self,
                                                       NULL,
                                                       &error);
  if (G_UNLIKELY (error))
    {
      g_warning ("Could not add notify to GConf key %s. %s", HD_GCONF_KEY_VIEWS_ACTIVE, error->message);
      error = (g_error_free (error), NULL);
    }

  /* Set _NET_NUMBER_OF_DESKTOPS property */
  wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  propvalue[0] = MAX_HOME_VIEWS;

  XChangeProperty (wm->xdpy, wm->root_win->xwindow,
		   wm->atoms[MBWM_ATOM_NET_NUMBER_OF_DESKTOPS],
		   XA_CARDINAL, 32, PropModeReplace,
		   (unsigned char *) propvalue,
		   1);

  /* Monitor ~/.backgrounds dir */
  backgrounds_dir = g_build_filename (g_get_home_dir (),
                                      BACKGROUNDS_DIR,
                                      NULL);
  if (g_mkdir_with_parents (backgrounds_dir,
                            S_IRUSR | S_IWUSR | S_IXUSR |
                            S_IRGRP | S_IXGRP |
                            S_IROTH | S_IXOTH))
    {
      g_warning ("Could not make %s dir", backgrounds_dir);
    }

  backgrounds_dir_uri = gnome_vfs_get_uri_from_local_path (backgrounds_dir);
  result = gnome_vfs_monitor_add (&priv->backgrounds_dir_monitor,
                                  backgrounds_dir_uri,
                                  GNOME_VFS_MONITOR_DIRECTORY,
                                  (GnomeVFSMonitorCallback) backgrounds_dir_changed,
                                  self);

  if (result == GNOME_VFS_OK)
    g_debug ("%s. Started to monitor %s",
             __FUNCTION__,
             backgrounds_dir_uri);
  else
    g_warning ("Cannot monitor directory ~/.backgrounds for changed background files. %s",
               gnome_vfs_result_to_string (result));

  g_free (backgrounds_dir);
  g_free (backgrounds_dir_uri);

  hd_home_view_container_update_active_views (container, TRUE);
}

static void
hd_home_view_container_dispose (GObject *self)
{
  HdHomeViewContainerPrivate *priv = HD_HOME_VIEW_CONTAINER (self)->priv;

  if (priv->home)
    priv->home = (g_object_unref (priv->home), NULL);

  if (priv->backgrounds_dir_monitor)
    priv->backgrounds_dir_monitor = (gnome_vfs_monitor_cancel (priv->backgrounds_dir_monitor), NULL);

  G_OBJECT_CLASS (hd_home_view_container_parent_class)->dispose (self);
}

static void
hd_home_view_container_set_property (GObject      *self,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HdHomeViewContainerPrivate *priv = HD_HOME_VIEW_CONTAINER (self)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      priv->comp_mgr = g_value_get_pointer (value);
      break;
    case PROP_HOME:
      if (priv->home)
        g_object_unref (priv->home);
      priv->home = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
    }
}

static void
hd_home_view_container_get_property (GObject    *self,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  HdHomeViewContainerPrivate *priv = HD_HOME_VIEW_CONTAINER (self)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      g_value_set_pointer (value, priv->comp_mgr);
      break;
    case PROP_HOME:
      g_value_set_object (value, priv->home);
      break;
    case PROP_CURRENT_VIEW:
      g_value_set_uint (value, priv->current_view);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
    }
}

static void
hd_home_view_container_allocate (ClutterActor          *self,
                                 const ClutterActorBox *box,
                                 gboolean               absolute_origin_changed)
{
  HdHomeViewContainer *container = HD_HOME_VIEW_CONTAINER (self);
  HdHomeViewContainerPrivate *priv = container->priv;
  ClutterUnit width, height;
  guint i;
  ClutterActorBox child_box = { 0, };
  ClutterUnit offset = 0;

  /* Chain up */
  CLUTTER_ACTOR_CLASS (hd_home_view_container_parent_class)->allocate (self,
                                                                       box,
                                                                       absolute_origin_changed);

  width = box->x2 - box->x1;
  height = box->y2 - box->y1;

  if (priv->previous_view != priv->current_view && priv->next_view != priv->current_view)
    offset = CLUTTER_UNITS_FROM_INT(priv->offset);

  for (i = 0; i < MAX_HOME_VIEWS; i++)
    {
      if (!priv->active_views[i])
        continue;

      child_box.x1 = width;
      child_box.y1 = 0;
      child_box.x2 = width + width;
      child_box.y2 = height;

      if (i == priv->current_view)
        {
          child_box.x1 = offset;
          child_box.x2 = offset + width;
        }
      else if (i == priv->previous_view && offset > 0)
        {
          child_box.x1 = offset - width;
          child_box.x2 = offset;
        }
      else if (i == priv->next_view && offset < 0)
        {
          child_box.x1 += offset;
          child_box.x2 += offset;
        }

      clutter_actor_allocate (priv->views[i], &child_box, absolute_origin_changed);

      /* Make sure offscreen views are hidden. Views positioned offscreen
       * wouldn't be seen anyway, but they introduce extra overheads in
       * Clutter/GLES if they are CLUTTER_ACTOR_IS_VISIBLE. We use < rather
       * than <= here because given the positioning above, the clause would
       * always be true otherwise */
      if ((child_box.x1 < width) &&
          (child_box.x2 > 0))
        {
          if (!CLUTTER_ACTOR_IS_VISIBLE(priv->views[i]))
            clutter_actor_show(priv->views[i]);
        }
      else
        {
          if (CLUTTER_ACTOR_IS_VISIBLE(priv->views[i]))
            clutter_actor_hide(priv->views[i]);
        }
    }
}

static void
hd_home_view_container_class_init (HdHomeViewContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  object_class->constructed = hd_home_view_container_constructed;
  object_class->dispose = hd_home_view_container_dispose;
  object_class->get_property = hd_home_view_container_get_property;
  object_class->set_property = hd_home_view_container_set_property;

  actor_class->allocate = hd_home_view_container_allocate;

  g_type_class_add_private (klass, sizeof (HdHomeViewContainerPrivate));

  g_object_class_install_property (object_class,
                                   PROP_COMP_MGR,
                                   g_param_spec_pointer ("comp-mgr",
                                                         "Composite Manager",
                                                         "Hildon Desktop Clutter Composite Manager object",
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_HOME,
                                   g_param_spec_object ("home",
                                                        "Home",
                                                        "Hildon Desktop Home actor",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_CURRENT_VIEW,
                                   g_param_spec_uint ("current-view",
                                                      "Current view",
                                                      "The current view",
                                                      0,
                                                      MAX_HOME_VIEWS - 1,
                                                      0,
                                                      G_PARAM_READABLE));
}

static void
hd_home_view_container_init (HdHomeViewContainer *self)
{
  HdHomeViewContainerPrivate *priv;

  /* Create priv member */
  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   HD_TYPE_HOME_VIEW_CONTAINER, HdHomeViewContainerPrivate);
}

ClutterActor *
hd_home_view_container_new (HdCompMgr    *comp_mgr,
                            ClutterActor *home)
{
  ClutterActor *actor;

  actor = g_object_new (HD_TYPE_HOME_VIEW_CONTAINER,
                        "comp-mgr", comp_mgr,
                        "home", home,
                        NULL);

  return actor;
}

guint
hd_home_view_container_get_current_view (HdHomeViewContainer *container)
{
  HdHomeViewContainerPrivate *priv;

  g_return_val_if_fail (HD_IS_HOME_VIEW_CONTAINER (container), 0);

  priv = container->priv;

  return priv->current_view;
}

void
hd_home_view_container_set_current_view (HdHomeViewContainer *container,
                                         guint                current_view)
{
  HdHomeViewContainerPrivate *priv;
  MBWindowManager *wm;
  long propvalue[1];
  GError *error = NULL;

  g_return_if_fail (HD_IS_HOME_VIEW_CONTAINER (container));
  g_return_if_fail (current_view >= 0 && current_view < MAX_HOME_VIEWS);

  priv = container->priv;

  /* Determine previous and next views */
  priv->current_view = current_view;

  hd_home_view_container_update_previous_and_next_view (container);

  /* Store current view in GConf */
  gconf_client_set_int (priv->gconf_client,
                        HD_GCONF_KEY_VIEWS_CURRENT,
                        current_view + 1,
                        &error);

  if (error)
    {
      g_warning ("Could not store current view to GConf (%s). %s",
                 HD_GCONF_KEY_VIEWS_CURRENT,
                 error->message);
      g_clear_error (&error);
    }

  /* Sync GConf */
  gconf_client_suggest_sync (priv->gconf_client,
                             &error);

  if (error)
    {
      g_warning ("Could not sync GConf. %s",
                 error->message);
      g_clear_error (&error);
    }

  /* Set _NET_DESKTOP porperty to root window */
  wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  propvalue[0] = current_view;

  XChangeProperty (wm->xdpy, wm->root_win->xwindow,
		   wm->atoms[MBWM_ATOM_NET_CURRENT_DESKTOP],
		   XA_CARDINAL, 32, PropModeReplace,
		   (unsigned char *) propvalue,
		   1);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));
}

ClutterActor *
hd_home_view_container_get_view (HdHomeViewContainer *container,
                                 guint                view_id)
{
  HdHomeViewContainerPrivate *priv;

  g_return_val_if_fail (HD_IS_HOME_VIEW_CONTAINER (container), NULL);
  g_return_val_if_fail (view_id < MAX_HOME_VIEWS, NULL);

  priv = container->priv;

  return priv->views[view_id];
}

gboolean
hd_home_view_container_get_active (HdHomeViewContainer *container,
                                   guint                view_id)
{
  HdHomeViewContainerPrivate *priv;

  g_return_val_if_fail (HD_IS_HOME_VIEW_CONTAINER (container), FALSE);
  g_return_val_if_fail (view_id < MAX_HOME_VIEWS, FALSE);

  priv = container->priv;

  return priv->active_views[view_id];
}

void
hd_home_view_container_set_offset (HdHomeViewContainer *container,
                                   ClutterUnit          offset)
{
  HdHomeViewContainerPrivate *priv;

  g_return_if_fail (HD_IS_HOME_VIEW_CONTAINER (container));

  priv = container->priv;

  if (priv->timeline)
    return;

  priv->offset = CLUTTER_UNITS_TO_INT(offset);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));
}

static void
scroll_back_new_frame_cb (ClutterTimeline     *timeline,
                          gint                 frame_num,
                          HdHomeViewContainer *container)
{
  HdHomeViewContainerPrivate *priv = container->priv;

  float amt = frame_num / (float)priv->frames;
  amt = hd_transition_ease_out(amt);
  /* If we overshoot, go negative for the first part of the transition */
  if (priv->animation_overshoot)
    amt = amt * -cos(amt*3.141592);

  priv->offset = (int)((1-amt) * priv->timeline_offset);
  /* prod the blur control so it notices that the background has changed */
  hd_render_manager_blurred_changed();

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));
}

static void
scroll_back_completed_cb (ClutterTimeline     *timeline,
                          HdHomeViewContainer *container)
{
  HdHomeViewContainerPrivate *priv = container->priv;
  HdCompMgr *hmgr = HD_COMP_MGR (priv->comp_mgr);
  MBWindowManagerClient *desktop;

  if (priv->timeline)
    priv->timeline = (g_object_unref (priv->timeline), NULL);

  desktop = hd_comp_mgr_get_desktop_client (hmgr);

  if (desktop)
    { /* Synchronize here, we may not come from clutter_x11_event_filter()
       * at all. */
      mb_wm_client_stacking_mark_dirty (desktop);
      mb_wm_sync (MB_WM_COMP_MGR (hmgr)->wm);
    }
}

/* Velocity is the speed in pixels/second, and we attempt to set the scroll
 * speed accordingly */
void
hd_home_view_container_scroll_back (HdHomeViewContainer *container, gint velocity)
{
  HdHomeViewContainerPrivate *priv;
  guint width;

  g_return_if_fail (HD_IS_HOME_VIEW_CONTAINER (container));

  priv = container->priv;

  if (priv->timeline)
    return;

  clutter_actor_get_size (CLUTTER_ACTOR (container), &width, NULL);

  /* if no velocity, make something up */
  if (velocity == 0)
    {
      priv->animation_overshoot = FALSE;
      velocity = width*5;
    }
  else
    {
      /* Overshoot if we were going in one direction, but expect to
       * go in the other */
      priv->animation_overshoot = FALSE;//(velocity>0) != (priv->offset>0);
      /* make sure velocity is within a sensible range, we don't want this
       * taking more than a second, or totally flicking past... */
      velocity = ABS(velocity);
      if (velocity > 10000) velocity = 10000;
      if (velocity < width) velocity = width;
    }

  /* we use 1570, because it's roughly 1000 * PI/2 - this keeps the initial speed
   * the same as the drag velocity given in the argument (because the ease_out
   * function in scroll_back_new_frame_cb uses sin, and sin(x)==x near 0) */
  priv->timeline = clutter_timeline_new_for_duration
                        (ABS (priv->offset) * 1570 / velocity);
  /* TODO: when sign(priv->offset) != sign(velocity) then the home view
   * is moving back to its old position, and maybe we should have some
   * overshoot */

  priv->frames = clutter_timeline_get_n_frames (priv->timeline);
  priv->timeline_offset = priv->offset;

  g_debug ("frames: %u, offset: %d",
           priv->frames, priv->offset);

  g_signal_connect (priv->timeline, "new-frame",
                    G_CALLBACK (scroll_back_new_frame_cb), container);
  g_signal_connect (priv->timeline, "completed",
                    G_CALLBACK (scroll_back_completed_cb), container);

  clutter_timeline_start (priv->timeline);
}

void
hd_home_view_container_scroll_to_previous (HdHomeViewContainer *container, gint velocity)
{
  HdHomeViewContainerPrivate *priv;
  guint width;

  g_return_if_fail (HD_IS_HOME_VIEW_CONTAINER (container));

  priv = container->priv;

  if (priv->timeline)
    return;

  clutter_actor_get_size (CLUTTER_ACTOR (container), &width, NULL);

  priv->offset -= width;
  hd_home_view_container_set_current_view (container,
                                           priv->previous_view);

  hd_home_view_container_scroll_back (container, velocity);
}

ClutterTimeline *
hd_home_view_container_scroll_to_next (HdHomeViewContainer *container, gint velocity)
{
  HdHomeViewContainerPrivate *priv;
  guint width;

  g_return_val_if_fail (HD_IS_HOME_VIEW_CONTAINER (container), NULL);

  priv = container->priv;

  if (priv->timeline)
    return NULL;

  clutter_actor_get_size (CLUTTER_ACTOR (container), &width, NULL);

  priv->offset += width;
  hd_home_view_container_set_current_view (container,
                                           priv->next_view);

  hd_home_view_container_scroll_back (container, velocity);

  return priv->timeline;
}

void
hd_home_view_container_set_reactive (HdHomeViewContainer *container,
                                     gboolean             reactive)
{
  HdHomeViewContainerPrivate *priv;
  guint i;

  g_return_if_fail (HD_IS_HOME_VIEW_CONTAINER (container));

  priv = container->priv;

  for (i = 0; i < MAX_HOME_VIEWS; i++)
    if (priv->active_views[i])
      clutter_actor_set_reactive (priv->views[i], reactive);
}

ClutterActor *
hd_home_view_container_get_previous_view (HdHomeViewContainer *container)
{
  HdHomeViewContainerPrivate *priv;

  g_return_val_if_fail (HD_IS_HOME_VIEW_CONTAINER (container), NULL);

  priv = container->priv;

  if (priv->previous_view != priv->current_view)
    return priv->views[priv->previous_view];

  return NULL;
}

ClutterActor *
hd_home_view_container_get_next_view (HdHomeViewContainer *container)
{
  HdHomeViewContainerPrivate *priv;

  g_return_val_if_fail (HD_IS_HOME_VIEW_CONTAINER (container), NULL);

  priv = container->priv;

  if (priv->next_view != priv->current_view)
    return priv->views[priv->next_view];

  return NULL;
}

