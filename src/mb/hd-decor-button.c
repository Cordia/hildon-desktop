/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author: Gordon Williams <gordon.williams@collabora.co.uk>
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
#include "hd-decor-button.h"
#include "hd-theme.h"
#include "hd-render-manager.h"
#include "../home/hd-title-bar.h"
#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-wm-util.h>
#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>


static Bool
hd_decor_button_press_handler (MBWMObject       *obj,
                               int               mask,
                               void             *userdata)
{
  MBWMDecorButton *mbbutton = MB_WM_DECOR_BUTTON(obj);
  HdTitleBar *bar = HD_TITLE_BAR(hd_render_manager_get_title_bar());
  if (bar)
    hd_title_bar_right_pressed(bar,
        mbbutton->state != MBWMDecorButtonStateInactive);
  return True;
}

static Bool
hd_decor_button_release_handler (MBWMObject       *obj,
                                 int               mask,
                                 void             *userdata)
{
  MBWMDecorButton *mbbutton = MB_WM_DECOR_BUTTON(obj);
  HdTitleBar *bar = HD_TITLE_BAR(hd_render_manager_get_title_bar());
  if (bar)
    hd_title_bar_right_pressed(bar,
        mbbutton->state != MBWMDecorButtonStateInactive);
  return True;
}

static void
hd_decor_button_class_init (MBWMObjectClass *klass)
{

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdDecorButton";
#endif
}

static void
hd_decor_button_destroy (MBWMObject *obj)
{
}

static int
hd_decor_button_init (MBWMObject *obj, va_list vap)
{
  MBWMObjectProp               prop;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
        {
        default:
          MBWMO_PROP_EAT (vap, prop);
        }
      prop = va_arg(vap, MBWMObjectProp);
    }

  mb_wm_object_signal_connect(obj, MBWMDecorButtonSignalPressed,
      hd_decor_button_press_handler, 0);
  mb_wm_object_signal_connect(obj, MBWMDecorButtonSignalReleased,
      hd_decor_button_release_handler, 0);

  return 1;
}


HdDecorButton* hd_decor_button_new (MBWindowManager               *wm,
                                    MBWMDecorButtonType            type,
                                    MBWMDecorButtonPack            pack,
                                    HdDecor                       *decor,
                                    MBWMDecorButtonPressedFunc     press,
                                    MBWMDecorButtonReleasedFunc    release,
                                    MBWMDecorButtonFlags           flags)
{
  HdDecorButton *decorbutton;

  decorbutton = HD_DECOR_BUTTON(
                  mb_wm_object_new (HD_TYPE_DECOR_BUTTON,
                            MBWMObjectPropWm,                      wm,
                            MBWMObjectPropDecorButtonType,         type,
                            MBWMObjectPropDecorButtonPack,         pack,
                            MBWMObjectPropDecor,                   decor,
                            MBWMObjectPropDecorButtonPressedFunc,  press,
                            MBWMObjectPropDecorButtonReleasedFunc, release,
                            MBWMObjectPropDecorButtonFlags,        flags,
                            NULL));

  return decorbutton;
}

int
hd_decor_button_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdDecorButtonClass),
	sizeof (HdDecorButton),
	hd_decor_button_init,
	hd_decor_button_destroy,
	hd_decor_button_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_DECOR_BUTTON, 0);
    }

  return type;
}

void
hd_decor_button_sync(HdDecorButton *button)
{
  MBWMDecorButton *mbbutton = MB_WM_DECOR_BUTTON(button);
  HdTitleBar *bar;

  bar = HD_TITLE_BAR(hd_render_manager_get_title_bar());
  if (!bar)
    return;

  hd_title_bar_right_pressed(bar,
      mbbutton->state != MBWMDecorButtonStateInactive);
}
