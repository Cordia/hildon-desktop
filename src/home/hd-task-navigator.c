/*
 * {{{
 * Implements a plate shown in task switcher view, which displays
 * the thumbnails of the running applications and incoming events.
 * Note that throughout the code "notification" refers to incoming
 * event notifications.
 *
 * Actor hierarchy:
 * @Navigator                 #ClutterGroup
 *   background               #ClutterRectangle, blurs the home view
 *   @Scroller                #TidyFingerScroll
 *     @Navigator_area        #HdScrollableGroup
 *       @Thumbnails          #ClutterGroup:s
 *       @Notification_area   #ClutterGroup
 *         @Notifications     #ClutterGroup:s
 *
 * Thumbnail.thwin hierarchy:
 *   .prison                  #ClutterGroup
 *     .apwin                 #ClutterActor
 *     .video                 #ClutterTexture
 *   .foreground              #ClutterCloneTexture
 *   .title                   #ClutterLabel or #ClutterGroup
 *    .title_icon             #ClutterTexture
 *    .title_text             #ClutterLabel
 *   .close                   #ClutterCloneTexture
 *   .hibernation             #ClutterCloneTexture
 *
 * TNote.notewin hierarchy:
 *   HdNote::actor            #ClutterActor
 *   close                    #ClutterCloneTexture
 * }}}
*/

/* Include files */
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <tidy/tidy-finger-scroll.h>

#include <matchbox/core/mb-wm.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

#include "hd-atoms.h"
#include "hd-comp-mgr.h"
#include "hd-gtk-utils.h"
#include "hd-task-navigator.h"
#include "hd-scrollable-group.h"

/* Standard definitions {{{ */
#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN "hd-task-navigator"

/* Measures (in pixels).  Unless indicated, none of them is tunable. */
/* Common platform metrics */
#define SCREEN_WIDTH                    800
#define SCREEN_HEIGHT                   480
#define MARGIN_DEFAULT                    8
#define MARGIN_HALF                       4
#define NORMAL_ICON_SIZE                 32

/* Notification window as displayed at the bottom of the navigator.
 * %NOTE_MARGIN is the space to leave between the screen and the
 * window's left/right side. */
#define NOTE_MARGIN                      58
#define NOTE_WIDTH                      342
#define NOTE_HEIGHT                      80
#define NOTE_CLOSE_WIDTH                THWIN_CLOSE_WIDTH

/* These constants descripbe the layout of notifications shown in the
 * title area of a thumbnail.  %TITLE_NOTE_TEXT_MARGIN is the space
 * between the icon and the notification summary. */
#define TITLE_NOTE_ICON_SIZE            NORMAL_ICON_SIZE
#define TITLE_NOTE_TEXT_MARGIN          MARGIN_DEFAULT

/* Application thumbnail dimensions, depending on the number of
 * currently running applications.  %THUMB_3_* also account for
 * the 4-window case and %THUMB_5_* apply to all other cases. */
#define THUMB_1_WIDTH                   376
#define THUMB_1_HEIGHT                  206
#define THUMB_2_WIDTH                   326
#define THUMB_2_HEIGHT                  180
#define THUMB_3_WIDTH                   276
#define THUMB_3_HEIGHT                  153
#define THUMB_5_WIDTH                   228
#define THUMB_5_HEIGHT                  128

/*
 * %THUMB_DFLT_TOP_MARGIN:        Unless there are too many thumbnails
 *                                how much space to leave between the
 *                                first row of thumbnails and the top
 *                                of the screen.
 * %THUMB_DFLT_BOTTOM_MARGIN:     Likewise for the last row of thumbnails
 *                                and the bottom of the screen, provided
 *                                that there're no notifications.
 * %THUMB_MIN_HORIZONTAL_MARGIN:  The minimum amount of space to leave
 *                                between the outer side of the outmost
 *                                thumbnail and the edge of the screen.
 * %THUMB_DFLT_HORIZONTAL_GAP:    In less contrainted circumstances leave
 *                                that much space between thumbnails.
 * %THUMB_DFLT_VERTICAL_GAP:      Likewise.  Both are tunable.
 */
#define THUMB_DFLT_TOP_MARGIN           60
#define THUMB_DFLT_BOTTOM_MARGIN        NOTE_HEIGHT
#define THUMB_MIN_HORIZONTAL_MARGIN     NOTE_MARGIN
#define THUMB_DFLT_HORIZONTAL_GAP       20
#define THUMB_DFLT_VERTICAL_GAP         THUMB_DFLT_HORIZONTAL_GAP

/* Metrics inside a thumbnail. */
#define THWIN_TITLE_BACKGROUND_HEIGHT   47
#define THWIN_TITLE_AREA_LEFT_GAP       47
#define THWIN_TITLE_AREA_RIGHT_GAP      MARGIN_HALF
#define THWIN_TITLE_AREA_BOTTOM_MARGIN  MARGIN_DEFAULT
#define THWIN_TITLE_AREA_HEIGHT         (THWIN_TITLE_BACKGROUND_HEIGHT \
                                         - THWIN_TITLE_AREA_BOTTOM_MARGIN)
#define THWIN_CLOSE_WIDTH               43
#define THWIN_CLOSE_HEIGHT              THWIN_TITLE_BACKGROUND_HEIGHT

/*
 * %EFFECT_LENGTH:                Determines how many miliseconds should it
 *                                take to fly and zoom thumbnails.  Tunable.
 *                                Increase for the better observation of the
 *                                effects or decrase for faster feedback.
 * %VIDEO_SCREENSHOT_DIR:         Where to search for the last-frame video
 *                                screenshots.  If an application has such
 *                                an image it is displayed in switcher mode
 *                                instead of its its thumbnail.  The file is
 *                                "%VIDEO_SCREENSHOT_DIR/<class_hint>.jpg".
 */
#define EFFECT_LENGTH                   1000
#define VIDEO_SCREENSHOT_DIR            "/tmp/fmp/out"

/* Standard definitions }}} */

/* Type definitions {{{ */
/*
 * Contains enough information to lay out the contents of the navigator.
 * Filled by calc_layout() and mostly used by layout().  The layout of
 * the thumbnails is divided into rows, which are laid out similarly,
 * except for the last one, which may contain fewer thumbnails than
 * the others.  All measures are in pixels (save for @cells_per_row).
 */
typedef struct
{
  /*
   * -- @cells_per_row:   How many thumbnails to lay out in a row
   *                      (not in the last row).
   * -- @xpos:            The horizontal position of the leftmost
   *                      thumbnails relative to the @Navigator_area
   *                      (not in the last row).
   * -- @last_row_xpos:   Likewise, but for the last row.
   * -- @ypos:            The vertical position of the topmost
   *                      thumbnails relative to the @Navigator_area.
   * -- @hspace, @vspace: When a thumbnail is placed somewhere don't
   *                      place other thumbnails within this rectangle.
   * -- @wthumb, @hthumb: The desired dimensions of the thumbnails.
   */
  guint cells_per_row;
  guint xpos, last_row_xpos, ypos;
  guint hspace, vspace;
  guint wthumb, hthumb;
} Layout;

/* Provides access to the inner parts of a thumbnail. */
typedef struct
{
  /*
   * -- @thwin:       @Navigator_area's thumbnail window and event responder.
   * -- @prison:      Clips, scales and positions @apwin.
   * -- @foreground:  Scaled decoration created from @Master_foreground
   *                  covering the whole thumbnail.
   *
   * -- @title:       What to put in the thumbnail's title area.
   *                  Anchored at top-middle.
   * -- @title_icon:  If the title is a notification (@hdnote is set)
   *                  then its icon, otherwise %NULL.
   *                  Anchored in the middle.
   * -- @title_text:  If the title is a notification then its summary,
   *                  otherwise %NULL.
   *
   * -- @close:       The cross in the top-right corner created from
   *                  @Master_close.  Anchored in the middle.
   * -- @hibernation: Decoration created from @Master_hibernated to
   *                  indicate hibernated state or %NULL to not apply
   *                  such decoration.  Anchored in the middle.
   *
   * -- @apwin:       The pristine application window, not to be touched.
   *                  Hidden if we have a .video.
   * -- @inapwin:     Delimits the non-decoration area in @apwin; this is
   *                  what we want to show in the navigator, not the whole
   *                  @apwin.
   * -- @parent:      @apwin's original parent we took it away from when we
   *                  entered the navigator.
   * -- @class_hint:  @apwin's XClassHint.res_class (effectively the name
   *                  of the application).  Needs to be XFree()d.
   *
   * -- @video_fname: Where to look for the last-frame video screenshot
   *                  for this application.  Deduced from .class_hint.
   * -- @video_mtime: The last modification time of the image loaded as
   *                  .video.  Used to decide if it should be refreshed.
   * -- @video:       The downsampled texture of the image loaded from
   *                  .video_fname or %NULL.
   *
   * -- @hdnote:                Notification of .apwin if it has one.
   *                            This case .title reflects .hdnote's
   *                            icon and summary.  A %HdNote added to
   *                            the switcher is either in a %Thumbnail
   *                            or in a %TNote, exclusively.
   * -- @hdnote_changed_cb_id:  The ID of the signal handler used to
   *                            track changes in .hdnote if set.
   */
  ClutterActor                *thwin, *prison, *foreground;
  ClutterActor                *title, *title_icon, *title_text;
  ClutterActor                *close, *hibernation;
  ClutterActor                *parent, *apwin, *video;
  const MBGeometry            *inapwin;
  gchar                       *class_hint, *video_fname;
  time_t                       video_mtime;
  HdNote                      *hdnote;
  unsigned long                hdnote_changed_cb_id;
} Thumbnail;

/* Incoming event notification as shown in the @Notification_area. */
typedef struct
{
  /*
   * -- @hdnote:      The notification's client window, which has the
   *                  texture to be shown.
   * -- @destination: Which application @hdnote belongs to;
   *                  needs to be XFree()d.
   * -- @notewin:     @Navigator_area's actor and event responder.
   */
  HdNote                      *hdnote;
  gchar                       *destination;
  ClutterActor                *notewin;
} TNote;

/* Used by add_effect_closure() to store what to call when the effect
 * completes. */
typedef struct
{
  /* @fun(@actor, @funparam) is what is called eventually.
   * @fun is not %NULL, @actor is g_object_ref()ed.
   * @handler_id is the signal handler. */
  ClutterEffectCompleteFunc    fun;
  ClutterActor                *actor;
  gpointer                     funparam;
  gulong                       handler_id;
} EffectClosure;
/* Type definitions }}} */

