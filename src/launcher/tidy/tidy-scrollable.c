/* tidy-scrollable.c: Scrollable interface
 *
 * Copyright (C) 2008 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Chris Lord <chris@openedhand.com>
 */

#include "tidy-scrollable.h"

static void
tidy_scrollable_base_init (gpointer g_iface)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      g_object_interface_install_property (g_iface,
                                   g_param_spec_object ("hadjustment",
                                                        "TidyAdjustment",
                                                        "Horizontal adjustment",
                                                        TIDY_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

      g_object_interface_install_property (g_iface,
                                   g_param_spec_object ("vadjustment",
                                                        "TidyAdjustment",
                                                        "Vertical adjustment",
                                                        TIDY_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

      initialized = TRUE;
    }
}

GType
tidy_scrollable_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      static const GTypeInfo info =
        {
          sizeof (TidyScrollableInterface),
          tidy_scrollable_base_init,        /* base_init */
          NULL,
        };
      type = g_type_register_static (G_TYPE_INTERFACE,
                                     "TidyScrollable", &info, 0);
    }
  return type;
}

void
tidy_scrollable_set_adjustments (TidyScrollable *scrollable,
                                 TidyAdjustment *hadjustment,
                                 TidyAdjustment *vadjustment)
{
  TIDY_SCROLLABLE_GET_INTERFACE (scrollable)->set_adjustments (scrollable,
                                                               hadjustment,
                                                               vadjustment);
}

void
tidy_scrollable_get_adjustments (TidyScrollable *scrollable,
                                 TidyAdjustment **hadjustment,
                                 TidyAdjustment **vadjustment)
{
  TIDY_SCROLLABLE_GET_INTERFACE (scrollable)->get_adjustments (scrollable,
                                                               hadjustment,
                                                               vadjustment);
}

