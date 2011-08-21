#ifndef __HD_TASK_NAVIGATOR_H__
#define __HD_TASK_NAVIGATOR_H__

#include <clutter/clutter.h>
#include "mb/hd-note.h"

#define HD_TYPE_TASK_NAVIGATOR    hd_task_navigator_get_type()
#define HD_TASK_NAVIGATOR(obj)                          \
  G_TYPE_CHECK_INSTANCE_CAST((obj),                     \
                             HD_TYPE_TASK_NAVIGATOR,    \
                             HdTaskNavigator)

/* Adds nothing to its parent. */
typedef struct _HdTaskNavigator HdTaskNavigator;
typedef struct _HdTaskNavigatorClass HdTaskNavigatorClass;

struct _HdTaskNavigator
{
  ClutterGroup             parent;
};

struct _HdTaskNavigatorClass
{
  ClutterGroupClass        parent_class;
};

GType            hd_task_navigator_get_type  (void);
HdTaskNavigator *hd_task_navigator_new       (void);

gboolean hd_task_navigator_is_active  (void);
gboolean hd_task_navigator_is_empty   (void);
gboolean hd_task_navigator_is_crowded (void);
gboolean hd_task_navigator_has_apps   (void);
gboolean hd_task_navigator_has_notifications (void);
gboolean hd_task_navigator_has_unseen_notifications (void);
gboolean hd_task_navigator_has_window (HdTaskNavigator * self,
                                       ClutterActor * win);
ClutterActor *hd_task_navigator_find_app_actor (HdTaskNavigator *self,
                                                const gchar *id);

void hd_task_navigator_scroll_back (HdTaskNavigator *self);

void hd_task_navigator_zoom_in   (HdTaskNavigator *self,
                                  ClutterActor *win,
                                  ClutterEffectCompleteFunc fun,
                                  gpointer funparam);
void hd_task_navigator_zoom_out  (HdTaskNavigator *self,
                                  ClutterActor *win,
                                  ClutterEffectCompleteFunc fun,
                                  gpointer funparam);

void hd_task_navigator_transition_done  (HdTaskNavigator *self);

void hd_task_navigator_add_window       (HdTaskNavigator *self,
                                         ClutterActor *win);
void hd_task_navigator_remove_window    (HdTaskNavigator *self,
                                         ClutterActor *win,
                                         ClutterEffectCompleteFunc fun,
                                         gpointer funparam);

void hd_task_navigator_hibernate_window (HdTaskNavigator *self,
                                         ClutterActor *win);
void hd_task_navigator_replace_window   (HdTaskNavigator *self,
                                         ClutterActor *old_win,
                                         ClutterActor *new_win);
void hd_task_navigator_notification_thread_changed (HdTaskNavigator *self,
                                                    ClutterActor *win,
                                                    char *nothread);

void hd_task_navigator_add_dialog (HdTaskNavigator *self,
                                   ClutterActor *win,
                                   ClutterActor *dialog);
void hd_task_navigator_remove_dialog (HdTaskNavigator *self,
                                      ClutterActor *dialog);

void hd_task_navigator_add_notification    (HdTaskNavigator *self,
                                            HdNote *hdnote);
void hd_task_navigator_remove_notification (HdTaskNavigator *self,
                                            HdNote *hdnote);
void hd_task_navigator_activate(int x, int y, int close);

void hd_task_navigator_sort_thumbs(void);
void hd_task_navigator_rotate_thumbs(void);

int hd_task_navigator_mode(void);
void hd_task_navigator_rotate(int mode);
void hd_task_navigator_update_orientation(gboolean portrait);
void hd_task_navigator_update_win_orientation(Window xwindow,gboolean portrait);
gboolean hd_task_navigator_get_disable_portrait(MBWindowManagerClient *c);
#endif /* ! __HD_TASK_NAVIGATOR_H__ */