/* Private variables {{{ */
/*
 * -- @Navigator:         Root group, event responder.
 * -- @Scroller:          Viewport of @Navigation_area and controls
 *                        its scrolling.  Moved and scaled when zooming.
 * -- @Navigator_area:    Contains the complete layout.
 * -- @Notification_area: Contains notifications that are shown at the
 *                        bottom of the switcher in individual windows.
 *                        There may be other notifications shown in the
 *                        title area of thumbnails, but these are not in
 *                        in this container.  Moved vertically.
 */
static HdScrollableGroup *Navigator_area;
static ClutterActor *Navigator, *Scroller, *Notification_area;

/*
 * -- @Thumbnails:    Array of %Thumbnail:s.
 * -- @Notifications: Array of %TNote:s.
 *
 * Order is significant in the arrays.
 */
static GArray *Thumbnails, *Notifications;

/* Common textures for %Thumbnail.foreground, .close and .hybernate. */
static ClutterTexture *Master_close, *Master_hibernated;
static ClutterTexture *Master_foreground;

/*
 * Effect templates and their corresponding timelines.
 * -- @Fly_effect:  for moving thumbnails and notification windows around.
 * -- @Zoom_effect: for zooming in and out of application windows.
 */
static ClutterTimeline *Fly_effect_timeline, *Zoom_effect_timeline;
static ClutterEffectTemplate *Fly_effect, *Zoom_effect;

/* Thumbnail title text (application name or notification) properties. */
static ClutterColor Title_text_color = { .alpha = 0xFF };
static const gchar *Title_text_font  = "Anything but NULL";
/* Private variables }}} */

/* Program code */
/* Graphics loading {{{ */
/* Returns the texture of an invisible (transparent) @width x @height box. */
static ClutterTexture *
empty_texture (guint width, guint height)
{
  guint bpp;
  guchar *rgb;
  ClutterTexture *fake;

  /* clutter_texture_new_from_actor (clutter_rectangle_new())
   * is no good because that cannot be cloned reliably. */
  bpp = 4; // Set it to 3 to get a visible black box.
  rgb = g_new0 (guchar, width*height*bpp);
  fake = CLUTTER_TEXTURE (clutter_texture_new ());
  clutter_texture_set_from_rgb_data (fake, rgb, bpp == 4,
                                     width, height,
                                     width*bpp, bpp,
                                     0, NULL);
  g_free (rgb);
  return fake;
}

/* Destroying @pixbuf, turns it into a @ClutterTexture.
 * On failure creates a fake rectangle with the same size
 * or if @fallback is disabled just returns %NULL. */
static ClutterTexture *
pixbuf2texture (GdkPixbuf *pixbuf, gboolean fallback)
{
  GError *err;
  gboolean isok;
  ClutterTexture *texture;

#ifndef G_DISABLE_CHECKS
  if (gdk_pixbuf_get_colorspace (pixbuf) != GDK_COLORSPACE_RGB
      || gdk_pixbuf_get_bits_per_sample (pixbuf) != 8
      || gdk_pixbuf_get_n_channels (pixbuf) !=
         (gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3))
    {
      g_critical("image not in expected rgb/8bps format");
      goto damage_control;
    }
#endif

  err = NULL;
  texture = CLUTTER_TEXTURE (clutter_texture_new ());
  isok = clutter_texture_set_from_rgb_data (texture,
                                            gdk_pixbuf_get_pixels (pixbuf),
                                            gdk_pixbuf_get_has_alpha (pixbuf),
                                            gdk_pixbuf_get_width (pixbuf),
                                            gdk_pixbuf_get_height (pixbuf),
                                            gdk_pixbuf_get_rowstride (pixbuf),
                                            gdk_pixbuf_get_n_channels (pixbuf),
                                            0, &err);
  if (!isok)
    {
      g_warning ("clutter_texture_set_from_rgb_data: %s", err->message);
      g_object_unref (texture);
damage_control:
      texture = fallback
        ? empty_texture (gdk_pixbuf_get_width (pixbuf),
                         gdk_pixbuf_get_height (pixbuf))
        : NULL;
    }

  g_object_unref (pixbuf);
  return texture;
}

/* Returns a texture showing an icon at size @isize anchored in the middle.
 * If the loaded icon is larger than @isize x @isize it is scaled down.
 * If @iname is %NULL or there is an error returns an empty rectangle. */
static ClutterTexture *
load_icon (const gchar * iname, guint isize)
{
  guint w, h;
  GError *err;
  GdkPixbuf *pixbuf;
  ClutterTexture *texture;

  if (!iname)
    return empty_texture (isize, isize);

  err = NULL;
  if (!(pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                           iname, isize, 0, &err)))
    {
      g_warning ("gtk_icon_theme_load_icon(%s): %s", iname, err->message);
      return empty_texture (isize, isize);
    }
  else

  /* Scale @texture down if it's larger than desired.
   * TODO Unused right now. */
  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);
  if (w > isize || h > isize)
    {
      GdkPixbuf *p;

      p = h >= w
        ? gdk_pixbuf_scale_simple (pixbuf, isize, isize * h/w,
                                   GDK_INTERP_NEAREST)
        : gdk_pixbuf_scale_simple (pixbuf, isize * w/h, isize,
                                   GDK_INTERP_NEAREST);
      g_object_unref (pixbuf);
      pixbuf = p;
    }

  texture = pixbuf2texture (pixbuf, TRUE);
  clutter_actor_set_anchor_point_from_gravity (CLUTTER_ACTOR (texture),
                                               CLUTTER_GRAVITY_CENTER);

  return texture;
}

/* Returns the texture of @fname as is or a fake one on error. */
static ClutterTexture *
load_image (const gchar * fname)
{
  GError *err;
  GdkPixbuf *pixbuf;

  if (!fname)
    return empty_texture (SCREEN_WIDTH, SCREEN_HEIGHT);

  err = NULL;
  if (!(pixbuf = gdk_pixbuf_new_from_file (fname, &err)))
    {
      g_warning ("%s: %s", fname, err->message);
      return empty_texture (SCREEN_WIDTH, SCREEN_HEIGHT);
    }
  else
    return pixbuf2texture (pixbuf, TRUE);
}

/*
 * Loads @fname, resizing and cropping it as necessary to fit
 * in a @aw x @ah rectangle.  Contrary to load_image() it doesn't
 * fall back creating an empty rectangle on error but simply
 * returns %NULL.
 */
static ClutterActor *
load_image_fit(char const * fname, guint aw, guint ah)
{
  GError *err;
  GdkPixbuf *pixbuf;
  gint dx, dy;
  gdouble dsx, dsy, scale;
  guint vw, vh, sw, sh, dw, dh;
  ClutterActor *final;
  ClutterTexture *texture;

  /* On error the caller sure has better recovery plan than an
   * empty rectangle.  (ie. showing the real application window). */
  err = NULL;
  if (!(pixbuf = gdk_pixbuf_new_from_file (fname, &err)))
    {
      g_warning ("%s: %s", fname, err->message);
      return NULL;
    }

  /* @sw, @sh := size in pixels of the untransformed image. */
  sw = gdk_pixbuf_get_width (pixbuf);
  sh = gdk_pixbuf_get_height (pixbuf);

  /*
   * @vw and @wh tell how many pixels should the image have at most
   * in horizontal and vertical dimensions.  If the image would have
   * more we will scale it down before we create its #ClutterTexture.
   * This is to reduce texture memory consumption.
   */
  vw = aw / 2;
  vh = ah / 2;

  /*
   * Detemine if we need to and how much to scale @pixbuf.  If the image
   * is larger than requested (@aw and @ah) then scale to the requested
   * amount but do not scale more than @vw/@aw ie. what the memory saving
   * would demand.  Instead crop it later.  If one direction needs more
   * scaling than the other choose that factor.
   */
  dsx = dsy = 1;
  if (sw > vw)
    dsx = (gdouble)vw / MIN (aw, sw);
  if (sh > vh)
    dsy = (gdouble)vh / MIN (ah, sh);
  scale = MIN (dsx, dsy);

  /* If the image is too large (even if we scale it) crop the center.
   * These are the final parameters to gdk_pixbuf_scale().
   * @dw, @dh := the final pixel width and height. */
  dx = dy = 0;
  dw = sw * scale;
  dh = sh * scale;

  if (dw > vw)
    {
      dx = -(gint)(dw - vw) / 2;
      dw = vw;
    }

  if (dh > vh)
    {
      dy = -(gint)(dh - vh) / 2;
      dh = vh;
    }

  /* Crop and scale @pixbuf if we need to. */
  if (scale < 1)
    {
      GdkPixbuf *tmp;

      /* Make sure to allocate the new pixbuf with the same
       * properties as the old one has, gdk_pixbuf_scale()
       * may not like it otherwise. */
      tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                            gdk_pixbuf_get_has_alpha(pixbuf),
                            gdk_pixbuf_get_bits_per_sample(pixbuf),
                            dw, dh);
      gdk_pixbuf_scale (pixbuf, tmp, 0, 0,
                        dw, dh, dx, dy, scale, scale,
                        GDK_INTERP_BILINEAR);
      g_object_unref (pixbuf);
      pixbuf = tmp;
    }

  if (!(texture = pixbuf2texture(pixbuf, FALSE)))
    return NULL;

  /* If @pixbuf is smaller than desired place it centered
   * on a @vw x @vh size black background. */
  if (dw < vw || dh < vh)
    {
      static const ClutterColor bgcolor = { 0x00, 0x00, 0x00, 0xFF };
      ClutterActor *bg;

      bg = clutter_rectangle_new_with_color (&bgcolor);
      clutter_actor_set_size (bg, vw, vh);

      if (dw < vw)
        clutter_actor_set_x (CLUTTER_ACTOR (texture), (vw - dw) / 2);
      if (dh < vh)
        clutter_actor_set_y (CLUTTER_ACTOR (texture), (vh - dh) / 2);

      final = clutter_group_new ();
      clutter_container_add (CLUTTER_CONTAINER (final),
                             bg, texture, NULL);
    }
  else
    final = CLUTTER_ACTOR (texture);

  /* @final is @vw x @vh large, make it appear as if it @aw x @ah. */
  clutter_actor_set_scale (final, (gdouble)aw/vw, (gdouble)ah/vh);

  return final;
}
/* Graphics loading }}} */

/* Navigator utilities {{{ */
/* Tells whether we're in the navigator view. */
gboolean
hd_task_navigator_is_active (HdTaskNavigator * self)
{
  return CLUTTER_ACTOR_IS_VISIBLE (self);
}

/* Tells whether the navigator (@Navigator_area) is empty. */
gboolean
hd_task_navigator_is_empty (HdTaskNavigator * self)
{
  return !Thumbnails->len && !Notifications->len;
}

/* Returns whether we can and will show @win in the navigator.
 * @win can be %NULL, in which case this function returns %FALSE. */
