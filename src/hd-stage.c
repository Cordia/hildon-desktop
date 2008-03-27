/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-stage.h"

#include <clutter/clutter-stage.h>
#include <clutter/clutter-x11.h>

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

static Window
hd_get_overlay_window (Display *dpy, int screen)
{
  XserverRegion region;
  static Window overlay = None;
  XRectangle rectangle = {0};

  if (overlay != None)
    return overlay;

  overlay = XCompositeGetOverlayWindow (dpy,
                                        RootWindow (dpy, screen));

  XSelectInput (dpy,
                overlay,
                FocusChangeMask |
                ExposureMask |
                PropertyChangeMask |
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask);

  /* FIXME: button size */
  rectangle.width  = 100;
  rectangle.height = 50;
  region = XFixesCreateRegion (dpy, &rectangle, 1);

  XFixesSetWindowShapeRegion (dpy,
                              overlay,
                              ShapeBounding,
                              0, 0,
                              None);

  XFixesSetWindowShapeRegion (dpy,
                              overlay,
                              ShapeInput,
                              0, 0,
                              region);

  XFixesDestroyRegion (dpy, region);

  return overlay;

}

void
hd_stage_grab_pointer (ClutterActor *stage)
{
  Window clutter_window;
  Display *dpy = clutter_x11_get_default_display ();
  int status;

  clutter_window = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

  status = XGrabPointer (dpy,
                         clutter_window,
                         False,
                         ButtonPressMask | ButtonReleaseMask,
                         GrabModeAsync,
                         GrabModeAsync,
                         None,
                         None,
                         CurrentTime);

}

void
hd_stage_ungrab_pointer (ClutterActor *stage)
{
  Display *dpy = clutter_x11_get_default_display ();

  XUngrabPointer (dpy, CurrentTime);

}

ClutterActor *
hd_get_default_stage ()
{
  Display *dpy = clutter_x11_get_default_display ();
  int screen = clutter_x11_get_default_screen ();
  static ClutterActor *stage = NULL;
  Window overlay;

  if (stage != NULL)
    return stage;

  stage = clutter_stage_get_default ();

  overlay = hd_get_overlay_window (dpy, screen);

  if (overlay != None)
    {
      Window clutter_window;

      clutter_actor_realize (stage);
      clutter_actor_set_size (stage,
                              WidthOfScreen  (ScreenOfDisplay (dpy, screen)),
                              HeightOfScreen (ScreenOfDisplay (dpy, screen)));

      clutter_window = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

      XReparentWindow (dpy,
                       clutter_window,
                       overlay,
                       0, 0);

      XSelectInput (dpy,
                    clutter_window,
                    FocusChangeMask |
                    ExposureMask |
                    PointerMotionMask |
                    PropertyChangeMask |
                    ButtonPressMask | ButtonReleaseMask |
                    KeyPressMask | KeyReleaseMask);

      XSync (dpy, False);


    }

  return stage;

}
