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

#ifndef _HAVE_HD_REMOTE_TEXTURE_H
#define _HAVE_HD_REMOTE_TEXTURE_H

#include <matchbox/core/mb-wm.h>
#include <matchbox/client-types/mb-wm-client-app.h>
#include <tidy/tidy-mem-texture.h>

typedef struct HdRemoteTexture      HdRemoteTexture;
typedef struct HdRemoteTextureClass HdRemoteTextureClass;

#define HD_REMOTE_TEXTURE(c)       ((HdRemoteTexture*)(c))
#define HD_REMOTE_TEXTURE_CLASS(c) ((HdRemoteTextureClass*)(c))
#define HD_TYPE_REMOTE_TEXTURE     (hd_remote_texture_class_type ())
#define HD_IS_REMOTE_TEXTURE(c)    (MB_WM_OBJECT_TYPE(c)==HD_TYPE_REMOTE_TEXTURE)

struct HdRemoteTexture
{
  MBWMClientApp    parent;

  /* Private */
  unsigned long    client_message_handler_id;
  TidyMemTexture  *texture;

  key_t         shm_key;
  guint         shm_width;
  guint         shm_height;
  guint         shm_bpp;
  const guchar *shm_addr;
};

struct HdRemoteTextureClass
{
  MBWMClientAppClass parent;
};

MBWindowManagerClient*
hd_remote_texture_new (MBWindowManager *wm, MBWMClientWindow *win);

int hd_remote_texture_class_type (void);

#endif
