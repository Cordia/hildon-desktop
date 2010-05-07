/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008-2009 Nokia Corporation.
 *
 * Author:  Gordon Williams <gordon.williams@collabora.co.uk>
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

/*
 * This object keeps track of all Clutter renderables on the screen and
 * ensures that they are rendered in the correct order.
 */

#ifndef __HD_RENDER_MANAGER_H__
#define __HD_RENDER_MANAGER_H__

#include <clutter/clutter.h>
#include "mb/hd-comp-mgr.h"
#include "hd-task-navigator.h"
#include "hd-home.h"
#include "../launcher/hd-launcher.h"
#include "../tidy/tidy-cached-group.h"

G_BEGIN_DECLS

#define HD_TYPE_RENDER_MANAGER_STATE (hd_render_manager_state_get_type ())
#define HD_TYPE_RENDER_MANAGER      (hd_render_manager_get_type ())
#define HD_RENDER_MANAGER(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_RENDER_MANAGER, HdRenderManager))
#define HD_IS_RENDER_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_RENDER_MANAGER))
#define HD_RENDER_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_RENDER_MANAGER, HdRenderManagerClass))
#define HD_IS_RENDER_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_RENDER_MANAGER))
#define HD_RENDER_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_RENDER_MANAGER, HdRenderManagerClass))

typedef struct _HdRenderManager         HdRenderManager;
typedef struct _HdRenderManagerPrivate  HdRenderManagerPrivate;
typedef struct _HdRenderManagerClass    HdRenderManagerClass;

struct _HdRenderManager
{
  TidyCachedGroup parent_instance;

  HdRenderManagerPrivate *priv;
};

struct _HdRenderManagerClass
{
  TidyCachedGroupClass parent_class;
};

/* Various view states */
typedef enum
{
  HDRM_STATE_UNDEFINED      = 0, /* just for startup - should never use this */
  HDRM_STATE_HOME           = 1 << 0, /* home frontmost */
  HDRM_STATE_HOME_EDIT      = 1 << 1, /* home frontmost, and edit mode */
  HDRM_STATE_HOME_EDIT_DLG  = 1 << 2, /* home frontmost (and looks like edit
                                         mode) - but when the dialogs close we
                                         want to return to HOME_EDIT state. This
                                         has no grab so dialogs work. */
  HDRM_STATE_HOME_PORTRAIT  = 1 << 3,
  HDRM_STATE_APP            = 1 << 4, /* app frontmost */
  HDRM_STATE_APP_PORTRAIT   = 1 << 5,
  HDRM_STATE_TASK_NAV       = 1 << 6,
  HDRM_STATE_LAUNCHER       = 1 << 7,
  HDRM_STATE_NON_COMPOSITED = 1 << 8, /* non-composited fullscreen mode */
  HDRM_STATE_LOADING        = 1 << 9, /* Loading screen */
  HDRM_STATE_LOADING_SUBWIN = 1 << 10, /* Loading screen, but displaying
                                         background apps */
  HDRM_STATE_NON_COMP_PORT  = 1 << 11, /* non-composited portrait mode */
  HDRM_STATE_AFTER_TKLOCK  = 1 << 12 /* bogus mode used in state transition
                                         after tklock */
} HDRMStateEnum;

/* Does the desktop need to be above apps? */
#define STATE_ONE_OF(state, states) (((state) & (states)) != 0)

/* While the task switcher grabs all applications it still needs the desktop
 * (focused) because otherwise hardware keyboard (quick dialing and contacts)
 * won't work in switcher/launcher views. */
#define STATE_NEED_DESKTOP(s) \
  STATE_ONE_OF((s), HDRM_STATE_HOME  | HDRM_STATE_HOME_PORTRAIT | \
                    HDRM_STATE_HOME_EDIT | HDRM_STATE_HOME_EDIT_DLG | \
                    HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV | \
                    HDRM_STATE_LOADING)

#define STATE_NEED_WHOLE_SCREEN_INPUT(s) \
  STATE_ONE_OF((s), HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV | \
                    HDRM_STATE_HOME_EDIT | HDRM_STATE_LOADING_SUBWIN)

#define STATE_NEED_TASK_NAV(s) \
  STATE_ONE_OF((s), HDRM_STATE_TASK_NAV)