gboolean
hd_task_navigator_has_window (HdTaskNavigator * self, ClutterActor * win)
{
  guint i;

  for (i = 0; i < Thumbnails->len; i++)
    if (g_array_index (Thumbnails, Thumbnail, i).apwin == win)
      return TRUE;
  return FALSE;
}

/* Enters the navigator without animation. */
void
hd_task_navigator_enter (HdTaskNavigator * self)
{
  /* Reset the position of @Scroller, which may have been changed when
   * we zoomed in last time.  Also scroll @Navigator_area to the top. */
  clutter_actor_set_scale (Scroller, 1, 1);
  clutter_actor_set_position (Scroller, 0, 0);
  hd_scrollable_group_set_viewport_y (Navigator_area, 0);
  clutter_actor_show (Navigator);
}

/* Leaves the navigator without a single word.
 * It's up to you what's left on the screen. */
void
hd_task_navigator_exit (HdTaskNavigator *self)
{
  clutter_actor_hide (CLUTTER_ACTOR (self));
}

/*
 * Returns the height of the @Notification_area in pixels.
 * It's like clutter_actor_get_height() but that function
 * is not reliable always due to clutter's allocation and
 * layout mechanisms.  In a sense this function returns
 * the intended height of the area, which we know for sure.
 */
static guint
notes_height (void)
{
  guint note_rows;

  note_rows = Notifications->len / 2 + Notifications->len % 2;
  return NOTE_HEIGHT * note_rows;
}

/* Misnamed function returning the expected height of the @Navigator_area. */
static guint
navigator_height (void)
{
  /* We assume the vertical position of @Notification_area
   * is always correct. */
  return clutter_actor_get_y (Notification_area) + notes_height ();
}

/* Updates our #HdScrollableGroup's idea about @Navigator_area's height. */
static void
set_navigator_height (guint hnavigator)
{
  hd_scrollable_group_set_real_estate (Navigator_area,
                                       HD_SCROLLABLE_GROUP_VERTICAL,
                                       hnavigator);
}
/* Navigator utilities }}} */

/* Clutter utilities {{{ */
/* add_effect_closure()'s #ClutterTimeline::completed handler. */
static gboolean
call_effect_closure(ClutterTimeline * timeline, EffectClosure *closure)
{
  g_signal_handler_disconnect (timeline, closure->handler_id);
  closure->fun (closure->actor, closure->funparam);
  g_object_unref (closure->actor);
  g_slice_free (EffectClosure, closure);
  return FALSE;
}

/* If @fun is not %NULL call it with @actor and @funparam when
 * @timeline is "completed".  Otherwise NOP. */
static void
add_effect_closure(ClutterTimeline * timeline,
                   ClutterEffectCompleteFunc fun,
                   ClutterActor * actor, gpointer funparam)
{
  EffectClosure *closure;

  if (!fun)
    return;

  closure = g_slice_new (EffectClosure);
  closure->fun        = fun;
  closure->actor      = g_object_ref (actor);
  closure->funparam   = funparam;
  closure->handler_id = g_signal_connect (timeline, "completed",
                                          G_CALLBACK (call_effect_closure),
                                          closure);
}

/* #ClutterEffectCompleteFunc of show_when_complete(). */
static void
show_newborn (ClutterActor * newborn, gpointer unused)
{
  clutter_actor_show (newborn);
}

/* Hide @actor and only show it after the flying animation finished.
 * Used when a new thing is created but you need to move other things
 * out of the way to make room for it. */
static void
show_when_complete (ClutterActor * actor)
{
  clutter_actor_hide (actor);
  add_effect_closure (Fly_effect_timeline, show_newborn, actor, NULL);
}

/* Returns whether we're doing any flying animation. */
static gboolean
is_flying (void)
{
  /*
   * The template is referenced by every effect,
   * therefore if it's not referenced by anyone
   * we're not flying.
   */
  return G_OBJECT (Fly_effect)->ref_count > 1;
}

/* Tells whether we should bother with animation or just setting the
 * @actor's property is enough. */
static gboolean
need_to_animate (ClutterActor * actor)
{
  if (!hd_task_navigator_is_active (HD_TASK_NAVIGATOR (Navigator)))
    /* Navigator is not visible, animation wouldn't be seen. */
    return FALSE;
  else if (CLUTTER_IS_GROUP (actor))
    /* Don't fly empty groups. */
    return clutter_group_get_n_children (CLUTTER_GROUP (actor)) > 0;
  else
    return TRUE;
}

/*
 * Translates @actor to @xpos and @ypos either smoothly or not, depending on
 * the circumstances.   Use when you know the new coordinates are different
 * from then current ones.  Returns whether the translation will be animated.
 */
static gboolean
move (ClutterActor * actor, gint xpos, gint ypos)
{
  if (need_to_animate (actor))
    {
      clutter_effect_move (Fly_effect, actor, xpos, ypos, NULL, NULL);
      return TRUE;
    }
  else
    {
      clutter_actor_set_position (actor, xpos, ypos);
      return FALSE;
    }
}

/* Like move(), except that it does nothing if @actor's current coordinates
 * are the same as the new ones.  Used to make sure no animation takes effect
 * in such a case. */
static gboolean
check_and_move (ClutterActor * actor, gint xpos_new, gint ypos_new)
{
  gint xpos_now, ypos_now;

  clutter_actor_get_position (actor, &xpos_now, &ypos_now);
  return xpos_now != xpos_new || ypos_now != ypos_new
    ? move (actor, xpos_new, ypos_new) : FALSE;
}

/* Like move() but scales @actor instead of moving it. */
static gboolean
scale (ClutterActor * actor, gdouble xscale, gdouble yscale)
{
  if (need_to_animate (actor))
    {
      clutter_effect_scale (Fly_effect, actor, xscale, yscale, NULL, NULL);
      return TRUE;
    }
  else
    {
      clutter_actor_set_scale (actor, xscale, yscale);
      return FALSE;
    }
}

/* Like check_and_move() with respect to move(). */
static gboolean
check_and_scale (ClutterActor * actor, gdouble sx_new, gdouble sy_new)
{
  gdouble sx_now, sy_now;

  clutter_actor_get_scale (actor, &sx_now, &sy_now);
  return sx_now != sx_new || sy_now != sy_new
    ? scale (actor, sx_new, sy_new) : FALSE;
}

/* Moves @actor between #ClutterContainer:s safely. */
static void
reparent (ClutterActor * actor, ClutterActor * new_parent,
          ClutterActor * old_parent)
{
  /* Removing an actor from a group unreferences it.
   * Make sure it's not destroyed accidentally. */
  g_object_ref (actor);
  clutter_container_remove_actor (CLUTTER_CONTAINER (old_parent), actor);
  clutter_container_add_actor (CLUTTER_CONTAINER (new_parent), actor);
  g_object_unref (actor);
}
/* Clutter utilities }}} */

/* Layout engine {{{ */
/*
 * Returns the desired size of the thumbnails in pixels.  This depends
 * solely on the number of thumbnails currently in the navigator.
 * Either of the return arguments can be %NULL if you're not interested
 * in that dimension.
 */
static void
thumb_size (guint * wthumbp, guint * hthumbp)
{
  guint wthumb, hthumb;

  if (Thumbnails->len <= 1)
    {
      wthumb = THUMB_1_WIDTH;
      hthumb = THUMB_1_HEIGHT;
    }
  else if (Thumbnails->len <= 2)
    {
      wthumb = THUMB_2_WIDTH;
      hthumb = THUMB_2_HEIGHT;
    }
  else if (Thumbnails->len <= 4)
    {
      wthumb = THUMB_3_WIDTH;
      hthumb = THUMB_3_HEIGHT;
    }
  else
    {
      wthumb = THUMB_5_WIDTH;
      hthumb = THUMB_5_HEIGHT;
    }

  if (wthumbp)
    *wthumbp = wthumb;
  if (hthumbp)
    *hthumbp = hthumb;
}

/* Returns the top-middle coordinates of the title area of
 * a thumbnail of width @wthumb.  This is used to position
 * the title material, which is anchored at the same point. */
static guint
title_area_pos (guint wthumb)
{
  return (wthumb - (- THWIN_TITLE_AREA_LEFT_GAP
                    + THWIN_TITLE_AREA_RIGHT_GAP
                    + THWIN_CLOSE_WIDTH)) / 2;
}

/*
 * Lays out @Thumbnails on @Navigator_area, and their inner portions.
 * Makes actors fly if it's appropriate.  @newborn is either a new thumbnail
 * or notification to be displayed; it won't be animated.  Returns the
 * position of the bottom of the lowest thumbnail, which is essentially
 * a boundary of this thumbnail area and is used to decide where to place
 * the notification area.
 */
static guint
layout_thumbs (const Layout * lout, ClutterActor * newborn)
{
  gdouble sxfg, syfg;
  const Thumbnail *thumb;
  guint wbg, hbg, xthumb, ythumb, i;

  /* Scale .foreground to the exact size of the thumbnail. */
  clutter_actor_get_size (CLUTTER_ACTOR (Master_foreground), &wbg, &hbg);
  sxfg = (gdouble) lout->wthumb / wbg;
  syfg = (gdouble) lout->hthumb / hbg;

  /* Place and scale each thumbnail row by row. */
  xthumb = ythumb = 0xB002E;
  for (i = 0; i < Thumbnails->len; i++)
    {
      gdouble sxprison, syprison;

      /* If it's a new row re/set @ythumb and @xthumb. */
      thumb = &g_array_index (Thumbnails, Thumbnail, i);
      if (!(i % lout->cells_per_row))
        {
          if (i == 0)
            /* This is the very first row. */
            ythumb = lout->ypos;
          else
            ythumb += lout->vspace;

          /* Use @last_row_xpos if it's the last row. */
          xthumb = i + lout->cells_per_row <= Thumbnails->len
            ? lout->xpos : lout->last_row_xpos;
        }

      /* .prison is centered in the thumbnail, %MARGIN_DEFAULT
       * at both sides. */
      sxprison = (gdouble)(lout->wthumb - 2*MARGIN_DEFAULT)
        / thumb->inapwin->width;
      syprison = (gdouble)(lout->hthumb - 2*MARGIN_DEFAULT)
        / thumb->inapwin->height;

      /* Set the reaction area of .thwin. */
      clutter_actor_set_size (thumb->thwin, lout->wthumb, lout->hthumb);

      /* Keep .close right justified. */
      if (thumb->thwin != newborn)
        { /* @thwin's been there, we may animate its moving. */
          check_and_move (thumb->thwin, xthumb, ythumb);
          check_and_scale (thumb->foreground, sxfg, syfg);
          check_and_scale (thumb->prison, sxprison, syprison);
          check_and_move (thumb->title, title_area_pos (lout->wthumb), 0);
          check_and_move (thumb->close, /* Anchored in the middle. */
                          lout->wthumb - THWIN_CLOSE_WIDTH / 2,
                          THWIN_CLOSE_HEIGHT / 2);
        }
      else
        { /* @thwin is a new one to enter the navigator,
           * don't animate it (it's hidden anyway). */
          clutter_actor_set_position (thumb->thwin, xthumb, ythumb);
          clutter_actor_set_scale (thumb->foreground, sxfg, syfg);
          clutter_actor_set_scale (thumb->prison, sxprison, syprison);
          clutter_actor_set_position (thumb->title,
                                      title_area_pos (lout->wthumb), 0);
          clutter_actor_set_position (thumb->close,
                                      lout->wthumb - THWIN_CLOSE_WIDTH / 2,
                                      THWIN_CLOSE_HEIGHT / 2);
        }

      xthumb += lout->hspace;
    }

  return ythumb + lout->hthumb;
}

