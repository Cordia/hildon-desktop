/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author: Thomas Thurman <thomas.thurman@collabora.co.uk>
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

#include "hd-decor.h"
#include "hd-comp-mgr.h"
#include "hd-app.h"

static void
hd_decor_class_init (MBWMObjectClass *klass)
{
  /* MBWMDecorClass *d_class = MB_WM_DECOR_CLASS (klass); */

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdDecor";
#endif
}

static void
hd_decor_destroy (MBWMObject *obj)
{
  /* he have to do this because hd-theme often adds a callback for us and then
   * doesn't remove it. */
  g_source_remove_by_user_data (obj);
   /* nothing */
}

static int
hd_decor_init (MBWMObject *obj, va_list vap)
{
  HdDecor *d = HD_DECOR (obj);

  d->progress = NULL;

  return 1;
}

HdDecor*
hd_decor_new (MBWindowManager      *wm,
              MBWMDecorType         type)
{
  MBWMObject *decor;

  decor = mb_wm_object_new (HD_TYPE_DECOR,
                            MBWMObjectPropWm,               wm,
                            MBWMObjectPropDecorType,        type,
                            NULL);

  return HD_DECOR(decor);
}

int
hd_decor_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdDecorClass),
	sizeof (HdDecor),
	hd_decor_init,
	hd_decor_destroy,
	hd_decor_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_DECOR, 0);
    }

  return type;
}