#define STATE_IS_APP(s) \
  STATE_ONE_OF((s), HDRM_STATE_APP | HDRM_STATE_APP_PORTRAIT | \
		    HDRM_STATE_NON_COMPOSITED | HDRM_STATE_NON_COMP_PORT)

/* Can we switch to portrait mode? */
#define STATE_IS_PORTRAIT_CAPABLE(s) \
  (STATE_ONE_OF((s), \
   HDRM_STATE_APP | HDRM_STATE_HOME | HDRM_STATE_NON_COMPOSITED) \
   || STATE_IS_EDIT_MODE (s))

#define STATE_IS_PORTRAIT(s) \
  STATE_ONE_OF((s), HDRM_STATE_APP_PORTRAIT | HDRM_STATE_HOME_PORTRAIT | \
               HDRM_STATE_NON_COMP_PORT)

#define STATE_IS_EDIT_MODE(s) \
  STATE_ONE_OF((s), HDRM_STATE_HOME_EDIT | HDRM_STATE_HOME_EDIT_DLG)

/* Are we displaying a loading screen? */
#define STATE_IS_LOADING(s) \
  STATE_ONE_OF((s), HDRM_STATE_LOADING | HDRM_STATE_LOADING_SUBWIN)

#define STATE_SHOW_OPERATOR(s) \
  STATE_ONE_OF((s), HDRM_STATE_HOME | HDRM_STATE_HOME_PORTRAIT)

#define STATE_SHOW_STATUS_AREA(s) \
  STATE_ONE_OF((s), HDRM_STATE_HOME | HDRM_STATE_HOME_PORTRAIT | \
                    HDRM_STATE_APP | HDRM_STATE_APP_PORTRAIT | \
                    HDRM_STATE_LOADING )

/* Show applets in the background? We DON'T show them for apps so
 * the app->task nav transition doesn't show them fading out in
 * the background */
#define STATE_SHOW_APPLETS(s) \
  (!STATE_ONE_OF((s), HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV | \
                      HDRM_STATE_APP | HDRM_STATE_APP_PORTRAIT))

/* Are we in a state where we should blur the buttons + status menu?
 * Task Navigator + launcher zoom out, so are a bad idea. for HOME_EDIT
 * We want to blur stuff, but not our buttons/applets... */
#define STATE_BLUR_BUTTONS(s) \
  (!STATE_ONE_OF((s), HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV | \
                      HDRM_STATE_HOME_EDIT | HDRM_STATE_LOADING | \
                      HDRM_STATE_LOADING_SUBWIN ))

/* are we in a state where we want the toolbar buttons to be out the front?
 * We need this in task_nav (and launcher if the buttons ever come back)
 * because blur_front sits behind task_nav and launcher and they will
 * block the clicks to the toolbar if it is there. */
#define STATE_TOOLBAR_FOREGROUND(s) \
  (STATE_ONE_OF((s), HDRM_STATE_TASK_NAV | HDRM_STATE_LAUNCHER))

/* States to move the home applets out to the front in. We move them
 * out for home_edit so we can drag them, but we also move them for
 * task nav/launcher so we can get the applets to fade nicely into the
 * background */
#define STATE_HOME_FRONT(s) \
  (STATE_ONE_OF((s), HDRM_STATE_HOME_EDIT | HDRM_STATE_HOME_EDIT_DLG | \
                     HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV))

#define STATE_IN_EDIT_MODE(s) \
  (STATE_ONE_OF((s), HDRM_STATE_HOME_EDIT | HDRM_STATE_HOME_EDIT_DLG))

#define STATE_DISCARD_PREVIEW_NOTE(s) \
  (STATE_ONE_OF((s), HDRM_STATE_TASK_NAV))

/* If we have notes then we may want to be able to click on them. If so
 * then we don't want h-d to have a grab over the notification, so subtract
 * them from the grab area */
#define STATE_UNGRAB_NOTES(s) \
  (STATE_ONE_OF((s), HDRM_STATE_APP | HDRM_STATE_HOME | \
                     HDRM_STATE_HOME_EDIT | HDRM_STATE_HOME_EDIT_DLG))

