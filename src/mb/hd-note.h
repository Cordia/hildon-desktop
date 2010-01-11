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

#ifndef _HAVE_HD_NOTE_H
#define _HAVE_HD_NOTE_H

/* We need to make sure %_GNU_SOURCE is not defined because mb-wm.h tries
 * to define it unconditionally.  By including features.h we can have its
 * effects if it indeed was defined. */
#include <features.h>
#undef _GNU_SOURCE

#include <matchbox/core/mb-wm.h>
#include <matchbox/client-types/mb-wm-client-note.h>

typedef struct HdNote      HdNote;
typedef struct HdNoteClass HdNoteClass;

#define HD_NOTE(c) ((HdNote*)(c))
#define HD_NOTE_CLASS(c) ((HdNoteClass*)(c))
#define HD_TYPE_NOTE (hd_note_class_type ())
#define HD_IS_NOTE(c) (MB_WM_OBJECT_TYPE(c)==HD_TYPE_NOTE)

#define HD_IS_BANNER_NOTE       MB_WM_CLIENT_IS_BANNER_NOTE
#define HD_IS_INFO_NOTE         MB_WM_CLIENT_IS_INFO_NOTE
#define HD_IS_CONFIRMATION_NOTE MB_WM_CLIENT_IS_CONFIRMATION_NOTE
#define HD_IS_INCOMING_EVENT_PREVIEW_NOTE  \
  MB_WM_CLIENT_IS_INCOMING_EVENT_PREVIEW_NOTE
#define HD_IS_INCOMING_EVENT_NOTE          \
  MB_WM_CLIENT_IS_INCOMING_EVENT_NOTE

enum
{
  HdNoteSignalChanged = 1,
};

struct HdNote
{
  MBWMClientNote  parent;

  /* For Info:s (hd_util_modal_blocker_realize()) */
  unsigned long   modal_blocker_cb_id;

  /* For Banner:s, Info:s and Confirmation:s,
   * which are sized to fill the screen,
   * to know when to resize. */
  unsigned long   screen_size_changed_cb_id;

  /* For IncomingEvent:s: property cache and signal id.
   * The strings in the cache are X-allocated. */
  char *properties[6];
  unsigned long   property_changed_cb_id;
};

struct HdNoteClass
{
  MBWMClientNoteClass parent;
};

MBWindowManagerClient* hd_note_new (MBWindowManager *wm, MBWMClientWindow *win);
void hd_note_clicked (HdNote *self, void *unused, void *actor);
const char *hd_note_get_destination (HdNote *self);
const char *hd_note_get_message (HdNote *self);
const char *hd_note_get_summary (HdNote *self);
const char *hd_note_get_count (HdNote *self);
const char *hd_note_get_time (HdNote *self);
const char *hd_note_get_icon (HdNote *self);

int hd_note_class_type (void);

#endif
