/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * Copyright (C) 2006 Nokia
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
 */

#ifndef _HAVE_TIDY_HIGHLIGHT_H
#define _HAVE_TIDY_HIGHLIGHT_H

#include <clutter/clutter.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define TIDY_TYPE_HIGHLIGHT (tidy_highlight_get_type ())

#define TIDY_HIGHLIGHT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TIDY_TYPE_HIGHLIGHT, TidyHighlight))

#define TIDY_HIGHLIGHT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TIDY_TYPE_HIGHLIGHT, TidyHighlightClass))

#define TIDY_IS_HIGHLIGHT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TIDY_TYPE_HIGHLIGHT))

#define TIDY_IS_HIGHLIGHT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TIDY_TYPE_HIGHLIGHT))

#define TIDY_HIGHLIGHT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TIDY_TYPE_HIGHLIGHT, TidyHighlightClass))

typedef struct _TidyHighlight        TidyHighlight;
typedef struct _TidyHighlightPrivate TidyHighlightPrivate;
typedef struct _TidyHighlightClass   TidyHighlightClass;

struct _TidyHighlight
{
  ClutterActor                 parent;

  /*< priv >*/
  TidyHighlightPrivate    *priv;
};

struct _TidyHighlightClass
{
  ClutterActorClass parent_class;
};

GType          tidy_highlight_get_type           (void) G_GNUC_CONST;

TidyHighlight *tidy_highlight_new                (ClutterTexture      *texture);
void           tidy_highlight_set_amount(TidyHighlight *sub, float amount);
void           tidy_highlight_set_color (TidyHighlight *sub, ClutterColor *col);

G_END_DECLS

#endif