/*
 * Utility mathematical function used in layout calculation.
 * Usually there are a group of even-sized things with uniform
 * gaps in between and you want to know where to place it in
 * a larger container.  In this case @total is the size of
 * that container, @factor is the number of things, @term1
 * is their size and is the amont of gap to leave between.
 *
 * Since this function only depends on its arguments and has
 * no side effects it can be declared "const", which makes it
 * possible subject to common subexpression evaluation by the
 * compiler.
 */
static gint __attribute__ ((const))
layout_fun (gint total, gint term1, gint term2, gint factor)
{
  /* Make sure all terms and factors are int:s because the result of
   * the outer subtraction can be negative and division is sensitive
   * to signedness. */
  return (total - (term1 * factor + term2 * (factor - 1))) / 2;
}

/* Calculates the layout of the thumbnails and fills in @lout.
 * The layout depends on the number of thumbnails in the navigator
 * and the presence of notifications in the notification area. */
static void
calc_layout (Layout * lout)
{
  guint xgap, ygap;
  guint nrows, cells_per_last_row;

  /* Get the desired size of the thumbnails. */
  thumb_size (&lout->wthumb, &lout->hthumb);

  /* Figure out how many thumbnails to squeeze into one row
   * (not the last one, which may be different). */
  if (Thumbnails->len <= 1)
    lout->cells_per_row = 1;
  else if (Thumbnails->len <= 4)
    lout->cells_per_row = 2;
  else
    lout->cells_per_row = 3;

  /* Calculate the horizontal position of the leftmost thumbnails
   * (not in the last row) and the gaps between thumbnails. */
  if (Thumbnails->len <= 4)
    {
      /* There are not many thumbnails, so we have more freedom to choose
       * the margin and the gaps.  Since we don't like very large gaps,
       * fix it arbitrarily and infer @xpos from that. */
      xgap = Thumbnails->len <= 1 ? 0 : THUMB_DFLT_HORIZONTAL_GAP;
      ygap = Thumbnails->len <= 2 ? 0 : THUMB_DFLT_VERTICAL_GAP;
      lout->xpos = layout_fun (SCREEN_WIDTH, lout->wthumb, xgap,
                               lout->cells_per_row);
    }
  else
    {
      /* There are many thumbnails and we need to maintain a
       * minimal width of margins.  Infer the size of the gaps
       * (which, apparently will be 0 in the end). */
      lout->xpos = THUMB_MIN_HORIZONTAL_MARGIN;
      xgap = layout_fun (SCREEN_WIDTH, lout->wthumb, lout->xpos,
                         lout->cells_per_row);
      ygap = xgap;
    }

  /* Calculate the horizontal position of the leftmost cell
   * in the last row. */
  nrows = Thumbnails->len / lout->cells_per_row;
  cells_per_last_row = Thumbnails->len % lout->cells_per_row;
  if (cells_per_last_row > 0)
    {
      lout->last_row_xpos = layout_fun (SCREEN_WIDTH,
                                        lout->wthumb, xgap,
                                        cells_per_last_row);
      nrows++;
    }
  else /* The last row has as many thumbnails as the others. */
    lout->last_row_xpos = lout->xpos;

  /* Calculate the vertical position of the first row. */
  if (nrows <= 3)
    { /* We've got up to 9 thumbnails. */
      gint ab;

      /* Leave space for the notification area if it's not empty,
       * otherwise try not to cover the status area. */
      if (!Notifications->len)
        ab = THUMB_DFLT_TOP_MARGIN;
      else if (nrows <= 2)
        ab = THUMB_DFLT_TOP_MARGIN - THUMB_DFLT_BOTTOM_MARGIN;
      else
        ab = -THUMB_DFLT_BOTTOM_MARGIN;

      lout->ypos = layout_fun (SCREEN_HEIGHT + ab, lout->hthumb, ygap, nrows);
    }
  else
    /* There are too many thumbnails to fit on the screen,
     * so it's pointless to leave a vertical margin. */
    lout->ypos = 0;

  lout->hspace = lout->wthumb + xgap;
  lout->vspace = lout->hthumb + ygap;
}

/* Lays out the @Thumbnails and the @Notification_area in @Navigator_area. */
static void
layout (ClutterActor * newborn)
{
  Layout lout;
  guint hnavigator;

  /* This layout machinery is based on invariants, which basically
   * means we don't pay much attention to what caused the layout
   * update, but we rely on the current state of matters. */
  calc_layout (&lout);
  hnavigator = layout_thumbs (&lout, newborn);

  /*
   * Find the appropriate vertical position for the notification area
   * and place it there.  Adding new thumbnails or removing the last
   * notification may make it necessary to relicate the area.
   * (Naturally we cannot use navigator_height() because we're
   * just about to move the @Notification_area.)
   */
  if (Thumbnails->len <= 9)
    /* This case there's no question about the placement of
     * the notification area. */
    hnavigator = SCREEN_HEIGHT - NOTE_HEIGHT;
  else if (Notifications->len > 0)
    /*
     * If there are more than 9 thumbnails, leave a default gap
     * just above the notification area unless it's empty.
     * Otherwise don't leave any gap because the position of the
     * notification are determines the height of the navigator.
     */
    hnavigator += THUMB_DFLT_VERTICAL_GAP;

  check_and_move (Notification_area, 0, hnavigator);
  hnavigator += notes_height ();
  set_navigator_height (hnavigator);

  if (newborn && is_flying ())
    show_when_complete (newborn);
}
/* Layout engine }}} */

/* Child adoption {{{ */
/* Returns whether the application represented by @thumb has
 * a video screenshot and it should be loaded or reloaded.
 * If so it refreshes @thumb->video_mtime. */
static gboolean
need_to_load_video (Thumbnail * thumb)
{
  /* Already has a video loaded? */
  if (thumb->video)
    {
      struct stat sbuf;
      gboolean clear_video, load_video;

      /* Refresh or unload it? */
      load_video = clear_video = FALSE;
      g_assert (thumb->video_fname);
      if (stat (thumb->video_fname, &sbuf) < 0)
        {
          if (errno != ENOENT)
            g_warning ("%s: %m", thumb->video_fname);
          clear_video = TRUE;
        }
      else if (sbuf.st_mtime > thumb->video_mtime)
        {
          clear_video = load_video = TRUE;
          thumb->video_mtime = sbuf.st_mtime;
        }

      if (clear_video)
        {
          clutter_container_remove_actor (CLUTTER_CONTAINER (thumb->prison),
                                          thumb->video);
          thumb->video = NULL;
        }

      return load_video;
    }
  else if (thumb->video_fname)
    {
      struct stat sbuf;

      /* Do we need to load it? */
      if (stat(thumb->video_fname, &sbuf) == 0)
        {
          thumb->video_mtime = sbuf.st_mtime;
          return TRUE;
        }
      else if (errno != ENOENT)
        g_warning ("%s: %m", thumb->video_fname);
    }

  return FALSE;
}

/* Start managing @thumb's application window and loads/reloads
 * its last-frame video screenshot if necessary. */
static void
claim_win (Thumbnail * thumb)
{
  g_assert (hd_task_navigator_is_active (HD_TASK_NAVIGATOR (Navigator)));

  /*
   * Take @thumb->apwin into our care even if there is a video screenshot.
   * If we don't @thumb->apwin will be managed by its current parent and
   * we cannot force hiding it which would result in a full-size apwin
   * appearing in the background if it's added in switcher mode.
   */
  thumb->parent = clutter_actor_get_parent (thumb->apwin);
  reparent (thumb->apwin, thumb->prison, thumb->parent);

  /* Load the video screenshot and place its actor in the hierarchy. */
  if (need_to_load_video (thumb))
    {
      g_assert (!thumb->video);
      thumb->video = load_image_fit (thumb->video_fname,
                                     thumb->inapwin->width,
                                     thumb->inapwin->height);
      if (thumb->video)
        {
          clutter_actor_set_position (thumb->video,
                                      thumb->inapwin->x,
                                      thumb->inapwin->y);
          clutter_container_add_actor (CLUTTER_CONTAINER (thumb->prison),
                                       thumb->video);
        }
    }

  if (!thumb->video)
    {
      /* Show @apwin just in case it isn't.  Make sure it doesn't hide
       * our decoration.  Let the thumbnail window steal the application
       * window's clicks, so we can zoom in. */
      clutter_actor_show (thumb->apwin);
      clutter_actor_set_reactive (thumb->apwin, FALSE);
    }
  else
    /* Only show @thumb->video. */
    clutter_actor_hide (thumb->apwin);
}

/* Stop managing @thumb's application window and give it back
 * to its original parent. */
static void
release_win (const Thumbnail * thumb)
{
  /*
   * It would be important to hide before reparenting the application window
   * otherwise it would remain shown.  But this is not the case it seems.
   * clutter_actor_hide (thumb->apwin);
   */
  reparent (thumb->apwin, thumb->parent, thumb->prison);
}
/* Child adoption }}} */

/* Managing @Thumbnails {{{ */
/* Return the %Thumbnail whose thumbnail window is @thwin. */
static Thumbnail *
find_by_thwin (ClutterActor * thwin)
{
  guint i;
  Thumbnail *thumb;

  for (i = 0; i < Thumbnails->len; i++)
    {
      thumb = &g_array_index (Thumbnails, Thumbnail, i);
      if (thumb->thwin == thwin)
        return thumb;
    }

  g_critical("find_by_thwin(%p): thwin not found", thwin);
  return NULL;
}

