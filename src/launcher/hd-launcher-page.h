/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
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
 * A HdLauncherPage displays a category of apps.
 */

#ifndef __HD_LAUNCHER_PAGE_H__
#define __HD_LAUNCHER_PAGE_H__

#include <clutter/clutter-actor.h>
#include "hd-launcher-tile.h"

G_BEGIN_DECLS

#define HD_TYPE_LAUNCHER_PAGE            (hd_launcher_page_get_type ())
#define HD_LAUNCHER_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER_PAGE, HdLauncherPage))
#define HD_IS_LAUNCHER_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER_PAGE))
#define HD_LAUNCHER_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_LAUNCHER_PAGE, HdLauncherPageClass))
#define HD_IS_LAUNCHER_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_PAGE))
#define HD_LAUNCHER_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER_PAGE, HdLauncherPageClass))

typedef struct _HdLauncherPage         HdLauncherPage;
typedef struct _HdLauncherPagePrivate  HdLauncherPagePrivate;
typedef struct _HdLauncherPageClass    HdLauncherPageClass;

struct _HdLauncherPage
{
  ClutterActor parent_instance;

  HdLauncherPagePrivate *priv;
};

struct _HdLauncherPageClass
{
  ClutterActorClass parent_class;
};

typedef enum
{
  HD_LAUNCHER_PAGE_TRANSITION_IN = 0, /* in for main view */
  HD_LAUNCHER_PAGE_TRANSITION_OUT, /* out for main view */
  HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK, /* out for main view when the subview is over it */
  HD_LAUNCHER_PAGE_TRANSITION_LAUNCH, /* launching an application */
  HD_LAUNCHER_PAGE_TRANSITION_IN_SUB, /* in for sub-menu */
  HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB, /* out for sub-menu */
  HD_LAUNCHER_PAGE_TRANSITION_BACK, /* back out the main view as a subview appears */
  HD_LAUNCHER_PAGE_TRANSITION_FORWARD, /* main view forwards after a subview is removed */
} HdLauncherPageTransition;

GType            hd_launcher_page_get_type (void) G_GNUC_CONST;
ClutterActor    *hd_launcher_page_new      (void);
ClutterActor    *hd_launcher_page_get_grid      (HdLauncherPage *page);

void hd_launcher_page_add_tile (HdLauncherPage *page, HdLauncherTile* tile);
void hd_launcher_page_transition(HdLauncherPage *page,
                                 HdLauncherPageTransition trans_type);
void hd_launcher_page_transition_stop(HdLauncherPage *page);
ClutterFixed hd_launcher_page_get_scroll_y(HdLauncherPage *page);
void hd_launcher_page_set_drag_distance(HdLauncherPage *page, float d);
float hd_launcher_page_get_drag_distance(HdLauncherPage *page);

const char *hd_launcher_page_get_transition_string(
                                         HdLauncherPageTransition trans_type);

/* Fixed sizes.
 * FIXME: These should come from getting the screen size
 */
#define HD_LAUNCHER_PAGE_WIDTH  (HD_COMP_MGR_LANDSCAPE_WIDTH)
#define HD_LAUNCHER_PAGE_HEIGHT (HD_COMP_MGR_LANDSCAPE_HEIGHT)
#define HD_LAUNCHER_PAGE_YMARGIN (70)

G_END_DECLS

#endif /* __HD_LAUNCHER_PAGE_H__ */
