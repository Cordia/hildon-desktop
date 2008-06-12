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

#include <matchbox/core/mb-wm.h>
#include <matchbox/client-types/mb-wm-client-note.h>

typedef struct HdNote      HdNote;
typedef struct HdNoteClass HdNoteClass;

#define HD_NOTE(c) ((HdNote*)(c))
#define HD_NOTE_CLASS(c) ((HdNoteClass*)(c))
#define HD_TYPE_NOTE (hd_note_class_type ())
#define HD_IS_NOTE(c) (MB_WM_OBJECT_TYPE(c)==HD_TYPE_NOTE)

typedef enum _HdNoteType
{
  HdNoteTypeBanner        = 0,
  HdNoteTypeInfo,
  HdNoteTypeConfirmation,
}HdNoteType;

struct HdNote
{
  MBWMClientNote  parent;

  HdNoteType      note_type;
};

struct HdNoteClass
{
  MBWMClientNoteClass parent;
};

MBWindowManagerClient* hd_note_new (MBWindowManager *wm, MBWMClientWindow *win);

int hd_note_class_type (void);

#endif