/* Return the %Thumbnail whose application window is @apwin. */
static Thumbnail *
find_by_apwin (ClutterActor * apwin)
{
  guint i;
  Thumbnail *thumb;

  for (i = 0; i < Thumbnails->len; i++)
    {
      thumb = &g_array_index (Thumbnails, Thumbnail, i);
      if (thumb->apwin == apwin)
        return thumb;
    }

  g_critical("find_by_apwin(%p): apwin not found", apwin);
  return NULL;
}
/* Managing @Thumbnails }}} */

/* Setting a thumbnail's title {{{ */
static MBWMClientWindow *actor_to_client_window (ClutterActor *win);

/* Sets or replaces @thumb's title.  Its exact position is expected
 * to be set by layout_thumbs() later. */
static void
set_thumb_title (Thumbnail * thumb, ClutterActor * title)
{
  /* Anchor @title at the bottom-middle. */
  clutter_actor_set_anchor_point_from_gravity (title,
                                               CLUTTER_GRAVITY_NORTH);
  clutter_container_add_actor (CLUTTER_CONTAINER (thumb->thwin), title);

  if (thumb->title)
    {
      guint wthumb;

      /* Either called from hd_task_navigator_add_notification()
       * or *_remove_notification(). */

      thumb_size (&wthumb, NULL);
      clutter_actor_set_x (title, title_area_pos (wthumb));

      /* TODO Some fading effect later. */
      clutter_container_raise_child (CLUTTER_CONTAINER (thumb->thwin),
                                     title, thumb->title);
      clutter_container_remove_actor (CLUTTER_CONTAINER (thumb->thwin),
                                      thumb->title);
    }
  else
    /* New window, layout_thumbs() will position it correctly.
     * It's layer is alright on the top (for now). */;

  thumb->title = title;
}

/* Returns a styled #ClutterLabel for @text. */
static ClutterActor *
thumb_title_text (const gchar *text)
{
  return clutter_label_new_full (Title_text_font, text, &Title_text_color);
}

/*
 * Reset @thumb's title to the application's name.  Called to set the
 * initial title of a thumbnail (if it has no notifications otherwise)
 * or when it had, but that notification is removed from the switcher.
 */
static void
reset_thumb_title (Thumbnail * thumb)
{
  const MBWMClientWindow *mbwmcwin;

  if (thumb->title)
    { /* Reset the title being set_thumb_title_from_hdnote(). */
      g_assert (thumb->hdnote != NULL);
      mb_wm_object_signal_disconnect (MB_WM_OBJECT (thumb->hdnote),
                                      thumb->hdnote_changed_cb_id);
      mb_wm_object_unref (MB_WM_OBJECT (thumb->hdnote));
      thumb->hdnote = NULL;
      thumb->title_icon = thumb->title_text = NULL;
    } else /* @thumb is being created */
      g_assert (thumb->hdnote == NULL);

  mbwmcwin = actor_to_client_window (thumb->apwin);
  set_thumb_title (thumb, thumb_title_text (mbwmcwin->name));
}

/* Called when Thumbnail.hdnote's summary or icon changes to update
 * @thumb->title, provided that that it is set from @hdnote. */
static Bool
title_note_changed (HdNote * hdnote, int unused, Thumbnail * thumb)
{
  gchar *icon, *summary;

  g_assert (thumb->title_icon != NULL);
  g_assert (thumb->title_text != NULL);

  if ((icon = hd_note_get_icon (hdnote)) != NULL)
    {
      clutter_container_remove_actor (CLUTTER_CONTAINER (thumb->title),
                                      thumb->title_icon);
      thumb->title_icon = CLUTTER_ACTOR (load_icon (icon,
                                                    TITLE_NOTE_ICON_SIZE));
      clutter_actor_set_position (thumb->title_icon,
                                  TITLE_NOTE_ICON_SIZE/2,
                                  THWIN_TITLE_AREA_HEIGHT
                                    - TITLE_NOTE_ICON_SIZE/2);
      clutter_container_add_actor (CLUTTER_CONTAINER (thumb->title),
                                   thumb->title_icon);
      XFree (icon);
    }
  if ((summary = hd_note_get_summary (hdnote)) != NULL)
    {
      clutter_label_set_text (CLUTTER_LABEL (thumb->title_text), summary);
      XFree (summary);
    }

  return False;
}

/* Sets @thumb's title to @hdnote's icon and summary.  This is called
 * when a notification whose application is already running is added
 * to the switcher. */
static void
set_thumb_title_from_hdnote (Thumbnail * thumb, HdNote * hdnote)
{
  ClutterActor *title;
  gchar *summary, *icon;

  g_assert (!thumb->hdnote);

  /* It is not recoverable if we cannot get the @summary.
   * @icon is not that critical. */
  if (!(summary = hd_note_get_summary (hdnote)))
    return;
  icon = hd_note_get_icon (hdnote);

  /* Center the @icon vertically in the title area. */
  thumb->title_icon = CLUTTER_ACTOR (load_icon (icon,
                                                TITLE_NOTE_ICON_SIZE));
  clutter_actor_set_position (thumb->title_icon,
                              TITLE_NOTE_ICON_SIZE / 2,
                              THWIN_TITLE_AREA_HEIGHT
                                - TITLE_NOTE_ICON_SIZE / 2);

  /* Align @summary to the top, so it doesn't look different
   * from application titles. */
  thumb->title_text = thumb_title_text (summary);
  clutter_actor_set_x (thumb->title_text,
                       TITLE_NOTE_ICON_SIZE + TITLE_NOTE_TEXT_MARGIN);

  /* @title = @title_icon + @title_text. */
  title = clutter_group_new ();
  clutter_container_add (CLUTTER_CONTAINER (title),
                         thumb->title_icon, thumb->title_text, NULL);
  set_thumb_title (thumb, title);

  /* Update @text when @hdnote changes. */
  thumb->hdnote = mb_wm_object_ref (MB_WM_OBJECT (hdnote));
  thumb->hdnote_changed_cb_id = mb_wm_object_signal_connect (
                        MB_WM_OBJECT (hdnote), HdNoteSignalChanged,
                        (MBWMObjectCallbackFunc)title_note_changed,
                        thumb);

  XFree (summary);
  if (icon)
    XFree (icon);
}
/* Setting a thumbnail's title }}} */

/* Zooming {{{ */
/* add_effect_closure() callback for hd_task_navigator_zoom_in()
 * to leave the navigator. */
static void
zoom_in_complete (ClutterActor * navigator, ClutterActor * apwin)
{
  /* To minimuze confusion the navigator hides all application windows it
   * knows about when it starts hiding.  Undo it for the one we have zoomed
   * inte, because it is expected to be shown. XXX Not necessary ATM. */
  clutter_actor_hide (navigator);
  clutter_actor_show (apwin);
}

/*
 * Zoom into @win, which must be shown in the navigator.  @win is not
 * returned to its original parent until the effect is complete, when
 * the navigator disappears.  Calls @fun(@win, @funparam) then, unless
 * @fun is %NULL.  Only to be called when the navigator is active.
 */
void
hd_task_navigator_zoom_in (HdTaskNavigator * self, ClutterActor * win,
                           ClutterEffectCompleteFunc fun, gpointer funparam)
{ g_debug (__FUNCTION__);
  gint xpos, ypos;
  gdouble xscale, yscale;
  const Thumbnail *thumb;

  if (!hd_task_navigator_is_active (self))
    {
      g_critical("attempt to zoom in from an inactive navigator");
      goto damage_control;
    }
  if (!(thumb = find_by_apwin (win)))
    {
      hd_task_navigator_exit (self);
      goto damage_control;
    }

  /*
   * Zoom the navigator itself so that when the effect is complete
   * the non-decoration part of .apwin is in its regular position
   * and size in application view.
   *
   * @xpos, @ypos := .prison's absolute coordinates.
   */
  clutter_actor_get_position  (thumb->thwin,  &xpos,    &ypos);
  clutter_actor_get_scale     (thumb->prison, &xscale,  &yscale);
  ypos -= hd_scrollable_group_get_viewport_y (Navigator_area);
  xpos += MARGIN_DEFAULT;
  ypos += MARGIN_DEFAULT;

  clutter_effect_scale (Zoom_effect, Scroller,
                        1 / xscale, 1 / yscale, NULL, NULL);
  clutter_effect_move (Zoom_effect, Scroller,
                       -xpos / xscale + thumb->inapwin->x,
                       -ypos / yscale + thumb->inapwin->y,
                       NULL, NULL);

  add_effect_closure (Zoom_effect_timeline,
                      (ClutterEffectCompleteFunc)zoom_in_complete,
                      CLUTTER_ACTOR(self), win);
  add_effect_closure (Zoom_effect_timeline,
                      (ClutterEffectCompleteFunc)fun, win, funparam);
  return;

damage_control:
  if (fun != NULL)
    fun(win, funparam);
}

/*
 * Zoom out of @win to the navigator.  @win must have previously been added,
 * and is taken immedeately from its current parent.  Unless @fun is %NULL
 * @fun(@win, @funparam) is executed when the effect completes.  Only to be
 * called when the navigator is not active.
 */
void
hd_task_navigator_zoom_out (HdTaskNavigator * self, ClutterActor * win,
                            ClutterEffectCompleteFunc fun, gpointer funparam)
{ g_debug (__FUNCTION__);
  guint hthumb;
  const Thumbnail *thumb;
  gdouble sxprison, syprison, sxscroller, syscroller;
  gint yarea, xthumb, ythumb, xprison, yprison, xscroller, yscroller;

  if (hd_task_navigator_is_active (self))
    {
      g_critical("attempt to zoom out of an already active navigator");
      goto damage_control;
    }
  if (!(thumb = find_by_apwin (win)))
    {
      hd_task_navigator_enter (self);
      goto damage_control;
    }

  /* Our "show" callback will grab the butts of @win. */
  clutter_actor_show (Navigator);

  /* @hthumb := intended real size of @thumb
   * @xthumb, @ythumb := intended real position of @thumb */
  thumb_size (NULL, &hthumb);
  clutter_actor_get_position (thumb->thwin,  &xthumb, &ythumb);

  /*
   * Scroll the @Navigator_area so that @thumb is closest the middle
   * of the screen.  #HdScrollableGroup does not let us scroll the
   * viewport out of the real estate, but in return we need to ask
   * how much we actually managed to scroll.
   */
  yarea = ythumb - (SCREEN_HEIGHT - hthumb) / 2;
  hd_scrollable_group_set_viewport_y (Navigator_area, yarea);
  yarea = hd_scrollable_group_get_viewport_y (Navigator_area);

  /* Reposition and rescale the @Scroller so that .apwin is shown exactly
   * in the same position and size as in the application view. */

  /* Make @ythumb absolute (relative to the top of the screen). */
  ythumb -= yarea;

  /* @xprison, @yprison := absolute position of .prison. */
  xprison = xthumb + MARGIN_DEFAULT;
  yprison = ythumb + MARGIN_DEFAULT;
  clutter_actor_get_scale (thumb->prison, &sxprison, &syprison);

  /* anchor of .prison      <- start of non-decoration area of .apwin
   * visual size of .prison <- width of non-decoration area of .apwin */
  xscroller = thumb->inapwin->x - xprison/sxprison;
  yscroller = thumb->inapwin->y - yprison/syprison;
  sxscroller = 1 / sxprison;
  syscroller = 1 / syprison;

  clutter_actor_set_scale     (Scroller, sxscroller, syscroller);
  clutter_actor_set_position  (Scroller,  xscroller,  yscroller);
  clutter_effect_scale (Zoom_effect, Scroller, 1, 1, NULL, NULL);
  clutter_effect_move  (Zoom_effect, Scroller, 0, 0, NULL, NULL);
  add_effect_closure (Zoom_effect_timeline, fun, win, funparam);
  return;

damage_control:
  if (fun != NULL)
    fun(win, funparam);
}
/* Zooming }}} */

