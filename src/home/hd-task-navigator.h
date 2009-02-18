#ifndef __HD_TASK_NAVIGATOR_H__
#define __HD_TASK_NAVIGATOR_H__

#include <clutter/clutter.h>
#include "hd-note.h"

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

gboolean hd_task_navigator_is_active  (HdTaskNavigator *self);
gboolean hd_task_navigator_is_empty   (HdTaskNavigator *self);
gboolean hd_task_navigator_has_apps   (HdTaskNavigator *self);
gboolean hd_task_navigator_has_notifications (HdTaskNavigator * self);
gboolean hd_task_navigator_has_window (HdTaskNavigator * self,
                                       ClutterActor * win);

void hd_task_navigator_enter     (HdTaskNavigator *self);
void hd_task_navigator_exit      (HdTaskNavigator *self);

void hd_task_navigator_zoom_in   (HdTaskNavigator *self,
                                  ClutterActor *win,
                                  ClutterEffectCompleteFunc fun,
                                  gpointer funparam);
void hd_task_navigator_zoom_out  (HdTaskNavigator *self,
                                  ClutterActor *win,
                                  ClutterEffectCompleteFunc fun,
                                  gpointer funparam);

void hd_task_navigator_add_window       (HdTaskNavigator *self,
                                         ClutterActor *win);
void hd_task_navigator_remove_window    (HdTaskNavigator *self,
                                         ClutterActor *win,
                                         ClutterEffectCompleteFunc fun,
                                         gpointer funparam);
void hd_task_navigator_replace_window   (HdTaskNavigator *self,
                                         ClutterActor *old_win,
                                         ClutterActor *new_win);
void hd_task_navigator_hibernate_window (HdTaskNavigator *self,
                                         ClutterActor *win);

void hd_task_navigator_add_dialog (HdTaskNavigator *self,
                                   ClutterActor *win,
                                   ClutterActor *dialog);
void hd_task_navigator_remove_dialog (HdTaskNavigator *self,
                                      ClutterActor *dialog);

void hd_task_navigator_add_notification    (HdTaskNavigator *self,
                                            HdNote *hdnote);
void hd_task_navigator_remove_notification (HdTaskNavigator *self,
                                            HdNote *hdnote);

#endif /* ! __HD_TASK_NAVIGATOR_H__ */
