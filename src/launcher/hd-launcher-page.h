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
  HD_LAUNCHER_PAGE_TRANSITION_IN = 0,
  HD_LAUNCHER_PAGE_TRANSITION_OUT,
  HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK, /* out for topmost menu when the back is over it */
  HD_LAUNCHER_PAGE_TRANSITION_LAUNCH,
  HD_LAUNCHER_PAGE_TRANSITION_IN_SUB, /* in for sub-menu */
  HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB, /* out for sub-menu */
  HD_LAUNCHER_PAGE_TRANSITION_BACK, /* back out as a new menu appears */
  HD_LAUNCHER_PAGE_TRANSITION_FORWARD, /* forwards after a new menu is removed */
} HdLauncherPageTransition;

GType            hd_launcher_page_get_type (void) G_GNUC_CONST;
ClutterActor    *hd_launcher_page_new     (const gchar *icon_name,
                                           const gchar *text);
ClutterActor    *hd_launcher_page_get_grid      (HdLauncherPage *page);
const gchar     *hd_launcher_page_get_icon_name (HdLauncherPage *page);
const gchar     *hd_launcher_page_get_text      (HdLauncherPage *page);
void hd_launcher_page_set_icon_name (HdLauncherPage *page,
                                      const gchar *icon_name);
void hd_launcher_page_set_text      (HdLauncherPage *page,
                                      const gchar *text);

void hd_launcher_page_add_tile (HdLauncherPage *page, HdLauncherTile* tile);
void hd_launcher_page_transition(HdLauncherPage *page,
                                 HdLauncherPageTransition trans_type);
ClutterFixed hd_launcher_page_get_scroll_y(HdLauncherPage *page);

/* Fixed sizes.
 * FIXME: These should come from getting the screen size
 */
#define HD_LAUNCHER_PAGE_WIDTH  (800)
#define HD_LAUNCHER_PAGE_HEIGHT (480)
#define HD_LAUNCHER_PAGE_YMARGIN (70)

G_END_DECLS

#endif /* __HD_LAUNCHER_PAGE_H__ */
