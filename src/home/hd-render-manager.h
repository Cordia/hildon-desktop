/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
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
#include "hd-comp-mgr.h"
#include "hd-task-navigator.h"
#include "hd-home.h"
#include "../launcher/hd-launcher.h"

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
  ClutterGroup parent_instance;

  HdRenderManagerPrivate *priv;
};

struct _HdRenderManagerClass
{
  ClutterGroupClass parent_class;
};

/* button types that can be given to hd_render_manager_set_button */
typedef enum
{
  HDRM_BUTTON_NONE = 0,
  HDRM_BUTTON_TASK_NAV,
  HDRM_BUTTON_LAUNCHER,
  HDRM_BUTTON_MENU,
  HDRM_BUTTON_HOME_BACK, /* top-right back button shown in home edit view */
  HDRM_BUTTON_LAUNCHER_BACK, /* top-right back button shown in launcher view*/
  HDRM_BUTTON_EDIT, /* pop-down edit */
  HDRM_BUTTON_COUNT = HDRM_BUTTON_EDIT,
} HDRMButtonEnum;

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
  HDRM_STATE_APP            = 1 << 4, /* app frontmost */
  HDRM_STATE_APP_FULLSCREEN = 1 << 5, /* app frontmost and over everything */
  HDRM_STATE_TASK_NAV       = 1 << 6,
  HDRM_STATE_LAUNCHER       = 1 << 7,
  HDRM_STATE_APP_PORTRAIT   = 1 << 8,
} HDRMStateEnum;

/* Does the desktop need to be above apps? */
#define STATE_ONE_OF(state, states) (((state) & (states)) != 0)

#define STATE_NEED_DESKTOP(s) \
  STATE_ONE_OF((s), HDRM_STATE_HOME | HDRM_STATE_HOME_EDIT | \
                    HDRM_STATE_HOME_EDIT_DLG | HDRM_STATE_LAUNCHER)
/* Task Navigator doesn't need the desktop because it grabs all
 * applications anyway */

#define STATE_NEED_GRAB(s) \
  STATE_ONE_OF((s), HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV | \
                    HDRM_STATE_HOME_EDIT)

#define STATE_NEED_TASK_NAV(s) \
  STATE_ONE_OF((s), HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV)

#define STATE_IS_APP(s) \
  STATE_ONE_OF((s), HDRM_STATE_APP | HDRM_STATE_APP_FULLSCREEN | \
                    HDRM_STATE_APP_PORTRAIT)

#define STATE_SHOW_OPERATOR(s) \
  STATE_ONE_OF((s), HDRM_STATE_HOME | HDRM_STATE_HOME_EDIT | \
                    HDRM_STATE_HOME_EDIT_DLG)

/* Whether to show app title or not */
#define STATE_SHOW_TITLE(s) \
  STATE_ONE_OF((s), HDRM_STATE_APP)

#define STATE_SHOW_STATUS_AREA(s) \
  STATE_ONE_OF((s), HDRM_STATE_HOME | HDRM_STATE_APP | \
                    HDRM_STATE_APP_PORTRAIT)

/* Are we in a state where we should blur the buttons + status menu?
 * Task Navigator + launcher zoom out, so are a bad idea. */
#define STATE_BLUR_BUTTONS(s) \
  (!STATE_ONE_OF((s), HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV))

/* Whether to update part of the screen or not.
 * HDRM_STATE_TASK_NAV : Apps are scaled, and scroller is a nightmare
 *                       to deal with.
 * HDRM_STATE_LAUNCHER : Background is zoomed out, so our coordinates
 *                       will be wrong. Also might be showing task switcher
 *                       too! */
#define STATE_DO_PARTIAL_REDRAW(s) \
  (!STATE_ONE_OF((s), HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV))

GType hd_render_manager_state_get_type (void) G_GNUC_CONST;
GType hd_render_manager_get_type       (void) G_GNUC_CONST;

HdRenderManager *hd_render_manager_create (HdCompMgr *hdcompmgr,
		                           HdLauncher *launcher,
		                           ClutterActor *launcher_group,
					   HdHome *home,
					   HdTaskNavigator *task_nav);
HdRenderManager *hd_render_manager_get (void);

void hd_render_manager_set_status_area (ClutterActor *item);
void hd_render_manager_set_status_menu (ClutterActor *item);
void hd_render_manager_set_operator (ClutterActor *item);
void hd_render_manager_set_button (HDRMButtonEnum button,
                                   ClutterActor *item);
/* ----------------------------------------------------------------- */
ClutterActor *hd_render_manager_get_button(HDRMButtonEnum button);
ClutterActor *hd_render_manager_get_title_bar(void);
ClutterActor *hd_render_manager_get_status_area(void);
void hd_render_manager_set_visible(HDRMButtonEnum button, gboolean visible);
gboolean hd_render_manager_get_visible(HDRMButtonEnum button);
void hd_render_manager_update_tasks_button(void);
gboolean hd_render_manager_has_apps(void);

ClutterContainer *hd_render_manager_get_front_group(void);

void hd_render_manager_add_to_front_group(ClutterActor *a);

void hd_render_manager_set_state (HDRMStateEnum state);
HDRMStateEnum hd_render_manager_get_state(void);
gboolean hd_render_manager_is_changing_state(void);
const char *hd_render_manager_get_state_str(void);
gboolean hd_render_manager_in_transition(void);
void hd_render_manager_set_blur_app(gboolean blur);
void hd_render_manager_blur_if_you_need_to(MBWindowManagerClient *c);
gboolean hd_render_manager_is_client_visible(MBWindowManagerClient *c);
void hd_render_manager_set_launcher_subview(gboolean subview);

void hd_render_manager_return_windows(void);
void hd_render_manager_restack(void);
void hd_render_manager_place_titlebar_elements (void);

/* Sets whether any of the buttons will actually be set to do anything */
void hd_render_manager_set_reactive(gboolean reactive);

/* This queues a redraw using a signal set to _CLEANUP. The plan is that this
 * will be processed after all other signals (including the update_area
 * signals), which will allow us to update the full area rather than doing
 * multiple redraws (which flickers and takes ages) */
void hd_render_manager_queue_delay_redraw(void);

/* This stops any current transition that render manager is doing */
void hd_render_manager_stop_transition(void);

/* Called by hd-task-navigator when its state changes, as when notifications
 * arrive the button in the top-left may need to change */
void hd_render_manager_update(void);

G_END_DECLS

#endif /* __HD_RENDER_MANAGER_H__ */