#define STATE_ALLOW_CALL_FROM_HOME(s) \
  (STATE_ONE_OF((s), HDRM_STATE_HOME))

/* The states from which we show CallUI if rotated to portrait. */
#define STATE_SHOW_CALLUI(s) \
  (STATE_ONE_OF((s), HDRM_STATE_HOME | HDRM_STATE_HOME_PORTRAIT | \
                     HDRM_STATE_TASK_NAV | HDRM_STATE_LAUNCHER))

GType hd_render_manager_state_get_type (void) G_GNUC_CONST;
GType hd_render_manager_get_type       (void) G_GNUC_CONST;

HdRenderManager *hd_render_manager_create (HdCompMgr *hdcompmgr,
		                           HdLauncher *launcher,
					   HdHome *home,
					   HdTaskNavigator *task_nav);
HdRenderManager *hd_render_manager_get (void);

void hd_render_manager_set_status_area (ClutterActor *item);
void hd_render_manager_set_status_menu (ClutterActor *item);
void hd_render_manager_set_operator (ClutterActor *item);
void hd_render_manager_set_loading  (ClutterActor *item);
/* ----------------------------------------------------------------- */
ClutterActor *hd_render_manager_get_title_bar(void);
ClutterActor *hd_render_manager_get_status_area(void);
MBWindowManagerClient *hd_render_manager_get_status_area_client(void);

ClutterContainer *hd_render_manager_get_front_group(void);

void hd_render_manager_add_to_front_group(ClutterActor *a);

void hd_render_manager_set_state (HDRMStateEnum state);
HDRMStateEnum hd_render_manager_get_state(void);
HDRMStateEnum hd_render_manager_get_previous_state(void);
void hd_render_manager_set_state_portrait (void);
void hd_render_manager_set_state_unportrait (void);
gboolean hd_render_manager_is_changing_state(void);
const char *hd_render_manager_get_state_str(void);
gboolean hd_render_manager_in_transition(void);
gboolean hd_render_manager_is_client_visible(MBWindowManagerClient *c);
void hd_render_manager_set_launcher_subview(gboolean subview);

void hd_render_manager_return_windows(void);
void hd_render_manager_return_app (ClutterActor *actor);
void hd_render_manager_return_dialog (ClutterActor *actor);

void hd_render_manager_restack(void);
void hd_render_manager_place_titlebar_elements(void);

/* This stops any current transition that render manager is doing */
void hd_render_manager_stop_transition(void);

/* Called by hd-task-navigator when its state changes, as when notifications
 * arrive the button in the top-left may need to change */
void hd_render_manager_update(void);

/* Remove all zooming from home_blur but keep it blurred. */
void hd_render_manager_unzoom_background(void);

/* If something that is blurred has changed, update it. */
void hd_render_manager_blurred_changed(void);

/* Gets the current coordinates of the title. */
void hd_render_manager_get_title_xy (int *x, int *y);

/* Adds an input blocker which grabs the whole screen's input until either a
 * window appears or a timeout expires. We only use this because
 * if the user clicks rapidly then things like home applets can be launched
 * or state changed before the requested window appears. Currently this is
 * only used for STATE_HOME_EDIT, but it's been done like this because
 * it may be usable to solve problems in other states too.  */
void hd_render_manager_add_input_blocker(void);
/* See hd_render_manager_add_input_blocker. This should be called when
 * we don't need the input blocked any more. */
void hd_render_manager_remove_input_blocker(void);

/* Set the input viewport depending on what is currently visible.
 * Should be used sparingly - Only exported for HdTitleBar currently. */
void hd_render_manager_set_input_viewport(void);

/* Rotates the current inout viewport - called on rotate, so we can route
  * events to the right place, even before everything has properly resized. */
void hd_render_manager_flip_input_viewport(void);

/* Check how long ago the last window was mapped. If it was very
 * recent then most likely it was mapped before the dbus signal arrived. */
gboolean hd_render_manager_allow_dbus_launch_transition(void);

gboolean hd_render_manager_actor_is_visible(ClutterActor *actor);

void hd_render_manager_set_visibilities(void);

void hd_render_manager_update_blur_state(void);

G_END_DECLS

#endif /* __HD_RENDER_MANAGER_H__ */