/* Misc window commands {{{ */
/* Add hibernation decoration to @win.  TODO No API to un-hibernate. */
void
hd_task_navigator_hibernate_window (HdTaskNavigator * self,
                                    ClutterActor * win)
{
  Thumbnail *thumb;

  if (!(thumb = find_by_apwin(win)))
    return;
  if (thumb->hibernation)
    return;

  thumb->hibernation = clutter_clone_texture_new (Master_hibernated);
  clutter_actor_set_anchor_point_from_gravity (thumb->hibernation,
                                               CLUTTER_GRAVITY_CENTER);
  clutter_actor_set_position (thumb->hibernation,
                              THWIN_TITLE_AREA_LEFT_GAP / 2,
                              THWIN_TITLE_AREA_HEIGHT / 2);
  clutter_container_add_actor (CLUTTER_CONTAINER (thumb->thwin),
                               thumb->hibernation);
}

/* Tells us to show @new_win in place of @old_win, and forget about
 * the latter one entirely. */
void
hd_task_navigator_replace_window (HdTaskNavigator * self,
                                  ClutterActor * old_win,
                                  ClutterActor * new_win)
{ g_debug(__FUNCTION__);
  Thumbnail *thumb;
  gboolean showing;

  if (old_win == new_win || !(thumb = find_by_apwin(old_win)))
    return;

  showing = hd_task_navigator_is_active (self);
  if (showing)
    release_win (thumb);
  g_object_unref(thumb->apwin);
  thumb->apwin = g_object_ref(new_win);
  if (showing)
    claim_win (thumb);
}
/* Misc window commands }}} */

/* Add/remove windows {{{ */
static void remove_notewin (HdNote * hdnote);
static ClutterActor *add_notewin_from_hdnote (HdNote * hdnote);
static gboolean tnote_matches_thumb (const TNote * tnote,
                                     const Thumbnail * thumb);

/* Returns the window whose client's clutter client's texture is @win. */
static MBWMClientWindow *
actor_to_client_window (ClutterActor * win)
{
  const MBWMCompMgrClutterClient *cmgrcc;

  cmgrcc = g_object_get_data (G_OBJECT (win),
                              "HD-MBWMCompMgrClutterClient");
  return MB_WM_COMP_MGR_CLIENT (cmgrcc)->wm_client->window;
}

/* #ClutterEffectCompleteFunc for hd_task_navigator_remove_window()
 * called when the TV-turned-off effect of @thumb finishes. */
static void
thwin_turned_off_1 (ClutterActor * unused, Thumbnail * thumb)
{
  /* Release .apwin, forget .thwin and deallocate @thumb. */
  release_win (thumb);
  clutter_container_remove_actor (CLUTTER_CONTAINER (Navigator_area),
                                  thumb->thwin);
  g_object_unref(thumb->apwin);
  g_free (thumb);
}

/* Likewise.  This is a separate function because it needs a different
 * user data pointer (@cmgrcc) and we don't feel like defining a new
 * struture only for this purpose. */
static void
thwin_turned_off_2 (ClutterActor * unused, MBWMCompMgrClutterClient * cmgrcc)
{
  /* Undo what we did to @cmgrcc in hd_task_navigator_remove_window().
   * TODO hd-comp-mgr.c would ask for a hd_comp_mgr_sync_stacking(),
   *      do we need to? */
  mb_wm_comp_mgr_clutter_client_unset_flags (cmgrcc,
                                 MBWMCompMgrClutterClientDontUpdate
                               | MBWMCompMgrClutterClientEffectRunning);
  mb_wm_object_unref (MB_WM_OBJECT(cmgrcc));
}

/* Called when a %Thumbnail.thwin is clicked. */
static gboolean
thwin_clicked (ClutterActor * thwin, ClutterButtonEvent * event,
               gpointer unused)
{
  const Thumbnail *thumb;

  if ((thumb = find_by_thwin (thwin)) != NULL)
    g_signal_emit_by_name (Navigator, "thumbnail-clicked", thumb->apwin);
  return TRUE;
}

/* Called when a %Thumbnail.close (@thwin's close button) is clicked. */
static gboolean
thwin_close_clicked (ClutterActor * thwin, ClutterButtonEvent * event,
                     gpointer unused)
{
  const Thumbnail *thumb;

  if ((thumb = find_by_thwin (thwin)) != NULL)
    g_signal_emit_by_name (Navigator, "thumbnail-closed", thumb->apwin);
  return TRUE;
}

/* Fills @thumb, creating the .thwin hierarchy.  The exact position of the
 * inner actors is decided by layout_thumbs().  If there is a notewin for
 * this application it will be removed and added as the thumbnail title. */
static void
create_thumb (Thumbnail * thumb, ClutterActor * apwin)
{
  guint i;
  const TNote *tnote;
  XClassHint xwinhint;
  const MBWMClientWindow *mbwmcwin;

  mbwmcwin = actor_to_client_window (apwin);
  memset (thumb, 0, sizeof (*thumb));

  /* @apwin related fields */
  thumb->apwin      = g_object_ref(apwin);
  thumb->inapwin    = &mbwmcwin->geometry;
  if (XGetClassHint(mbwmcwin->wm->xdpy, mbwmcwin->xwindow, &xwinhint))
    {
      thumb->class_hint = xwinhint.res_class;
      XFree (xwinhint.res_name);
      thumb->video_fname = g_strdup_printf(VIDEO_SCREENSHOT_DIR "/%s.jpg",
                                           thumb->class_hint);
    }
  else
    g_warning ("XGetClassHint(%lx): failed", mbwmcwin->xwindow);

  /* .thwin */
  thumb->thwin = clutter_group_new ();
  clutter_actor_set_reactive (thumb->thwin, TRUE);
  g_signal_connect (thumb->thwin, "button-release-event",
                    G_CALLBACK (thwin_clicked), NULL);
  clutter_container_add_actor (CLUTTER_CONTAINER (Navigator_area),
                               thumb->thwin);

  /* .prison: clip @apwin's non-decoration area and anchor it
   * where the area starts. */
  thumb->prison = clutter_group_new ();
  clutter_actor_set_clip (thumb->prison,
                          thumb->inapwin->x,      thumb->inapwin->y,
                          thumb->inapwin->width,  thumb->inapwin->height);
  clutter_actor_set_anchor_point (thumb->prison,
                                  thumb->inapwin->x, thumb->inapwin->y);
  clutter_actor_set_position (thumb->prison,
                              MARGIN_DEFAULT, MARGIN_DEFAULT);
  clutter_container_add_actor (CLUTTER_CONTAINER (thumb->thwin),
                               thumb->prison);

  /* .foreground */
  thumb->foreground = clutter_clone_texture_new (Master_foreground);
  clutter_container_add_actor (CLUTTER_CONTAINER (thumb->thwin),
                               thumb->foreground);

  /* .title */
  /* Do we have a notification for @apwin? */
  for (i = 0; i < Notifications->len; i++)
    {
      tnote = &g_array_index (Notifications, TNote, i);
      if (tnote_matches_thumb (tnote, thumb))
        { /* Yes, steal it from the @Notification_area. */
          set_thumb_title_from_hdnote (thumb, tnote->hdnote);
          remove_notewin (thumb->hdnote);
          break;
        }
    }

  if (!thumb->title)
    /* No we don't; use MBWMClientWindow::name as .title. */
    reset_thumb_title (thumb);

  /* .close: anchor it in the middle. */
  thumb->close = clutter_clone_texture_new (Master_close);
  clutter_actor_set_reactive (thumb->close, TRUE);
  g_signal_connect_swapped (thumb->close, "button-release-event",
                            G_CALLBACK (thwin_close_clicked),
                            thumb->thwin);
  clutter_actor_set_anchor_point_from_gravity (thumb->close,
                                               CLUTTER_GRAVITY_CENTER);
  clutter_container_add_actor (CLUTTER_CONTAINER (thumb->thwin),
                               thumb->close);
}

/*
 * Asks the navigator to forget about @win.  If @fun is not %NULL it is
 * called when @win is actually removed from the screen; this may take
 * some time if there is an effect.  If not, it is called immedeately
 * with @funparam.
 */
