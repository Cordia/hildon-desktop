/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2010 Moises Martinez
 *
 * Author:  Moises Martinez <moimart@gmail.com>
 *	    inspired by old hd-task-navigator.[ch] which
 *          didn't indicate license or author
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

#ifndef __HD_TASK_NAVIGATOR_H__
#define __HD_TASK_NAVIGATOR_H__

#include <clutter/clutter.h>
#include <X11/extensions/Xrandr.h> /* For Rotation */
#include <hd-note.h>

G_BEGIN_DECLS

typedef void (*HdTaskNavigatorFunc) (ClutterActor *actor, gpointer data);

#define HD_TYPE_TASK_NAVIGATOR            (hd_task_navigator_get_type ())
#define HD_TASK_NAVIGATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_TASK_NAVIGATOR, HdTaskNavigator))
#define HD_TASK_NAVIGATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_TASK_NAVIGATOR, HdTaskNavigatorClass))
#define HD_IS_TASK_NAVIGATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_TASK_NAVIGATOR))
#define HD_IS_TASK_NAVIGATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_TASK_NAVIGATOR))
#define HD_TASK_NAVIGATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_TASK_NAVIGATOR, HdTaskNavigatorClass))

/* Adds nothing to its parent. */
typedef struct _HdTaskNavigator HdTaskNavigator;
typedef struct _HdTaskNavigatorClass HdTaskNavigatorClass;
typedef struct _HdTaskNavigatorPrivate HdTaskNavigatorPrivate;

struct _HdTaskNavigator
{
  ClutterGroup             parent;

  HdTaskNavigatorPrivate   *priv;
};

struct _HdTaskNavigatorClass
{
  ClutterGroupClass        parent_class;
};

GType            hd_task_navigator_get_type  (void);

HdTaskNavigator *hd_task_navigator_new       (void);

gboolean hd_task_navigator_is_active  (HdTaskNavigator *navigator);
gboolean hd_task_navigator_is_empty   (HdTaskNavigator *navigator);
gboolean hd_task_navigator_is_crowded (HdTaskNavigator *navigator);
gboolean hd_task_navigator_has_apps   (HdTaskNavigator *navigator);
gboolean hd_task_navigator_has_notifications (HdTaskNavigator *navigator);
gboolean hd_task_navigator_has_window (HdTaskNavigator * navigator,
                                       ClutterActor * win);
ClutterActor *hd_task_navigator_find_app_actor (HdTaskNavigator *navigator,
                                                const gchar *id);

void hd_task_navigator_scroll_back (HdTaskNavigator *navigator);

void hd_task_navigator_zoom_in   (HdTaskNavigator *navigator,
                                  ClutterActor *win,
                                  HdTaskNavigatorFunc fun,
                                  gpointer funparam);

void hd_task_navigator_zoom_out  (HdTaskNavigator *navigator,
                                  ClutterActor *win,
                                  HdTaskNavigatorFunc fun,
                                  gpointer funparam);

void hd_task_navigator_add_window       (HdTaskNavigator *navigator,
                                         ClutterActor *win);

void hd_task_navigator_remove_window    (HdTaskNavigator *navigator,
                                         ClutterActor *win,
                                         HdTaskNavigatorFunc fun,
                                         gpointer funparam);

void hd_task_navigator_hibernate_window (HdTaskNavigator *navigator,
                                         ClutterActor *win);
void hd_task_navigator_replace_window   (HdTaskNavigator *navigator,
                                         ClutterActor *old_win,
                                         ClutterActor *new_win);
void hd_task_navigator_notification_thread_changed (HdTaskNavigator *navigator,
                                                    ClutterActor *win,
                                                    char *nothread);

void hd_task_navigator_add_dialog (HdTaskNavigator *navigator,
                                   ClutterActor *win,
                                   ClutterActor *dialog);

void hd_task_navigator_remove_dialog (HdTaskNavigator *navigator,
                                      ClutterActor *dialog);

void hd_task_navigator_add_notification    (HdTaskNavigator *navigator,
                                            HdNote *hdnote);

void hd_task_navigator_remove_notification (HdTaskNavigator *navigator,
                                            HdNote *hdnote);

void hd_task_navigator_rotate (HdTaskNavigator *tn, 
		  	       Rotation rotation); 

#define HD_TYPE_TN_THUMBNAIL            (hd_tn_thumbnail_get_type ())
#define HD_TN_THUMBNAIL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_TN_THUMBNAIL, HdTnThumbnail))
#define HD_TN_THUMBNAIL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_TN_THUMBNAIL, HdTnThumbnailClass))
#define HD_IS_TN_THUMBNAIL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_TN_THUMBNAIL))
#define HD_IS_TN_THUMBNAIL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_TN_THUMBNAIL))
#define HD_TN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_TN_THUMBNAIL, HdTnThumbnailClass))

/* Adds nothing to its parent. */
typedef struct _HdTnThumbnail HdTnThumbnail;
typedef struct _HdTnThumbnailClass HdTnThumbnailClass;
typedef struct _HdTnThumbnailPrivate HdTnThumbnailPrivate;

struct _HdTnThumbnail
{
  ClutterGroup             parent;

  HdTnThumbnailPrivate   *priv;
};

struct _HdTnThumbnailClass
{
  ClutterGroupClass        parent_class;
};

GType            hd_tn_thumbnail_get_type  (void);

ClutterActor    *hd_tn_thumbnail_new       (ClutterActor *window);

void hd_tn_thumbnail_claim_window (HdTnThumbnail *thumbnail);
 
void hd_tn_thumbnail_release_window (HdTnThumbnail *thumbnail);

void hd_tn_thumbnail_replace_window (HdTnThumbnail *thumbnail, ClutterActor *new_window);

void hd_tn_thumbnail_add_dialog (HdTnThumbnail *thumbnail, ClutterActor *dialog);

void hd_tn_thumbnail_remove_dialog (HdTnThumbnail *thumbnail, ClutterActor *dialog);

void hd_tn_thumbnail_reparent_dialog (HdTnThumbnail *thumbnail, ClutterActor *dialog);

gboolean hd_tn_thumbnail_has_dialog (HdTnThumbnail *thumbnail, ClutterActor *dialog);

gboolean hd_tn_thumbnail_has_dialogs (HdTnThumbnail *thumbnail);

void hd_tn_thumbnail_set_jail_scale (HdTnThumbnail *thumbnail, gdouble x, gdouble y);
 
void hd_tn_thumbnail_get_jail_scale (HdTnThumbnail *thumbnail, gdouble *x, gdouble *y);
 
gboolean hd_tn_thumbnail_is_app (HdTnThumbnail *thumbnail);

void hd_tn_thumbnail_update_inners (HdTnThumbnail *thumbnail, gint title_size);

ClutterActor *hd_tn_thumbnail_get_app_window (HdTnThumbnail *thumbnail);

G_END_DECLS

#endif /* ! __HD_TASK_NAVIGATOR_H__ */
