/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Tomas Frydrych <tf@o-hand.com>
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

#ifndef _HAVE_HD_DIALOG_H
#define _HAVE_HD_DIALOG_H

#include <matchbox/core/mb-wm.h>
#include <matchbox/client-types/mb-wm-client-dialog.h>

typedef struct HdDialog      HdDialog;
typedef struct HdDialogClass HdDialogClass;

#define HD_DIALOG(c) ((HdDialog*)(c))
#define HD_DIALOG_CLASS(c) ((HdDialogClass*)(c))
#define HD_TYPE_DIALOG (hd_dialog_class_type ())
#define HD_IS_DIALOG(c) (MB_WM_OBJECT_TYPE(c)==HD_TYPE_DIALOG)

struct HdDialog
{
  MBWMClientDialog  parent;

  unsigned long     release_cb_id;

  /* The height the client asked for last time.  Try to honor it
   * when the layout manager requests a new geometry. */
  unsigned          requested_height;
};

struct HdDialogClass
{
  MBWMClientDialogClass parent;
};

MBWindowManagerClient* hd_dialog_new (MBWindowManager  *wm,
				      MBWMClientWindow *win);

int hd_dialog_class_type (void);

#endif