void
hd_task_navigator_remove_window (HdTaskNavigator * self,
                                 ClutterActor * win,
                                 ClutterEffectCompleteFunc fun,
                                 gpointer funparam)
{ g_debug (__FUNCTION__);
  guint i;
  Thumbnail *thumb;
  ClutterActor *newborn;

  /* Find @thumb for @win. */
  for (i = 0; ; i++)
    {
      g_return_if_fail (i < Thumbnails->len);
      thumb = &g_array_index (Thumbnails, Thumbnail, i);
      if (thumb->apwin == win)
        break;
    }

  /* If we're active let's do the TV-turned-off effect on @thumb.
   * This effect is run in parallel with the flying effects. */
  if (hd_task_navigator_is_active (self))
    {
      MBWMCompMgrClutterClient *cmgrcc;

      /* @thumb points to an element of a #GArray, which is going
       * to be removed.  Let's make a copy of it. */
      thumb = g_memdup (thumb, sizeof (*thumb));

      /* Hold a reference on @win's clutter client.
       * This is taken from hd-comp-mgr.c:hd_comp_mgr_effect(), */
      cmgrcc = g_object_get_data(G_OBJECT(thumb->apwin),
                                 "HD-MBWMCompMgrClutterClient");
      mb_wm_object_ref (MB_WM_OBJECT(cmgrcc));
      mb_wm_comp_mgr_clutter_client_set_flags (cmgrcc,
                                 MBWMCompMgrClutterClientDontUpdate
                               | MBWMCompMgrClutterClientEffectRunning);

      /* Like when closing in application view, the effect is to scale down
       * vertically the thumbnail until its height becomes 0. */
      clutter_actor_lower_bottom (thumb->thwin);
      clutter_actor_move_anchor_point_from_gravity (thumb->thwin,
                                                    CLUTTER_GRAVITY_CENTER);

      /* At the end of effect deallocate @thumb which we just duplicated,
       * and release @cmgrcc. */
      clutter_effect_scale(Fly_effect, thumb->thwin, 1, 0, NULL, NULL);
      add_effect_closure(Fly_effect_timeline,
                         (ClutterEffectCompleteFunc)thwin_turned_off_1,
                         thumb->thwin, thumb);
      add_effect_closure(Fly_effect_timeline,
                         (ClutterEffectCompleteFunc)thwin_turned_off_2,
                         thumb->thwin, cmgrcc);
    }
  else
    { /* Not active, just remove .thwin. */
      clutter_container_remove_actor (CLUTTER_CONTAINER (Navigator_area),
                                      thumb->thwin);
      g_object_unref(thumb->apwin);
    }

  /* We don't need .class_hint even if we're doing the TV-turned-off
   * effect on @thumb. */
  if (thumb->class_hint)
    {
      XFree (thumb->class_hint);
      g_free (thumb->video_fname);
      thumb->class_hint = thumb->video_fname = NULL;
    }

  /* If @win had a notification, add it to @Notification_area. */
  newborn = NULL;
  if (thumb->hdnote)
    {
      mb_wm_object_signal_disconnect (MB_WM_OBJECT (thumb->hdnote),
                                      thumb->hdnote_changed_cb_id);
      newborn = add_notewin_from_hdnote (thumb->hdnote);
      mb_wm_object_unref (MB_WM_OBJECT (thumb->hdnote));
      thumb->hdnote = NULL;
    }

  g_array_remove_index (Thumbnails, i);
  layout (newborn);

  /* Arrange for calling @fun(@funparam) if/when appripriate. */
  if (is_flying ())
    add_effect_closure (Fly_effect_timeline,
                        fun, CLUTTER_ACTOR (self), funparam);
  else if (fun)
    fun(CLUTTER_ACTOR (self), funparam);
}

/*
 * Tells the swicher to show @win in a thumbnail when active.  If the
 * navigator is active now it starts managing @win.  When @win is managed
 * by the navigator it is not changed in any means other than reparenting
 * it and setting it to be non-reactive.  It is an error to add @win
 * multiple times.
 */
void
hd_task_navigator_add_window (HdTaskNavigator * self,
                              ClutterActor * win)
{ g_debug (__FUNCTION__);
  Thumbnail thumb;

  create_thumb (&thumb, win);
  if (hd_task_navigator_is_active (self))
    claim_win (&thumb);

  g_array_append_val (Thumbnails, thumb);
  if (Thumbnails->len == 5)
    {
      /* The layout guide requires this peculiar rearrangement
       * of thumbnails when we're adding the fifth one. */
      g_array_index (Thumbnails, Thumbnail, 4) =
        g_array_index (Thumbnails, Thumbnail, 3);
      g_array_index (Thumbnails, Thumbnail, 3) =
        g_array_index (Thumbnails, Thumbnail, 2);
      g_array_index (Thumbnails, Thumbnail, 2) = thumb;
    }

  layout (thumb.thwin);
}
/* Add/remove windows }}} */

/* Add/remove notifications {{{ */
/* Prepares @tnote with @hdnote but doesn't allocate a notewin yet. */
static void
create_tnote (TNote * tnote, HdNote * hdnote)
{
  memset (tnote, 0, sizeof (*tnote));
  tnote->hdnote = mb_wm_object_ref (MB_WM_OBJECT (hdnote));
  tnote->destination = hd_note_get_destination (hdnote);
}

/* Releases what was allocated by create_tnote(). */
static void
free_tnote (const TNote * tnote)
{
  mb_wm_object_unref (MB_WM_OBJECT (tnote->hdnote));
  if (tnote->destination)
    XFree (tnote->destination);
}

/* Returns whether the notification represented by @tnote belongs to
 * the application represented by @thumb. */
static gboolean
tnote_matches_thumb (const TNote * tnote, const Thumbnail * thumb)
{
  return thumb->class_hint && tnote->destination
    && !strcmp (thumb->class_hint, tnote->destination);
}

/* Returns the #ClutterActor of the texture of @hdnote. */
static ClutterActor *
hdnote_to_actor (HdNote * hdnote)
{
  MBWMCompMgrClutterClient *cmcc;

  /* This information could be stored in %TNote but the idea of repetitive
   * redundancy is not attractive. */
  cmcc = MB_WM_COMP_MGR_CLUTTER_CLIENT (MB_WM_CLIENT (hdnote)->cm_client);
  return mb_wm_comp_mgr_clutter_client_get_actor (cmcc);
}

/* TNote.notewin's "button-release-event" callback. */
static gboolean
notewin_clicked (ClutterActor *unused, ClutterButtonEvent * event,
                 HdNote * hdnote)
{
  g_signal_emit_by_name (Navigator, "notification-clicked", hdnote);
  return TRUE;
}

/* TNote.notewin's close button's "button-release-event" callback. */
static gboolean
notewin_close_clicked (ClutterActor *unused, ClutterButtonEvent * event,
                       HdNote * hdnote)
{
  g_signal_emit_by_name (Navigator, "notification-closed", hdnote);
  return TRUE;
}

/* Creates a notewin for @hdnote to be shown at the bottom of the switcher
 * as opposed to shoing @hdnote in the title of an application thumbnail. */
static ClutterActor *
create_notewin (HdNote * hdnote)
{
  ClutterActor *notewin, *close;

  notewin = clutter_group_new ();
  clutter_actor_set_size (notewin, NOTE_WIDTH, NOTE_HEIGHT);
  clutter_actor_set_reactive(notewin, TRUE);
  g_signal_connect (notewin, "button-release-event",
                    G_CALLBACK (notewin_clicked), hdnote);

  /* Take a reference of @hdnote's #ClutterActor on behalf of @notewin,
   * which will be taken away when the actor is removed from the
   * container. */
  clutter_container_add_actor (CLUTTER_CONTAINER (notewin),
                               g_object_ref (hdnote_to_actor (hdnote)));

  close = clutter_clone_texture_new (Master_close);
  clutter_actor_set_position (close, NOTE_WIDTH - NOTE_CLOSE_WIDTH, 0);
  clutter_actor_set_reactive (close, TRUE);
  g_signal_connect (close, "button-release-event",
                    G_CALLBACK (notewin_close_clicked), hdnote);
  clutter_container_add_actor (CLUTTER_CONTAINER (notewin), close);

  return notewin;
}

/* Removes @hdnote's notewin from the @Notification_area and updates
 * the remaining notewin:s layout.  The caller is responsible for
 * updateing the layout(). */
static void
remove_notewin (HdNote * hdnote)
{
  guint i, row;
  const TNote *tnote;

  /* Find the row in which it is. */
  row = 0;
  for (i = 0; ; i++)
    {
      g_return_if_fail (i < Notifications->len);
      tnote = &g_array_index (Notifications, TNote, i);
      if (tnote->hdnote == hdnote)
        {
          /*
           * Fuck clutter.  When a #ClutterGroup is destroyed it destroys
           * all of its actors rather than simply dropping a reference.
           * This is highly undesirable for the texture actor of @hdnote,
           * which we may need in the future, when we need to re-add a
           * notewin for @hdnote because its application is closed.
           */
          clutter_container_remove_actor (CLUTTER_CONTAINER (tnote->notewin),
                                          hdnote_to_actor (hdnote));
          clutter_container_remove_actor (
                                  CLUTTER_CONTAINER (Notification_area),
                                  tnote->notewin);
          free_tnote (tnote);
          g_array_remove_index (Notifications, i);
          break;
        }
      else if (i % 2)
        row++;
    }

  /* Starting from that row relocate the remaining notifications. */
  for (i = row * 2; i < Notifications->len; i++)
    {
      tnote = &g_array_index (Notifications, TNote, i);
      if (i + 1 >= Notifications->len || (i % 2))
        {
          /* It's either the last notification or it's the
           * second, fourth, ... one (counting from one),
           * which should be on the right. */
          move (tnote->notewin, NOTE_MARGIN + NOTE_WIDTH, NOTE_HEIGHT * row);
          row++;
        }
      else /* Otherwise on the left. */
        move (tnote->notewin, NOTE_MARGIN, NOTE_HEIGHT * row);
    }
}

/* Remove a notification from the navigator, either if it's shown
 * in the @Notification_area or in a thumbnail title area.
 * Exit navigator if it's become empty. */
void
hd_task_navigator_remove_notification (HdTaskNavigator * self,
                                       HdNote * hdnote)
{ g_debug (__FUNCTION__);
  guint i;
  Thumbnail *thumb;

  g_return_if_fail (hdnote != NULL);

  /* Is @hdnote in a thumbnail's title area? */
  for (i = 0; i < Thumbnails->len; i++)
    {
      thumb = &g_array_index (Thumbnails, Thumbnail, i);
      if (thumb->hdnote == hdnote)
        {
          reset_thumb_title (thumb);
          return;
        }
    }

  /* The navigator layout/position may need to be changed. */
  remove_notewin (hdnote);
  layout (NULL);
}

/* Like add_notewin_from_hdnote().  Returns whether the thumbnail
 * layout() should be updated. */
static gboolean
add_notewin_from_tnote (TNote * tnote)
{
  tnote->notewin = create_notewin (tnote->hdnote);
  g_array_append_val (Notifications, *tnote);
  clutter_container_add_actor (CLUTTER_CONTAINER (Notification_area),
                               tnote->notewin);

  /* Figure out where to display @tnote in @Notification_area.
   * New notification always appear on the right side of the area. */
  if (Notifications->len == 1)
    { /* This is the first one.  The height of the @Navigator_area
       * is updated by layout(). */
      clutter_actor_set_position (tnote->notewin,
                                  NOTE_MARGIN + NOTE_WIDTH, 0);
      return TRUE;
    }
  else if (Notifications->len % 2)
    { /* Third, fifth, sevent, ... notification. */
      const TNote *last;

      /* Start a new row and drop it right below the last
       * notification, leaving no gap. */
      g_assert (Notifications->len >= 2);
      last = &g_array_index (Notifications, TNote, Notifications->len-2);
      clutter_actor_set_position (tnote->notewin, NOTE_MARGIN + NOTE_WIDTH,
                     clutter_actor_get_y (last->notewin) + NOTE_HEIGHT);
      set_navigator_height (navigator_height ());

      /* If we're adding the second+ notification we needn't update
       * the navigator layout because then it doesn't change. */
      return FALSE;
    }
  else
    { /* Second, fourth, sixth, ... notification. */
      const TNote *last;

      /* Slide the last notification to the left and drop the new one at
       * its old place.  The height of the @Navigator_area doesn't change. */
      g_assert (Notifications->len >= 2);
      last = &g_array_index (Notifications, TNote, Notifications->len-2);
      clutter_actor_set_position (tnote->notewin,
                                  NOTE_MARGIN + NOTE_WIDTH,
                                  clutter_actor_get_y (last->notewin));
      if (move (last->notewin,
                NOTE_MARGIN, clutter_actor_get_y (last->notewin)))
        show_when_complete (tnote->notewin);

      return FALSE;
    }
}

/* Creates a notewin for @hdnote and adds it to the @Notification_area,
 * updating the layout of the existing notewins if necessary.  Returns
 * an actor you need to update the navigator layout() with, or %NULL. */
static ClutterActor *
add_notewin_from_hdnote (HdNote * hdnote)
{
  TNote tnote;

  create_tnote (&tnote, hdnote);
  return add_notewin_from_tnote (&tnote)
    ? tnote.notewin : NULL;
}

/* Show a notification in the navigator, either in the @Notification_area
 * or in the notification's thumbnail title area if it's running. */
void
hd_task_navigator_add_notification (HdTaskNavigator * self,
                                    HdNote * hdnote)
{ g_debug (__FUNCTION__);
  guint i;
  TNote tnote;
  Thumbnail *thumb;

  g_return_if_fail (hdnote != NULL);

  /* Is @hdnote's application in the switcher? */
  create_tnote (&tnote, hdnote);
  for (i = 0; i < Thumbnails->len; i++)
    {
      thumb = &g_array_index (Thumbnails, Thumbnail, i);
      if (tnote_matches_thumb (&tnote, thumb))
        {
          if (thumb->hdnote)
            {
              /* hildon-home should have replaced the summary of the existing
               * @thumb->hdnote instead; the easy way out is adding the new
               * one to the @Notification_area. */
              g_critical ("%s: attempt to add more than one notification "
                          "to `%s'", __FUNCTION__, thumb->class_hint);
              break;
            }

          free_tnote (&tnote);
          set_thumb_title_from_hdnote (thumb, hdnote);
          return;
        }
    }

  if (add_notewin_from_tnote (&tnote))
    layout (tnote.notewin);
}
/* Add/remove notifications }}} */

/* %HdTaskNavigator {{{ */
/* Callbacks {{{ */
G_DEFINE_TYPE (HdTaskNavigator, hd_task_navigator, CLUTTER_TYPE_GROUP);

/* @Swither's "show" handler. */
static gboolean
navigator_shown (ClutterActor * navigator, gpointer unused)
{
  guint i;

  /* Grab the keybord focus for navigator_hit(). */
  clutter_stage_set_key_focus (CLUTTER_STAGE (clutter_stage_get_default ()),
                               navigator);

  /* Take all application windows we know about into our care
   * because we responsible for showing them now. */
  for (i = 0; i < Thumbnails->len; i++)
    claim_win (&g_array_index (Thumbnails, Thumbnail, i));

  return FALSE;
}

/* @Navigator's "hide" handler. */
static gboolean
navigator_hidden (ClutterActor * navigator, gpointer unused)
{
  guint i;

  /* Undo navigator_show(). */
  clutter_stage_set_key_focus (CLUTTER_STAGE (clutter_stage_get_default ()),
                               NULL);
  for (i = 0; i < Thumbnails->len; i++)
    release_win (&g_array_index (Thumbnails, Thumbnail, i));

  return FALSE;
}

/* Called when you click the navigator outside thumbnails and notifications,
 * possibly to hide the navigator. */
static gboolean
navigator_clicked (ClutterActor * navigator, ClutterButtonEvent * event,
                   HdScrollableGroup * navarea)
{
  if (hd_scrollable_group_is_clicked (navarea))
    g_signal_emit_by_name (navigator, "background-clicked");
  return TRUE;
}

/* @Navigator_area's notify::has-clip callback to prevent clipping
 * from being applied. */
static gboolean
unclip (ClutterActor * actor, GParamSpec * prop, gpointer unused)
{
  /* Make sure not to recurse infinitely. */
  if (clutter_actor_has_clip (actor))
    clutter_actor_remove_clip (actor);
  return TRUE;
}
/* Callbacks }}} */

/* Creates an effect template for %EFFECT_LENGTH and also returns its timeline.
 * The timeline is usually needed to hook onto its "completed" signal. */
static ClutterEffectTemplate *
new_effect (ClutterTimeline ** timelinep)
{
  ClutterEffectTemplate *effect;

  /*
   * It's necessary to do it this way because there's no way to retrieve
   * an effect's timeline after it's created.  Ask the template not to make
   * copies of the timeline * but always use the very same one we provide,
   * otherwise * it would have to be clutter_timeline_start()ed for every
   * animation to get its signals.
   */
  *timelinep = clutter_timeline_new_for_duration (EFFECT_LENGTH);
  effect = clutter_effect_template_new (*timelinep, CLUTTER_ALPHA_RAMP_INC);
  clutter_effect_template_set_timeline_clone (effect, FALSE);

  return effect;
}

/* The object we create is initially hidden. */
static void
hd_task_navigator_init (HdTaskNavigator * self)
{
  static const ClutterColor bgcolor = { 0X00, 0X00, 0X00, 0XAA };
  GtkStyle *style;
  ClutterActor *bg;

  /* Data structures */
  Thumbnails = g_array_new (FALSE, FALSE, sizeof (Thumbnail));
  Notifications = g_array_new (FALSE, FALSE, sizeof (TNote));

  Navigator = CLUTTER_ACTOR (self);
  clutter_actor_set_reactive (Navigator, TRUE);
  g_signal_connect (Navigator, "show", G_CALLBACK (navigator_shown),  NULL);
  g_signal_connect (Navigator, "hide", G_CALLBACK (navigator_hidden), NULL);

  /* Actor hierarchy */
  bg = clutter_rectangle_new_with_color (&bgcolor);
  clutter_actor_set_size (bg, SCREEN_WIDTH, SCREEN_HEIGHT);
  clutter_container_add_actor (CLUTTER_CONTAINER (Navigator), bg);

  Scroller = tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);
  clutter_actor_set_size (Scroller, SCREEN_WIDTH, SCREEN_HEIGHT);
  clutter_container_add_actor (CLUTTER_CONTAINER (Navigator), Scroller);

  /*
   * When we zoom in we may need to move the @Scroller up or downwards.
   * If we leave clipping on that becomes visible then, by cutting one
   * half of the zooming window.  Circumvent it by removing clipping
   * at the same time it is set.  TODO This can be considered a hack.
   */
  Navigator_area = HD_SCROLLABLE_GROUP (hd_scrollable_group_new ());
  clutter_actor_set_reactive (CLUTTER_ACTOR (Navigator_area), TRUE);
  g_signal_connect (Navigator_area, "notify::has-clip",
                    G_CALLBACK (unclip), NULL);
  g_signal_connect_swapped (Navigator_area, "button-release-event",
                            G_CALLBACK (navigator_clicked), Navigator);
  clutter_container_add_actor (CLUTTER_CONTAINER (Scroller),
                               CLUTTER_ACTOR (Navigator_area));

  /*
   * It's important to set @Notification_area's position now, otherwise if
   * the first thing to add is a notification layout() will think it needs
   * to move() @Notification_area, delaying the appearance of the new
   * notification until the end of effect, which is bogus.
   */
  Notification_area = clutter_group_new ();
  clutter_actor_set_position (Notification_area,
                              0, SCREEN_HEIGHT - NOTE_HEIGHT);
  clutter_container_add_actor (CLUTTER_CONTAINER (Navigator_area),
                               Notification_area);

  /* Effect timelines */
  Fly_effect = new_effect (&Fly_effect_timeline);
  Zoom_effect = new_effect (&Zoom_effect_timeline);

  /* Master pieces */
  Master_close      = load_icon ("qgn_home_close",
                                 MIN (THWIN_CLOSE_WIDTH, THWIN_CLOSE_HEIGHT));
  Master_hibernated = load_icon ("hibernation-icon",
                                 MIN (THWIN_TITLE_AREA_LEFT_GAP,
                                      THWIN_TITLE_AREA_HEIGHT));

  style = gtk_rc_get_style_by_paths (gtk_settings_get_default(),
                                     "task-switcher-thumbnail", NULL,
                                     G_TYPE_NONE);
  if (style != NULL)
    { /* @Title_text_color.alpha is fixed and already set. */
      Title_text_font         = pango_font_description_to_string(style->font_desc);
      Title_text_color.red    = style->text[GTK_STATE_NORMAL].red   >> 8;
      Title_text_color.green  = style->text[GTK_STATE_NORMAL].green >> 8;
      Title_text_color.blue   = style->text[GTK_STATE_NORMAL].blue  >> 8;
      Master_foreground = load_image (style->rc_style->bg_pixmap_name[GTK_STATE_NORMAL]);
      /* @style is owned by GTK. */
    }
  else
    {
      g_warning ("Unable to get style for \"task-switcher-thumbnail\"");
      Master_foreground = empty_texture (SCREEN_WIDTH, SCREEN_HEIGHT);
    }

  /* We don't have anything to show yet, so let's hide. */
  clutter_actor_hide (Navigator);
}

static void
hd_task_navigator_class_init (HdTaskNavigatorClass * klass)
{
  /* background_clicked() is emitted when the navigator is active
   * and the background (outside thumbnails and notifications) is
   * clicked (not to scroll the navigator). */
  g_signal_new ("background-clicked", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  /* thumbnail_clicked(@apwin) is emitted when the navigator is active
   * and @apwin's thumbnail is clicked by the user to open the task. */
  g_signal_new ("thumbnail-clicked", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1, CLUTTER_TYPE_ACTOR);

  /* thumbnail_closed(@apwin) is emitted when the navigator is active
   * and @apwin's thumbnail is clicked by the user to close the task. */
  g_signal_new ("thumbnail-closed", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1, CLUTTER_TYPE_ACTOR);

  /* notification_clicked(@hdnote) is emitted when the notification window 
   * is clicked by the user to activate its action. */
  g_signal_new ("notification-clicked", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE, 1, G_TYPE_POINTER);

  /* Like thumbnail_closed. */
  g_signal_new ("notification-closed", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE, 1, G_TYPE_POINTER);
}

HdTaskNavigator *
hd_task_navigator_new (void)
{
  return g_object_new (HD_TYPE_TASK_NAVIGATOR, NULL);
}
/* %HdTaskNavigator }}} */

/* vim: set foldmethod=marker: */
/* End of hd-task-navigator.c */
