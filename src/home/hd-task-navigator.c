/*
 * {{{
 * Implements a plate shown in task switcher view, which displays
 * the thumbnails of the running applications and incoming events.
 * Note that throughout the code "notification" refers to incoming
 * event notifications.
 *
 * Actor hierarchy:
 * @Navigator                 #ClutterGroup
 *   @Scroller                #TidyFingerScroll
 *     @Navigator_area        #HdScrollableGroup
 *       @Thumbnails          #ClutterGroup:s
 *
 * Thumbnail.thwin hierarchy:
 *   .plate                   #ClutterGroup
 *     .frames.all            #ClutterGroup
 *       .frames.north_west   #ClutterCloneTexture
 *       .frames.north        #ClutterCloneTexture
 *       .frames.north_east   #ClutterCloneTexture
 *       .frames.west         #ClutterCloneTexture
 *       .frames.center       #ClutterCloneTexture  notifications
 *       .frames.east         #ClutterCloneTexture
 *       .frames.south_west   #ClutterCloneTexture
 *       .frames.south        #ClutterCloneTexture
 *       .frames.south_east   #ClutterCloneTexture
 *     .title                 #ClutterLabel
 *     .close                 #ClutterGroup
 *   .prison                  #ClutterGroup
 *     .titlebar              #ClutterGroup         applications
 *     .windows               #ClutterGroup         applications
 *       .apwin               #ClutterActor         applications
 *       .dialogs             #ClutterActor         applications
 *     .video                 #ClutterTexture       applications
 *     .icon                  #ClutterTexture       notifications
 *     .time                  #ClutterLabel         notifications
 *     .message               #ClutterLabel         notifications
 * }}}
*/

/* Include files */
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>
#include <tidy/tidy-finger-scroll.h>

#include <matchbox/core/mb-wm.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

#include "hildon-desktop.h"
#include "hd-atoms.h"
#include "hd-comp-mgr.h"
#include "hd-task-navigator.h"
#include "hd-scrollable-group.h"
#include "hd-switcher.h"
#include "hd-render-manager.h"
#include "hd-title-bar.h"
#include "hd-clutter-cache.h"
#include "hd-transition.h"
#include "hd-theme.h"
#include "hd-util.h"

/* Standard definitions {{{ */
#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN              "hd-task-navigator"

/* If g_return_*_if_fail() are disabled (as in the default configuration)
 * replace them with g_assert() to have something at least. */
#ifdef G_DISABLE_CHECKS
# undef  g_return_if_fail
# define g_return_if_fail         g_assert
#endif

/* Measures (in pixels).  Unless indicated, none of them is tunable. */
/* Common platform metrics */
#define SCREEN_WIDTH              HD_COMP_MGR_LANDSCAPE_WIDTH
#define SCREEN_HEIGHT             HD_COMP_MGR_LANDSCAPE_HEIGHT
#define MARGIN_DEFAULT             8
#define MARGIN_HALF                4
#define ICON_FINGER               48
#define ICON_STYLUS               32

/*
 * %GRID_TOP_MARGIN:              Space not considered at the top of the
 *                                switcher when layout out the thumbnails.
 * %GRID_HORIZONTAL_GAP,
 * %GRID_VERTICAL_GAP:            How much gap to leave between thumbnails.
 */
#define GRID_TOP_MARGIN           HD_COMP_MGR_TOP_MARGIN
#define GRID_HORIZONTAL_GAP       16
#define GRID_VERTICAL_GAP         16

/*
 * Application thumbnail dimensions, depending on the number of
 * currently running applications.  These dimension include everything
 * except gaps between thumbnails (naturally) and and enlarged close
 * button reaction area.  1-2 thumbnails are LARGE, 3-6 are MEDIUM
 * and the rest are SMALL.
 */
#define THUMB_LARGE_WIDTH         344
#define THUMB_LARGE_HEIGHT        214
#define THUMB_MEDIUM_WIDTH        224
#define THUMB_MEDIUM_HEIGHT       150
#define THUMB_SMALL_WIDTH         152
#define THUMB_SMALL_HEIGHT        112

/* Metrics inside a thumbnail. */
/* These are NOT the dimensions of the frame graphics but marings. */
#define FRAME_TOP_HEIGHT          32
#define FRAME_WIDTH                2
#define FRAME_BOTTOM_HEIGHT        2

/*
 * %CLOSE_ICON_SIZE:              Now this *is* the graphics size of the
 *                                close button; used to calculate where
 *                                to clip the title.  The graphics is
 *                                located in the top-right corner of the
 *                                thumbnail.
 * %CLOSE_AREA_SIZE:              The size of the area where the user can
 *                                click to close the thumbnail.  Thie area
 *                                and the graphics are centered at the same
 *                                point.
 */
#define CLOSE_ICON_SIZE           32
#define CLOSE_AREA_SIZE           64

#define TITLE_LEFT_MARGIN         MARGIN_DEFAULT
#define TITLE_RIGHT_MARGIN        MARGIN_HALF
#define TITLE_HEIGHT              FRAME_TOP_HEIGHT

#define PRISON_XPOS               FRAME_WIDTH
#define PRISON_YPOS               FRAME_TOP_HEIGHT

/*
 * %ZOOM_EFFECT_DURATION:         Determines how many miliseconds should
 *                                it take to zoom thumbnails.  Tunable.
 *                                Increase for the better observation of
 *                                effects or decrase for faster feedback.
 * %FLY_EFFECT_DURATION:          Same for the flying animation, ie. when
 *                                the windows are repositioned.
 */
#if 1
# define ZOOM_EFFECT_DURATION      200
# define FLY_EFFECT_DURATION       400
#else
# define ZOOM_EFFECT_DURATION      1000
# define FLY_EFFECT_DURATION       1000
#endif
/* Standard definitions }}} */

/* Macros {{{ */
#define for_each_thumbnail(li, thumb)     \
  for ((li) = Thumbnails; (li) && ((thumb) = li->data); \
       (li) = (li)->next)
#define for_each_appthumb(li, thumb)      \
  for ((li) = Thumbnails; (li) != Notifications && ((thumb) = li->data); \
       (li) = (li)->next)
#define for_each_notification(li, thumb)  \
  for ((li) = Notifications; (li) && ((thumb) = li->data); \
       (li) = (li)->next)

#define thumb_is_application(thumb)  ((thumb)->type == APPLICATION)
#define thumb_is_notification(thumb) ((thumb)->type == NOTIFICATION)
#define thumb_has_notification(thumb) ((thumb)->tnote != NULL)
/* Macros }}} */

/* Type definitions {{{ */
/* Layout {{{
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
   * -- @thumbsize:       Desired size of the thumbnails, points to one
   *                      of %Thumbsizes.
   */
  guint cells_per_row;
  guint xpos, last_row_xpos, ypos;
  guint hspace, vspace;
  const GtkRequisition *thumbsize;
} Layout;
/* }}} */

/* Thumbnail data structures {{{ */
/* Extra information about incoming event notification clients. */
typedef struct
{
  /*
   * -- @hdnote:               The notification client,
   *                           mb_wm_object_ref()ed.
   * -- @hdnote_changed_cb_id: %HdNoteSignalChanged callback id.
   */
  HdNote                      *hdnote;
  unsigned long                hdnote_changed_cb_id;
} TNote;

/* Structure to hol a %Thumbnail's frame decoration. */
typedef struct
{
  ClutterActor *all;

  union
  {
    struct
    {
      ClutterActor *north_west, *north, *north_east;
      ClutterActor *west, *center, *east;
      ClutterActor *south_west, *south, *south_east;
    };

    ClutterActor *pieces[9];
  };
} Thumbnail_frame;

/* Our central object, the thumbnail. */
typedef struct
{
  /* Applications and notifications are represented by the same structure. */
  enum { APPLICATION, NOTIFICATION } type;

  /*
   * -- @thwin:       @Navigator_area's thumbnail window and event responder.
   * -- @prison:      In application thumbnails it scales and positions @windows
   *                  and its contents.  Otherwise it's just a simple container.
   * -- @plate:       Groups the @title and the @frame graphics; used to fade
   *                  them all at once.
   * -- @title:       What to put in the thumbnail's title area.
   *                  Centered vertically within TITLE_HEIGHT.
   * -- @close:       An invisible actor (graphics is part of the frame)
   *                  reacting to user taps to close the thumbnail.
   *                  Slightly reaches out of the thumbnail bounds.
   * -- @frame:       Frame graphics.
   */
  ClutterActor        *thwin, *prison, *plate;
  ClutterActor        *title, *close;
  Thumbnail_frame      frame;

  union
  {
    /* Application-thumbnail-specific fields */
    struct
    {
      /*
       * -- @apwin:       The pristine application window, not to be touched.
       *                  Hidden if we have a .video.
       * -- @windows:     Just 0-dimension container for @apwin and @dialogs,
       *                  its sole purpose is to make it easier to hide them
       *                  when the %Thumbnail has a @video.
       * -- @titlebar:    An actor that looks like the original title bar.
       *                  Faded in/out when zooming in/out, but normally
       *                  transparent or not visible at all.
       * -- @dialogs:     The application's dialogs, popup menus and whatsnot
       *                  if it has or had any earlier, otherwise %NULL.
       *                  They are shown along with .apwin.  Hidden if
       *                  we have a .video.
       * -- @inapwin:     Delimits the non-decoration area in @apwin; this is
       *                  what we want to show in the switcher, not the whole
       *                  @apwin.
       * -- @saved_title: What the application window's title was when it
       *                  left for hibernation.  It's only use is to know
       *                  what to reset the thumb title to if its notification
       *                  is removed while the client is still hibernated.
       *                  Cleared when the window actor is replaced, presumably
       *                  because it's woken up.
       * -- @nodest:      What notifications this thumbnails is destination for.
       *                  Taken from the _HILDON_NOTIFICATION_THREAD property
       *                  of the thumbnail's client or its WM_CLASS hint.
       *                  Once checked not refreshed again.  Used in matching
       *                  the appropriate TNote for this application.
       */
      ClutterActor        *apwin, *windows, *titlebar;
      GPtrArray           *dialogs;
      MBGeometry           inapwin;
      gchar               *saved_title, *nodest;

      /*
       * -- @video_fname: Where to look for the last-frame video screenshot
       *                  for this application.  Deduced from some property
       *                  in the application's .desktop file.
       * -- @video_mtime: The last modification time of the image loaded as
       *                  .video.  Used to decide if it should be refreshed.
       * -- @video:       The downsampled texture of the image loaded from
       *                  .video_fname or %NULL.
       */
      ClutterActor        *video;
      const gchar         *video_fname;
      time_t               video_mtime;
    };

    /* Notification-thumbnail-specific fields */
    struct
    {
      ClutterActor        *icon, *time, *message;
    };
  };

  /*
   * -- @tnote:       Notifications always have a %TNote in one of the
   *                  @Thumbnails: either in a %Thumbnail of their own,
   *                  or in the application's they belong to.
   */
  TNote               *tnote;
} Thumbnail;
/* Thumbnail data structures }}} */

/* Clutter effect data structures {{{ */
/*
 * Describes a set of operations on a #ClutterActor.
 * The point is to provide a single interface for
 * doing the same with or without animation (flying).
 * Used by layout_thumbs().
 */
typedef struct
{
  void (*move)  (ClutterActor *actor, gint x, gint y);
  void (*resize)(ClutterActor *actor, gint w, gint h);
  void (*scale) (ClutterActor *actor, gdouble sx, gdouble sy);
} Flyops;

/* For resize_effect() and turnoff_effect(). */
typedef struct
{
  /*
   * @actor:                    The actor to be animated (refed).
   *                            In case of turnoff_effect() it is
   *                            a thwin.
   * @timeline:                 Used when one wants to cancel an effect
   *                            outside of the timeline.
   * @new_frame_cb_id, @timeline_complete_cb_id:
   *                            %ClutterTimeline signal handler IDs
   *                            @new_frame_cb_id can be 0.
   * @frame_fun:                Just about any value that can identify
   *                            an effect.  Typically the effect's
   *                            new-frame callback.  Can be %NULL.
   */
  ClutterActor *actor;
  ClutterTimeline *timeline;
  gulong new_frame_cb_id, timeline_complete_cb_id;
  gconstpointer effectid;

  /* Effect-specific context */
  union
  {
    /*
     * For linear_effect()s. In each frame a number of properties of @actor
     * is changed lineraly, such as width and height.  Each property has its
     * own structure.  The number of properties that can be controlled by
     * such an effect is limited by the the number of elements in the array.
     */
    struct
    {
      /*
       * @init:             The property's value at the start of the effect.
       * @diff:             By the end of effect how much the property should
       *                    be different with regards to @init.
       */
      gfloat init, diff;
    } linear[2];

    /* This is used by resize_effect() on __armel__. */
    struct
    { /* The final dimensions of @actor. */
      guint width, height;
    };

    /* For turnoff_effect() */
    struct
    {
      /*
       * @particles:                The little stars dancing in the background
       *                            of the squeezing thumbnail.  @ang0 is the
       *                            initial angle of a particle.
       * @all_particles:            Container of all the particles.  Used to
       *                            help positioning and setting and to set
       *                            uniform opacity.
       */
      struct
      {
        gdouble ang0;
        ClutterActor *particle;
      } particles[HDCM_UNMAP_PARTICLES];
      ClutterActor *all_particles;
    };
  };
} EffectClosure;

/* Used by add_effect_closure() to store what to call when the effect
 * completes. */
typedef struct
{
  /* @fun(@actor, @funparam) is what is called eventually.
   * @fun is not %NULL, @actor is g_object_ref()ed.
   * @handler_id identifies the signal handler. */
  ClutterEffectCompleteFunc    fun;
  ClutterActor                *actor;
  gpointer                     funparam;
  gulong                       handler_id;
} EffectCompleteClosure;
/* Clutter effect data structures }}} */
/* Type definitions }}} */

/* Private constantsa {{{ */
/* Possible thumbnail sizes. */
static const struct { GtkRequisition small, medium, large; } Thumbsizes =
{
  .large  = { THUMB_LARGE_WIDTH,  THUMB_LARGE_HEIGHT  },
  .medium = { THUMB_MEDIUM_WIDTH, THUMB_MEDIUM_HEIGHT },
  .small  = { THUMB_SMALL_WIDTH,  THUMB_SMALL_HEIGHT  },
};

/* Place and size an actor without animation. */
static const Flyops Fly_at_once =
{
  .move   = clutter_actor_set_position,
  .resize = clutter_actor_set_size,
  .scale  = clutter_actor_set_scale,
};

/* ...now with animation. */
static void check_and_move (ClutterActor *, gint, gint);
static void check_and_resize (ClutterActor *, gint, gint);
static void check_and_scale (ClutterActor *, gdouble, gdouble);
static const Flyops Fly_smoothly =
{
  .move   = check_and_move,
  .resize = check_and_resize,
  .scale  = check_and_scale,
};
/* }}} */

/* Private variables {{{ */
/*
 * -- @Navigator:         Root group, event responder.
 * -- @Scroller:          Viewport of @Navigation_area and controls
 *                        its scrolling.  Moved and scaled when zooming.
 * -- @Navigator_area:    Contains the complete layout.
 */
static HdScrollableGroup *Navigator_area;
static ClutterActor *Navigator, *Scroller;

/*
 * -- @Thumbnails:        List of %Thumbnail:s.
 * -- @NThumbnails:       Length of @Thumbnails.
 * -- @Notifications:     List of %TNote:s.
 * -- @Thumbsize:         @Thumbnails are layed out at this size.
 *                        One of @Thumbsizes.
 *
 * The lists are ordered by the appearance if the thumbnails on the grid,
 * left-to-right, top-to-bottom.  @Notifications is the tail of @Thumbnails.
 */
static GList *Thumbnails, *Notifications;
static guint NThumbnails;
static const GtkRequisition *Thumbsize;

/*
 * Effect templates and their corresponding timelines.
 * -- @Fly_effect:  For moving thumbnails and notification windows around
 *                  and closing the application thumbnail.  It is important
 *                  that they use the same template.  At the moment we don't
 *                  use the effect template, but it is kept around for the
 *                  day we may.
 * -- @Zoom_effect: For zooming in and out of application windows.
 */
static ClutterTimeline *Fly_effect_timeline, *Zoom_effect_timeline;
static ClutterEffectTemplate *Fly_effect, *Zoom_effect;

/*
 * The list of currently running effects created with new_effect().
 * Practically these are all effects used for flying.  Used to learn
 * if a particular effect is already running and if so change it,
 * rather than dumbly adding a new effect and create races between
 * then two of them.  Contains pointers to %EffectClosure:s.
 */
static GPtrArray *Effects;

/* gtkrc articles */
static const gchar *SystemFont, *SmallSystemFont;
static ClutterColor DefaultTextColor, ReversedTextColor;
/* Private variables }}} */

/* Program code */
/* Graphics loading {{{ */
/* Destroying @pixbuf, turns it into a #ClutterTexture.
 * Returns %NULL on failure. */
static ClutterActor *
pixbuf2texture (GdkPixbuf *pixbuf)
{
  GError *err;
  gboolean isok;
  ClutterActor *texture;

#ifndef G_DISABLE_CHECKS
  if (gdk_pixbuf_get_colorspace (pixbuf) != GDK_COLORSPACE_RGB
      || gdk_pixbuf_get_bits_per_sample (pixbuf) != 8
      || gdk_pixbuf_get_n_channels (pixbuf) !=
         (gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3))
    {
      g_critical ("image not in expected rgb/8bps format");
      goto damage_control;
    }
#endif

  err = NULL;
  texture = clutter_texture_new ();
  isok = clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (texture),
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
damage_control: __attribute__((unused))
      texture = NULL;
    }

  g_object_unref (pixbuf);
  return texture;
}

/* Loads @fname, resizing and cropping it as necessary to fit
 * in a @aw x @ah rectangle.  Returns %NULL on error. */
static ClutterActor *
load_image (char const * fname, guint aw, guint ah)
{
  GError *err;
  GdkPixbuf *pixbuf;
  gint dx, dy;
  gdouble dsx, dsy, scale;
  guint vw, vh, sw, sh, dw, dh;
  ClutterActor *final;
  ClutterActor *texture;

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
                            gdk_pixbuf_get_has_alpha (pixbuf),
                            gdk_pixbuf_get_bits_per_sample (pixbuf),
                            dw, dh);
      gdk_pixbuf_scale (pixbuf, tmp, 0, 0,
                        dw, dh, dx, dy, scale, scale,
                        GDK_INTERP_BILINEAR);
      g_object_unref (pixbuf);
      pixbuf = tmp;
    }

  if (!(texture = pixbuf2texture (pixbuf)))
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
        clutter_actor_set_x (texture, (vw - dw) / 2);
      if (dh < vh)
        clutter_actor_set_y (texture, (vh - dh) / 2);

      final = clutter_group_new ();
      clutter_container_add (CLUTTER_CONTAINER (final),
                             bg, texture, NULL);
    }
  else
    final = texture;

  /* @final is @vw x @vh large, make it appear as if it @aw x @ah. */
  clutter_actor_set_scale (final, (gdouble)aw/vw, (gdouble)ah/vh);

  return final;
}

/* Loads an icon as a #ClutterTexture.  Returns %NULL on error. */
static ClutterActor *
load_icon (const gchar * iname, guint isize)
{
  static const gchar *anyad[2];
  GtkIconInfo *icinf;
  ClutterActor *icon;

  anyad[0] = iname;
  if (!(icinf = gtk_icon_theme_choose_icon (gtk_icon_theme_get_default (),
                                           anyad, isize, 0)))
    return NULL;

  icon = (iname = gtk_icon_info_get_filename (icinf)) != NULL
    ? clutter_texture_new_from_file (iname, NULL) : NULL;
  gtk_icon_info_free (icinf);

  return icon;
}

/* Searches for an icon with name @iname and size @isize.
 * If it can't find or load it returns a hidden actor.
 * Otherwise the icon's texture is cached. */
static ClutterActor *
get_icon (const gchar * iname, guint isize)
{
  static GHashTable *cache;
  ClutterActor *icon;
  gchar *ikey;
  guint w, h;

  if (!iname)
    goto out;

  /* Is it cached?  We can't use %HdClutterCache because that doesn't
   * handle icons and we may need to load the same icon with different
   * sizes. */
  ikey = g_strdup_printf ("%s-%u", iname, isize);
  if (cache && (icon = g_hash_table_lookup (cache, ikey)) != NULL)
    { /* Yeah */
      g_free (ikey);
    }
  else if ((icon = load_icon (iname, isize)) != NULL)
    { /* No, but we could load it. */
      if (!cache)
        cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, g_object_unref);
      g_hash_table_insert (cache, ikey, icon);
    }
  else
    { /* Couldn't load it. */
      g_free (ikey);
      g_critical ("%s: failed to load icon", iname);
      goto out;
    }

  /* Icon found.  Set its anchor such that if @icon's real size differs
   * from the requested @isize then @icon would look as if centered on
   * an @isize large area. */
  icon = clutter_clone_texture_new (CLUTTER_TEXTURE (icon));
  clutter_actor_set_name (icon, iname);
  clutter_actor_get_size (icon, &w, &h);
  clutter_actor_move_anchor_point (icon,
                                   (gint)(w-isize)/2, (gint)(h-isize)/2);
  return icon;

out: /* Return something. */
  icon = clutter_rectangle_new ();
  clutter_actor_set_size (icon, isize, isize);
  clutter_actor_hide (icon);
  return icon;
}
/* Graphics loading }}} */

/* Fonts and colors {{{ */
/* Resolves a logical color name to a #GdkColor. */
static void
resolve_logical_color (GdkColor * actual_color, const gchar * logical_name)
{
  GtkStyle *style;

  style = gtk_rc_get_style_by_paths (gtk_settings_get_default (),
                                     NULL, NULL,
                                     GTK_TYPE_WIDGET);
  if (!style || !gtk_style_lookup_color (style, logical_name, actual_color))
    { /* Fall back to all-black. */
      g_critical ("%s: unknown color", logical_name);
      memset (actual_color, 0, sizeof (*actual_color));
    }
}

/* Returns a #ClutterColor for a logical color name. */
static void
resolve_clutter_color (ClutterColor * color, const gchar * logical_name)
{
  GdkColor tmp;

  resolve_logical_color (&tmp, logical_name);
  color->red    = tmp.red   >> 8;
  color->green  = tmp.green >> 8;
  color->blue   = tmp.blue  >> 8;
  color->alpha  = 0xFF;
}

/* Returns a font descrition string for a logical font name you can use
 * to create #ClutterLabel:s.  The returned string is yours. */
static gchar *
resolve_logical_font (const gchar * logical_name)
{
  GtkStyle *style;

  style = gtk_rc_get_style_by_paths (gtk_settings_get_default (),
                                     logical_name, NULL, G_TYPE_NONE);
  if (!style)
    { /* Fall back to system font. */
      g_critical("%s: unknown font", logical_name);
      return g_strdup ("Nokia Sans 18");
    }
  else
    return pango_font_description_to_string (style->font_desc);
}
/* Fonts and colors }}} */

/* Clutter utilities {{{ */
/* Animations {{{ */
/* Is @timeline doing doing anything? */
static inline gboolean
animation_in_progress (ClutterTimeline * timeline)
{
  return clutter_timeline_is_playing (timeline);
}

/* Stop activities on @timeline as if it were completed normally. */
static void
stop_animation (ClutterTimeline * timeline)
{
  guint nframes;

  /*
   * Fake "new-frame" and "complete" signals, clutter_timeline_advance()
   * emits neither of them in this case.  Get the timeline out of playing
   * state first, so remove_window() won't defer (again).  We don't use
   * clutter_timline_stop() because that's equivalent to pause()+rewind(),
   * but we need the rewind afterwards.
   */
  clutter_timeline_pause (timeline);
  nframes = clutter_timeline_get_n_frames (timeline);
  clutter_timeline_advance (timeline, nframes);
  g_signal_emit_by_name (timeline, "new-frame", NULL, nframes);
  g_signal_emit_by_name (timeline, "completed", NULL);
  clutter_timeline_rewind (timeline);
}

/* Tells whether we should bother with animation or just setting the
 * @actor's property is enough. */
static gboolean
need_to_animate (ClutterActor * actor)
{
  if (!hd_task_navigator_is_active ())
    /* Navigator is not visible, animation wouldn't be seen. */
    return FALSE;
  else if (CLUTTER_IS_GROUP (actor))
    /* Don't fly empty groups. */
    return clutter_group_get_n_children (CLUTTER_GROUP (actor)) > 0;
  else
    return TRUE;
}
/* Animations }}} */

/* Effects infrastructure {{{ */
/* General {{{ */
/* Returns whether @actos has an effect with @frame_fun.  Effects can use it
 * to recognize themselves and modify the existing one rather than starting
 * a new. */
static EffectClosure *
has_effect (ClutterActor * actor, gconstpointer effectid)
{
  guint i;
  EffectClosure *closure;

  if (!Effects)
    return NULL;

  /* Find @actor and @frame_fun in @Effects. */
  for (i = 0; i < Effects->len; i++)
    {
      closure = g_ptr_array_index (Effects, i);
      if (closure->actor == actor && closure->effectid == effectid)
        return closure;
    }

  return NULL;
}

/* Allocates an #EffectClosure and fills in the common fields.
 * Adds signals to @timeline. */
static EffectClosure *
new_effect (ClutterTimeline * timeline, ClutterActor * actor,
  void (*frame_fun)(ClutterTimeline *, gint, EffectClosure *),
  void (*complete_fun)(ClutterTimeline *, EffectClosure *))
{
  EffectClosure *closure;

  closure = g_slice_new (EffectClosure);
  closure->actor = g_object_ref (actor);

  if (frame_fun)
    closure->new_frame_cb_id = g_signal_connect (timeline, "new-frame",
                                                 G_CALLBACK (frame_fun),
                                                 closure);
  else
    closure->new_frame_cb_id = 0;
  closure->timeline_complete_cb_id = g_signal_connect (timeline, "completed",
                                              G_CALLBACK (complete_fun),
                                              closure);

  closure->timeline = g_object_ref (timeline);
  clutter_timeline_start (timeline);

  /* Register @closure in @Effects. */
  if (G_UNLIKELY (!Effects))
    Effects = g_ptr_array_new ();
  closure->effectid = frame_fun;
  g_ptr_array_add (Effects, closure);

  return closure;
}

/* Undoes new_effect().  Must be called at the end of effect.
 * If @timeline is still running the effect is cancelled
 * without further ado. */
static void
free_effect (ClutterTimeline * timeline, EffectClosure * closure)
{
  if (!Effects) /* I'd love to tease kuzak with a grif(:) */
    g_critical ("no Effects");
  else if (!g_ptr_array_remove_fast (Effects, closure))
    g_critical ("closure not in Effects");
  g_assert (timeline == closure->timeline);

  if (closure->new_frame_cb_id)
    g_signal_handler_disconnect (timeline, closure->new_frame_cb_id);
  g_signal_handler_disconnect (timeline, closure->timeline_complete_cb_id);
  g_object_unref (timeline);
  g_object_unref (closure->actor);
  g_slice_free (EffectClosure, closure);
}
/* General }}} */

/* Linear effects {{{ */
/*
 * Start or continue a linear effect on @actor during which one or more
 * properties of @actor are changed linearly depending on the current
 * progress of @timeline.  If @actor doesn't have a @frame_fun effect yet,
 * it starts a new one.  The named parameters have the same meaning as
 * in new_effect().  The varadic argument list should be pairs of %gdouble:s
 * teminated by a NAN.  Each pair describes the endpoint values of a property.
 * If an effect is already running it is altered such that by the end of
 * @timline the properties will reach their final intended values without
 * jumping.  In @frame_fun the current value of the properties can be
 * queried with linear_effect_value().
 */
static EffectClosure *
linear_effect (ClutterTimeline * timeline, ClutterActor * actor,
               void (*frame_fun)(ClutterTimeline *, gint, EffectClosure *),
               void (*complete_fun)(ClutterTimeline *, EffectClosure *),
               ...)
{
  guint i;
  va_list list;
  gfloat init, final;
  EffectClosure *closure;

  va_start (list, complete_fun);
  if (G_LIKELY (!(closure = has_effect (actor, frame_fun))))
    {
      /* @init and @diff are parameters of the line. */
      closure = new_effect (timeline, actor, frame_fun, complete_fun);
      for (i = 0; !isnanf (init = va_arg (list, gdouble)); i++)
        { g_assert (i < G_N_ELEMENTS (closure->linear));
          closure->linear[i].init = init;
          closure->linear[i].diff = va_arg (list, gdouble) - init;
        }
    }
  else
    {
      /*
       * Calculate @init2 and @diff2 from equations:
       * init1 + diff1*now  == init2 + diff2*now,
       * final2             == init2 + diff2.
       */
      gfloat now = clutter_timeline_get_progress (timeline);
      for (i = 0; !isnanf (init = va_arg (list, gdouble)); i++)
        { g_assert (i < G_N_ELEMENTS (closure->linear));
          final = va_arg (list, gdouble);
          closure->linear[i].diff = (final-init) / (1-now);
          closure->linear[i].init = final - closure->linear[i].diff;
        }
    }
  va_end (list);

  return closure;
}

/* Return the value of @which property at @now. */
static inline gfloat __attribute__((pure))
linear_effect_value (const EffectClosure * closure, guint which, gfloat now)
{ g_assert (which < G_N_ELEMENTS (closure->linear));
  return closure->linear[which].init + closure->linear[which].diff*now;
}
/* Linear effects }}} */

/* Effect closures {{{ */
/* add_effect_closure()'s #ClutterTimeline::completed handler. */
static void
call_effect_closure (ClutterTimeline * timeline,
                     EffectCompleteClosure *closure)
{
  g_signal_handler_disconnect (timeline, closure->handler_id);
  closure->fun (closure->actor, closure->funparam);
  g_object_unref (closure->actor);
  g_slice_free (EffectCompleteClosure, closure);
}

/* If @fun is not %NULL call it with @actor and @funparam when
 * @timeline is "completed".  Otherwise NOP. */
static void
add_effect_closure (ClutterTimeline * timeline,
                    ClutterEffectCompleteFunc fun,
                    ClutterActor * actor, gpointer funparam)
{
  EffectCompleteClosure *closure;

  if (!fun)
    return;

  closure = g_slice_new (EffectCompleteClosure);
  closure->fun        = fun;
  closure->actor      = g_object_ref (actor);
  closure->funparam   = funparam;
  closure->handler_id = g_signal_connect (timeline, "completed",
                                          G_CALLBACK (call_effect_closure),
                                          closure);
}
/* Effect closures }}} */
/* }}} */

/* RMS effects {{{ */
/*
 * In this section we define RMS effects: move, resize, scale.
 * In purpose they are similar to clutter_effect_move() etc.
 * but additionally their destination can be changed on the go,
 * allowing for smooth animations.  This is permitted by the
 * linear_effect() machinery.
 *
 * Here we define:
 * -- check_and_move(),   move(),   move_effect()
 * -- check_and_resize(), resize(), resize_effect()
 * -- check_and_scale(),  scale(),  scale_effect()
 *
 * Where:
 * -- RMS_effect(@timeline, @actor, @x, @y):
 *    Start an effect that sees @actor to @x and @y.  If @actor already
 *    has such an effect divert it to end up at this destination.
 * -- RMS(@actor, @x, @y):
 *    See @actor to @x and @y, either animated or not.  For animation
 *    it uses @Fly_effect_timeline.  We don't animate when the switcher
 *    is not active, we just drop @actor where it is destined to.
 * -- check_and_RMS(@actor, @x, @y):
 *    Like RMS but don't do anything if @actor's already at @x and @y.
 *    Cancels the already running effect if any.  This saves an effect
 *    when we would otherwise animate.
 */

/* This beautiful macro defines effect() and effect_effect().
 * @clutter_get_fun must have a signature (#ClutterActor, ptype*, ptype*),
 * while @clutter_set_fun is (#ClutterActor, ptype, ptype). */
#define DEFINE_RMS_EFFECT(effect, ptype,                            \
                          clutter_get_fun, clutter_set_fun)         \
/* @effect's %ClutterTimeline::new-frame callback. */               \
static void                                                         \
effect##_effect_frame (ClutterTimeline * timeline, gint frame,      \
                       EffectClosure * closure)                     \
{                                                                   \
  gfloat now = clutter_timeline_get_progress (timeline);            \
  clutter_set_fun (closure->actor,                                  \
                   linear_effect_value (closure, 0, now),           \
                   linear_effect_value (closure, 1, now));          \
}                                                                   \
                                                                    \
static void                                                         \
effect##_effect (ClutterTimeline * timeline, ClutterActor * actor,  \
                 ptype final1, ptype final2)                        \
{                                                                   \
  ptype init1, init2;                                               \
                                                                    \
  clutter_get_fun (actor, &init1, &init2);                          \
  linear_effect (timeline, actor,                                   \
                 effect##_effect_frame, free_effect,                \
                 (gdouble)init1, (gdouble)final1,                   \
                 (gdouble)init2, (gdouble)final2,                   \
                 NAN);                                              \
}                                                                   \
                                                                    \
static void                                                         \
effect (ClutterActor * actor, ptype final1, ptype final2)           \
{                                                                   \
  if (need_to_animate (actor))                                      \
    effect##_effect (Fly_effect_timeline, actor, final1, final2);   \
  else                                                              \
    clutter_set_fun (actor, final1, final2);                        \
}

DEFINE_RMS_EFFECT(move, gint,
                  clutter_actor_get_position, clutter_actor_set_position);
static void
check_and_move (ClutterActor * actor, gint xpos_new, gint ypos_new)
{
  EffectClosure *closure;
  gint xpos_now, ypos_now;

  clutter_actor_get_position (actor, &xpos_now, &ypos_now);
  if (xpos_now != xpos_new || ypos_now != ypos_new)
    move (actor, xpos_new, ypos_new);
  else if ((closure = has_effect (actor, move_effect_frame)) != NULL)
    free_effect (closure->timeline, closure);
}

/* On the gadget (or maybe in general if we're accelerated) we can't
 * resize continously because it blocks all effects and doesn't come
 * about anyway.  It's so even if we don't clip_on_resize(). */
#ifdef __i386__
DEFINE_RMS_EFFECT(resize, guint,
                  clutter_actor_get_size, clutter_actor_set_size);
static void
check_and_resize (ClutterActor * actor, gint width_new, gint height_new)
{
  EffectClosure *closure;
  guint width_now, height_now;

  clutter_actor_get_size (actor, &width_now, &height_now);
  if (width_now != width_new || height_now != height_new)
    resize (actor, width_new, height_new);
  else if ((closure = has_effect (actor, resize_effect_frame)) != NULL)
    free_effect (closure->timeline, closure);
}
#else /* __armel__ */
static void resize_effect_complete (ClutterTimeline * timeline,
                                    EffectClosure * closure)
{
  clutter_actor_set_size (closure->actor, closure->width, closure->height);
  free_effect (timeline, closure);
}

static void
resize_effect (ClutterTimeline * timeline, ClutterActor * actor,
                 guint wfinal, guint hfinal)
{
  guint width, height;
  EffectClosure *closure;

  closure = has_effect (actor, resize_effect);
  clutter_actor_get_size (actor, &width, &height);

  /* Resize now if the final dimension is shorter than the current.
   * Otherwise postpone until the end of effect and don't do anything
   * meanwhile. */
  if (wfinal < width && hfinal < height)
    {
      clutter_actor_set_size (actor, wfinal, hfinal);
      if (closure)
        free_effect (timeline, closure);
      return;
    }
  else if (wfinal < width)
    clutter_actor_set_width (actor, wfinal);
  else if (hfinal < height)
    clutter_actor_set_height (actor, hfinal);

  if (!closure)
    closure = new_effect (timeline, actor, NULL, resize_effect_complete);
  closure->width  = wfinal;
  closure->height = hfinal;
  clutter_timeline_start (timeline);
}

static void
resize (ClutterActor * actor, guint width, guint height)
{
  if (need_to_animate (actor))
    resize_effect (Fly_effect_timeline, actor, width, height);
  else
    clutter_actor_set_size (actor, width, height);
}

static void
check_and_resize (ClutterActor * actor, gint width_new, gint height_new)
{
  EffectClosure *closure;
  guint width_now, height_now;

  clutter_actor_get_size (actor, &width_now, &height_now);
  if (width_now != width_new || height_now != height_new)
    resize (actor, width_new, height_new);
  else if ((closure = has_effect (actor, resize_effect)) != NULL)
    free_effect (closure->timeline, closure);
}
#endif /* __armel__ */

DEFINE_RMS_EFFECT(scale, gdouble,
                  clutter_actor_get_scale, clutter_actor_set_scale);
static void
check_and_scale (ClutterActor * actor, gdouble sx_new, gdouble sy_new)
{
  EffectClosure *closure;
  gdouble sx_now, sy_now;

  /* Beware the rounding errors. */
  clutter_actor_get_scale (actor, &sx_now, &sy_now);
  if (fabs (sx_now - sx_new) > 0.0001 || fabs (sy_now - sy_new) > 0.0001)
    scale (actor, sx_new, sy_new);
  else if ((closure = has_effect (actor, scale_effect_frame)) != NULL)
    free_effect (closure->timeline, closure);
}
/* RMS effects }}} */

/* Boom effect {{{ */
/* If @x0 <= @t <= @x1 returns the value of f(@t), where f()
 * goes from (@x0, @y0) to (@x1, @y1) following a cosine curve.
 * Do the math if you don't believe. */
static inline __attribute__((const)) gdouble
turnoff_fun (gdouble x0, gdouble y0, gdouble x1, gdouble y1, gdouble t)
{
  gdouble a, c, d;

  d = cos (x1) / cos (x0);
  c = (y1 - d) / (y0 - d);
  a = (y0 - c) / cos (x0);
  return a*cos(t) + c;
}

/* ClutterTimeline::new-frame callback for turnoff_effect(). */
static void
turnoff_effect_frame (ClutterTimeline * timeline, gint frame,
                      EffectClosure * closure)
{
  gdouble now;

  // thwin scale-y    0.0 .. 0.5 cosine 1.0 .. 0.1
  // thwin scale-x    0.3 .. 0.8 cosine 1.0 .. 0.1
  // thwin opacity    0.5 .. 1.0 linear 255 .. 0.0
  // particle opacity 0.5 .. 1.0 sine   0.0 .. 1.0 .. 0.0
  // particle radius  0.5 .. 1.0 cosine 8.0 .. 72
  // particle angle   0.5 .. 1.0 linear 0.0 .. PI/2
  now = clutter_timeline_get_progress (timeline);

  /* @thwin */
  if (now <= 0.8)
    clutter_actor_set_scale (closure->actor,
                 now <= 0.3 ? 1.0 : turnoff_fun (0.3, 1, 0.8, 0.1, now),
                 now >= 0.5 ? 0.1 : turnoff_fun (0.0, 1, 0.5, 0.1, now));
  if (0.5 <= now)
    clutter_actor_set_opacity (closure->actor, 510 - 510*now);

  /* @particles */
  if (0.5 <= now)
    {
      guint i;
      gdouble t, all_rad;

      t = 2*now-1;
      all_rad = turnoff_fun (0.5, 8, 1, 72, now);
      for (i = 0; i < G_N_ELEMENTS (closure->particles); i++)
        {
          gdouble ang, rad;

          ang = closure->particles[i].ang0 + M_PI/2 * t;
          rad = all_rad * i/G_N_ELEMENTS (closure->particles);
          clutter_actor_set_position (closure->particles[i].particle,
                                      cos(ang)*rad, sin(ang)*rad);
        }

      clutter_actor_set_opacity (closure->all_particles, 255*sin(M_PI*t));
      if (!CLUTTER_ACTOR_IS_VISIBLE (closure->all_particles))
        clutter_actor_show (closure->all_particles);
    }
}

/* ClutterTimeline::completed callback for turnoff_effect(). */
static void
turnoff_effect_complete (ClutterTimeline * timeline,
                         EffectClosure * closure)
{
  clutter_container_remove_actor (CLUTTER_CONTAINER (Navigator),
                                  closure->all_particles);
  free_effect (timeline, closure);
}

/*
 * Do a TV-turned-off effect on @thwin: first squeeze it vertically,
 * then horizontally.  At the same time it is horozontally scaled
 * fade it out gradually and show little sparkles in the background.
 * The sparkles are dancing in an every growing circle.  They fade in
 * then fade out smoothly.
 */
static void
turnoff_effect (ClutterTimeline * timeline, ClutterActor * thwin)
{
  guint i;
  gint centerx, centery;
  EffectClosure *closure;

  closure = new_effect (timeline, thwin,
                        turnoff_effect_frame,
                        turnoff_effect_complete);

  /* Scale @thwin in the middle. */
  clutter_actor_move_anchor_point_from_gravity (thwin,
                                                CLUTTER_GRAVITY_CENTER);

  /* Create @closure->all_particles.  Place it at the center of @thwin
   * and hide it initially because particles are shown later during the
   * effect. */
  clutter_actor_get_position (thwin,  &centerx, &centery);
  centery -= hd_scrollable_group_get_viewport_y (Navigator_area);
  closure->all_particles = clutter_group_new ();
  clutter_actor_set_position (closure->all_particles, centerx, centery);
  clutter_container_add_actor (CLUTTER_CONTAINER (Navigator),
                               closure->all_particles);
  clutter_actor_hide (closure->all_particles);

  /* Create @closure->particles and add them to @closure->all_particles. */
  for (i = 0; i < G_N_ELEMENTS (closure->particles); i++)
    {
      ClutterActor *particle;

      particle = hd_clutter_cache_get_texture (HD_THEME_IMG_CLOSING_PARTICLE,
                                               TRUE);
      clutter_actor_set_anchor_point_from_gravity (particle,
                                                   CLUTTER_GRAVITY_CENTER);
      clutter_container_add_actor (CLUTTER_CONTAINER (closure->all_particles),
                                   particle);

      /* All particles has an own initial angle from which they go half
       * a circle until the end of animimation. */
      closure->particles[i].ang0 = 2*M_PI * g_random_double ();
      closure->particles[i].particle = particle;
    }
}
/* Boom effect }}} */

/* Misc {{{ */
/* #ClutterActor::notify::allocation callback to clip @actor to its size
 * whenever it changes.  Used to clip ClutterLabel:s to their allocated
 * size. */
static void clip_on_resize (ClutterActor * actor)
{
  ClutterUnit width, height;

  clutter_actor_get_sizeu (actor, &width, &height);
  clutter_actor_set_clipu (actor, 0, 0, width, height);
}

/* Utility function to set up or change a #ClutterLabel. */
static ClutterActor *
set_label_text_and_color (ClutterActor * label, const char * newtext,
                          const ClutterColor * color)
{
  const gchar *text;

  /* Only change the text if it's different from the current one.
   * Setting a #ClutterLabel's text causes relayout. */
  if (color)
    clutter_label_set_color (CLUTTER_LABEL (label), color);
  if (newtext && (!(text = clutter_label_get_text (CLUTTER_LABEL (label)))
                  || strcmp (newtext, text)))
    clutter_label_set_text (CLUTTER_LABEL (label), newtext);
  return label;
}

/* Hide @actor and only show it after the flying animation finished.
 * Used when a new thing is created but you need to move other things
 * out of the way to make room for it. */
static void
show_when_complete (ClutterActor * actor)
{
  clutter_actor_hide (actor);
  add_effect_closure (Fly_effect_timeline,
                      (ClutterEffectCompleteFunc)clutter_actor_show,
                      actor, NULL);
}
/* Misc }}} */
/* Clutter utilities }}} */

/* Navigator utilities {{{ */
/* Tells whether the switcher is shown. */
gboolean
hd_task_navigator_is_active (void)
{
  return CLUTTER_ACTOR_IS_VISIBLE (Navigator);
}

/* Tells whether the navigator is empty. */
gboolean
hd_task_navigator_is_empty (void)
{
  return NThumbnails == 0;
}

/* Returns whether at least thumbnails populate the switcher. */
gboolean
hd_task_navigator_is_crowded (void)
{
  return NThumbnails > 1;
}

/* Tells whether we have any application thumbnails. */
gboolean
hd_task_navigator_has_apps (void)
{
  return Thumbnails && Thumbnails != Notifications;
}

/* Tells whether we have any notification, either in the
 * notification area or shown in the title area of some thumbnail. */
gboolean
hd_task_navigator_has_notifications (void)
{
  return Notifications != NULL;
}

/* Returns whether we can and will show @win in the navigator.
 * @win can be %NULL, in which case this function returns %FALSE. */
gboolean
hd_task_navigator_has_window (HdTaskNavigator * self, ClutterActor * win)
{
  const GList *li;
  const Thumbnail *thumb;

  for_each_thumbnail (li, thumb)
    if (thumb->apwin == win)
      return TRUE;
  return FALSE;
}

ClutterActor *
hd_task_navigator_find_app_actor (HdTaskNavigator * self, const gchar * id)
{
  const GList *li;
  const Thumbnail *thumb;

  for_each_appthumb (li, thumb)
    {
      const gchar *appid;

      appid = g_object_get_data (G_OBJECT (thumb->apwin), "HD-ApplicationId");
      if (appid && !g_strcmp0 (appid, id))
        return thumb->apwin;
    }

  return NULL;
}

/* Scroll @Navigator_area to the top. */
void
hd_task_navigator_scroll_back (HdTaskNavigator * self)
{
  hd_scrollable_group_set_viewport_y (Navigator_area, 0);
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

/* Layout engine {{{ */
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
static inline gint __attribute__ ((const))
layout_fun (gint total, gint term1, gint term2, gint factor)
{
  /* Make sure all terms and factors are int:s because the result of
   * the outer subtraction can be negative and division is sensitive
   * to signedness. */
  return (total - (term1*factor + term2*(factor - 1))) / 2;
}

/* Calculates the layout of the thumbnails and fills in @lout.
 * The layout depends on the number of thumbnails. */
static void
calc_layout (Layout * lout)
{
  guint nrows_per_page;

  /* Figure out how many thumbnails to squeeze into one row
   * (not the last one, which may be different) and the maximum
   * number of fully visible rows at a time. */
  if (NThumbnails <= 3)
    {
      lout->thumbsize = NThumbnails <= 2
        ? &Thumbsizes.large : &Thumbsizes.medium;
      lout->cells_per_row = NThumbnails;
      nrows_per_page = 1;
    }
  else if (NThumbnails <= 6)
    {
      lout->thumbsize = &Thumbsizes.medium;
      lout->cells_per_row = 3;
      nrows_per_page = 2;
    }
  else
    {
      lout->thumbsize = &Thumbsizes.small;
      lout->cells_per_row = 4;
      nrows_per_page = NThumbnails <= 8 ? 2 : 3;
    }

  /* Gaps are always the same, regardless of the number of thumbnails.
   * Leave the last row left-aligned.  Center the first pageful amount
   * of rows vertically. */
  lout->xpos = layout_fun (SCREEN_WIDTH,
                           lout->thumbsize->width,
                           GRID_HORIZONTAL_GAP,
                           lout->cells_per_row);
  lout->last_row_xpos = lout->xpos;
  lout->ypos = layout_fun (SCREEN_HEIGHT + GRID_TOP_MARGIN,
                           lout->thumbsize->height,
                           GRID_VERTICAL_GAP,
                           nrows_per_page);
  lout->hspace = lout->thumbsize->width  + GRID_HORIZONTAL_GAP;
  lout->vspace = lout->thumbsize->height + GRID_VERTICAL_GAP;
}

/* Depending on the current @Thumbsize places the frame graphics
 * elements (except .center) of @thumb where they should be. */
static void
layout_thumb_frame (const Thumbnail * thumb, const Flyops * ops)
{
  guint wt, ht, wb, hb;

  /* This is quite boring. */
  clutter_actor_get_size(thumb->frame.north_west, &wt, &ht);
  clutter_actor_get_size(thumb->frame.south_west, &wb, &hb);

  ops->move (thumb->frame.north,      wt, 0);
  ops->move (thumb->frame.north_east, Thumbsize->width, 0);
  ops->move (thumb->frame.west,       0, ht);
  ops->move (thumb->frame.east,       Thumbsize->width, ht);
  ops->move (thumb->frame.south_west, 0, Thumbsize->height);
  ops->move (thumb->frame.south,      wb, Thumbsize->height);
  ops->move (thumb->frame.south_east, Thumbsize->width, Thumbsize->height);

  ops->scale (thumb->frame.north,
              (gdouble)(Thumbsize->width - 2*wt)
                / clutter_actor_get_width (thumb->frame.north), 1);
  ops->scale (thumb->frame.south,
              (gdouble)(Thumbsize->width - 2*wb)
                / clutter_actor_get_width (thumb->frame.south), 1);
  ops->scale (thumb->frame.west, 1,
              (gdouble)(Thumbsize->height - (ht+hb))
                / clutter_actor_get_height (thumb->frame.west));
  ops->scale (thumb->frame.east, 1,
              (gdouble)(Thumbsize->height - (ht+hb))
                / clutter_actor_get_height (thumb->frame.east));
}

/*
 * Lays out @Thumbnails on @Navigator_area, and their inner portions.
 * Makes actors fly if it's appropriate.  @newborn is either a new thumbnail
 * or notification to be displayed; it won't be animated.  Returns the
 * position of the bottom of the lowest thumbnail.  Also sets @Thumbsize.
 */
static guint
layout_thumbs (ClutterActor * newborn)
{
  Layout lout;
  guint maxwtitle;
  const GList *li;
  Thumbnail *thumb;
  guint xthumb, ythumb, i;
  const GtkRequisition *oldthsize;

  /* Save the old @Thumbsize to know if it's changed. */
  calc_layout (&lout);
  oldthsize = Thumbsize;
  Thumbsize = lout.thumbsize;

  /* Clip titles longer than this. */
  maxwtitle = Thumbsize->width
    - (TITLE_LEFT_MARGIN + TITLE_RIGHT_MARGIN + CLOSE_ICON_SIZE);

  /* Place and scale each thumbnail row by row. */
  xthumb = ythumb = 0xB002E;
  for (li = Thumbnails, i = 0; li && (thumb = li->data); li = li->next, i++)
    {
      const Flyops *ops;
      guint wprison, hprison;

      /* If it's a new row re/set @ythumb and @xthumb. */
      if (!(i % lout.cells_per_row))
        {
          if (i == 0)
            /* This is the very first row. */
            ythumb = lout.ypos;
          else
            ythumb += lout.vspace;

          /* Use @last_row_xpos if it's the last row. */
          xthumb = i + lout.cells_per_row <= NThumbnails
            ? lout.xpos : lout.last_row_xpos;
        }

      /* If @thwin's been there, animate as it's moving.  Otherwise if it's
       * a new one to enter the navigator, don't, it's hidden anyway. */
      ops = thumb->thwin == newborn ? &Fly_at_once : &Fly_smoothly;

      /* Place @thwin in any case. */
      ops->move (thumb->thwin, xthumb, ythumb);

      /* If @Thumbnails are not changing size and this is not a newborn
       * the inners of @thumb are already setup. */
      if (oldthsize == Thumbsize && thumb->thwin != newborn)
        goto skip_the_circus;

      /* Set thumbnail's reaction area. */
      clutter_actor_set_size (thumb->thwin, Thumbsize->width, Thumbsize->height);

      /* @thumb->close */
      ops->move (thumb->close, Thumbsize->width, 0);

      /* Make sure @thumb->title remains inside its confines. */
      ops->resize (thumb->title, maxwtitle,
                   clutter_actor_get_height (thumb->title));

      /* Place @thumb->frame. */
      layout_thumb_frame (thumb, ops);

      /* .prison is right below the title background and reserves
       * some pixels on both sides. */
      wprison = Thumbsize->width - 2*FRAME_WIDTH;
      hprison = Thumbsize->height - (FRAME_TOP_HEIGHT + FRAME_BOTTOM_HEIGHT);

      if (thumb_is_notification (thumb))
        { /* Boring details of laying out the inners of a nothumb. */
          gboolean change;
          guint isize, x, y;

          /* The icon size determine the placement of the inners of
           * notification thumbnails.  If the icon size changes,
           * those change too, but if not, neither the rest. */
          change = thumb->thwin == newborn
            || oldthsize == &Thumbsizes.large
            || Thumbsize == &Thumbsizes.large;

          /* .icon */
          isize = Thumbsize == &Thumbsizes.large
            ? ICON_FINGER : ICON_STYLUS;
          if (change && thumb->icon)
            {
              clutter_container_remove_actor (CLUTTER_CONTAINER (thumb->prison),
                                              thumb->icon);
              thumb->icon = NULL;
            }
          if (!thumb->icon)
            {
              thumb->icon = get_icon (hd_note_get_icon (thumb->tnote->hdnote),
                                      isize);
              clutter_container_add_actor (CLUTTER_CONTAINER (thumb->prison),
                                           thumb->icon);
            }
          if (change)
            ops->move (thumb->icon, MARGIN_DEFAULT, MARGIN_HALF);

          /* .time: %MARGIN_DEFAULT on the left and centered vertically
           * relative to the .icon. */
          if (change)
            {
              clutter_label_set_font_name (CLUTTER_LABEL (thumb->time),
                                           Thumbsize == &Thumbsizes.large
                                             ? SystemFont : SmallSystemFont);
              y = clutter_actor_get_height (thumb->time);
              ops->move (thumb->time,
                         MARGIN_DEFAULT + isize + MARGIN_DEFAULT,
                         MARGIN_HALF    + (isize-y) / 2);
            }

          /* .message: %MARGIN_DEFAULT at left-right and %MARGIN_HALF
           * at top-bottom. */
          x = MARGIN_DEFAULT;
          y = MARGIN_HALF + isize + MARGIN_HALF;
          if (change)
            ops->move (thumb->message, x, y);
          ops->resize (thumb->message,
                       wprison - x - MARGIN_DEFAULT,
                       hprison - y - MARGIN_HALF);

          /* Don't show .message on small thumbnails.
           * TODO fade in/out */
          if (Thumbsize == &Thumbsizes.small)
            clutter_actor_hide (thumb->message);
          else if (oldthsize == &Thumbsizes.small)
            clutter_actor_show (thumb->message);

          /* Background */
          clutter_actor_get_size (thumb->frame.center, &x, &y);
          ops->scale (thumb->frame.center,
                      (gdouble)wprison / x, (gdouble)hprison / y);
        }
      else
        ops->scale (thumb->prison,
                    (gdouble)wprison / thumb->inapwin.width,
                    (gdouble)hprison / thumb->inapwin.height);

skip_the_circus:
      xthumb += lout.hspace;
    }

  return ythumb + Thumbsize->height;
}

/* Lays out the @Thumbnails in @Navigator_area. */
static void
layout (ClutterActor * newborn)
{
  /* This layout machinery is based on invariants, which basically
   * means we don't pay much attention to what caused the layout
   * update, but we rely on the current state of matters. */
  set_navigator_height (layout_thumbs (newborn));

  if (newborn && animation_in_progress (Fly_effect_timeline))
    show_when_complete (newborn);
}
/* Layout engine }}} */

/* %Thumbnail:s {{{ */
static MBWMClientWindow *actor_to_client_window (ClutterActor *win,
                                       const HdCompMgrClient **hcmgrcp);

/*
 * Reset @thumb's title to the application's name.  Called to set the
 * initial title of a thumbnail (if it has no notifications otherwise)
 * or when it had, but that notification is removed from the switcher.
 */
static void
reset_thumb_title (Thumbnail * thumb)
{
  const gchar *new_title;
  const MBWMClientWindow *mbwmcwin;

  /* What to reset the title to? */
  if (thumb_has_notification (thumb))
    new_title = hd_note_get_summary (thumb->tnote->hdnote);
  else if (thumb->saved_title)
    /* Client must be having its sweet dreams in hibernation. */
    new_title = thumb->saved_title;
  else if ((mbwmcwin = actor_to_client_window (thumb->apwin, NULL)) != NULL)
    /* Normal case. */
    new_title = mbwmcwin->name;
  else
    new_title = NULL;

  g_assert (thumb->title != NULL);
  set_label_text_and_color (thumb->title, new_title, thumb->tnote
                            ? &ReversedTextColor : &DefaultTextColor);
}

/* Dress a %Thumbnails: create @thumb->frame.all and populate it
 * with frame graphics. */
static void
create_thumb_frame (Thumbnail * thumb)
{
  static ClutterGravity const gravities[] =
  {
    CLUTTER_GRAVITY_NORTH_WEST,             /* NW */
    CLUTTER_GRAVITY_NORTH_WEST,             /* N  */
    CLUTTER_GRAVITY_NORTH_EAST,             /* NE */
    CLUTTER_GRAVITY_NORTH_WEST,             /* W  */
    CLUTTER_GRAVITY_NORTH_WEST,             /* C  */
    CLUTTER_GRAVITY_NORTH_EAST,             /* E  */
    CLUTTER_GRAVITY_SOUTH_WEST,             /* SW */
    CLUTTER_GRAVITY_SOUTH_WEST,             /* S  */
    CLUTTER_GRAVITY_SOUTH_EAST,             /* SE */
  };
  static const gchar * const apthumb_fnames[] =
  {
    "TaskSwitcherThumbnailTitleLeft.png",
    "TaskSwitcherThumbnailTitleCenter.png",
    "TaskSwitcherThumbnailTitleRight.png",
    "TaskSwitcherThumbnailBorderLeft.png",
    NULL,
    "TaskSwitcherThumbnailBorderRight.png",
    "TaskSwitcherThumbnailBottomLeft.png",
    "TaskSwitcherThumbnailBottomCenter.png",
    "TaskSwitcherThumbnailBottomRight.png",
  }, * const nothumb_fnames[] =
  { /* These are also used for %APPLICATION %Thumbnails
     * which have a %TNote. */
    "TaskSwitcherNotificationTitleLeft.png",
    "TaskSwitcherNotificationTitleCenter.png",
    "TaskSwitcherNotificationTitleRight.png",
    "TaskSwitcherNotificationBorderLeft.png",
    "TaskSwitcherNotificationBackground.png",
    "TaskSwitcherNotificationBorderRight.png",
    "TaskSwitcherNotificationBottomLeft.png",
    "TaskSwitcherNotificationBottomCenter.png",
    "TaskSwitcherNotificationBottomRight.png",
  };

  guint i;
  const gchar * const * fnames;

  fnames = thumb_has_notification (thumb) ? nothumb_fnames : apthumb_fnames;

  thumb->frame.all = clutter_group_new ();
  clutter_actor_set_name (thumb->frame.all, "thumbnail frame");

  for (i = 0; i < G_N_ELEMENTS (gravities); i++)
    {
      if (thumb_is_application (thumb)
          && &thumb->frame.pieces[i] == &thumb->frame.center)
        {
          /* Skip the .center element for application thumbnails. */
          thumb->frame.center = NULL;
          continue;
        }

      g_assert (fnames[i] != NULL);
      thumb->frame.pieces[i] = hd_clutter_cache_get_texture (fnames[i], TRUE);
      clutter_actor_set_anchor_point_from_gravity (thumb->frame.pieces[i],
                                                   gravities[i]);
      clutter_container_add_actor (CLUTTER_CONTAINER (thumb->frame.all),
                                   thumb->frame.pieces[i]);
    }

  /* The @center piece is the background of the @prison.
   * This is why @frmame.all needs to be ordered below it. */
  if (thumb->frame.center)
    clutter_actor_set_position (thumb->frame.center,
                                PRISON_XPOS, PRISON_YPOS);
}

/* Replace a %Thumbnail's @frame.  This is necessary when an
 * application thumbnail gets or loses its notification. */
static void
recreate_thumb_frame (Thumbnail * thumb)
{
  ClutterActor *oldframe;

  /* Make sure the new frame is layered where the old was. */
  oldframe = thumb->frame.all;
  create_thumb_frame (thumb);
  clutter_container_add_actor (CLUTTER_CONTAINER (thumb->plate),
                               thumb->frame.all);
  clutter_actor_raise (thumb->frame.all, oldframe);
  clutter_container_remove_actor (CLUTTER_CONTAINER (thumb->plate),
                                  oldframe);

  layout_thumb_frame (thumb, &Fly_at_once);
}

/* Creates @thumb->thwin.  The exact position of the inner actors is decided
 * by layout_thumbs(). */
static void
create_thwin (Thumbnail * thumb)
{
  /* .prison */
  clutter_actor_set_name (thumb->prison, "prison");
  clutter_actor_set_position (thumb->prison,
                              PRISON_XPOS, PRISON_YPOS);

  /* .frame */
  create_thumb_frame (thumb);

  /* .title */
  thumb->title = clutter_label_new ();
  clutter_label_set_font_name (CLUTTER_LABEL (thumb->title), SmallSystemFont);
  clutter_label_set_use_markup(CLUTTER_LABEL(thumb->title), TRUE);
  clutter_actor_set_anchor_point_from_gravity (thumb->title, CLUTTER_GRAVITY_WEST);
  clutter_actor_set_position (thumb->title,
                              TITLE_LEFT_MARGIN, TITLE_HEIGHT / 2);
  g_signal_connect (thumb->title, "notify::allocation",
                    G_CALLBACK (clip_on_resize), NULL);
  reset_thumb_title (thumb);

  /* .close, anchored at the top-right corner of the close graphics. */
  thumb->close = clutter_group_new ();
  clutter_actor_set_size (thumb->close, CLOSE_AREA_SIZE, CLOSE_AREA_SIZE);
  clutter_actor_set_anchor_point (thumb->close,
                                  CLOSE_AREA_SIZE/2 + CLOSE_ICON_SIZE/2,
                                  CLOSE_AREA_SIZE/2 - CLOSE_ICON_SIZE/2);
  clutter_actor_set_reactive (thumb->close, TRUE);

  /* .plate */
  thumb->plate = clutter_group_new ();
  clutter_actor_set_name (thumb->plate, "plate");
  clutter_container_add (CLUTTER_CONTAINER (thumb->plate),
                         thumb->frame.all, thumb->title, thumb->close, NULL);

  /* .thwin: it is important that .plate is ordered below .prison
   * because of the center element of the frame. */
  thumb->thwin = clutter_group_new ();
  clutter_actor_set_name (thumb->thwin, "thumbnail");
  clutter_actor_set_reactive (thumb->thwin, TRUE);
  clutter_container_add (CLUTTER_CONTAINER (thumb->thwin),
                         thumb->plate, thumb->prison, NULL);
  clutter_container_add_actor (CLUTTER_CONTAINER (Navigator_area),
                               thumb->thwin);
}

/* Release everything related to @thumb. */
static void
free_thumb (Thumbnail * thumb)
{
  /* This will kill the entire actor hierarchy. */
  clutter_container_remove_actor (CLUTTER_CONTAINER (Navigator_area),
                                  thumb->thwin);

  /* The caller must have taken care of .tnote already. */
  g_assert (!thumb_has_notification (thumb));

  if (thumb_is_application (thumb))
    {
      if (thumb->apwin)
        g_object_unref (thumb->apwin);

      if (thumb->dialogs)
        {
          g_ptr_array_foreach (thumb->dialogs, (GFunc)g_object_unref, NULL);
          g_ptr_array_free (thumb->dialogs, TRUE);
          thumb->dialogs = NULL;
        }

      g_free(thumb->saved_title);
      if (thumb->nodest)
        XFree (thumb->nodest);
    }

  g_free (thumb);
}
/* %Thumbnail:s }}} */

/* Application thumbnails {{{ */
/* Child adoption {{{ */
/* Returns whether the application represented by @apthumb has
 * a video screenshot and it should be loaded or reloaded.
 * If so it refreshes @apthumb->video_mtime. */
static gboolean
need_to_load_video (Thumbnail * apthumb)
{
  /* Already has a video loaded? */
  if (apthumb->video)
    {
      struct stat sbuf;
      gboolean clear_video, load_video;

      /* Refresh or unload it? */
      load_video = clear_video = FALSE;
      g_assert (apthumb->video_fname);
      if (stat (apthumb->video_fname, &sbuf) < 0)
        {
          if (errno != ENOENT)
            g_warning ("%s: %m", apthumb->video_fname);
          clear_video = TRUE;
        }
      else if (sbuf.st_mtime > apthumb->video_mtime)
        {
          clear_video = load_video = TRUE;
          apthumb->video_mtime = sbuf.st_mtime;
        }

      if (clear_video)
        {
          clutter_container_remove_actor (CLUTTER_CONTAINER (apthumb->prison),
                                          apthumb->video);
          apthumb->video = NULL;
        }

      return load_video;
    }
  else if (apthumb->video_fname)
    {
      struct stat sbuf;

      /* Do we need to load it? */
      if (stat (apthumb->video_fname, &sbuf) == 0)
        {
          apthumb->video_mtime = sbuf.st_mtime;
          return TRUE;
        }
      else if (errno != ENOENT)
        g_warning ("%s: %m", apthumb->video_fname);
    }

  return FALSE;
}

/* Start managing @apthumb's application window and loads/reloads its
 * last-frame video screenshot if necessary.  Called when we enter
 * the switcher or when a new window is added in switcher view. */
static void
claim_win (Thumbnail * apthumb)
{
  g_assert (hd_task_navigator_is_active ());

  /*
   * Take @apthumb->apwin into our care even if there is a video screenshot.
   * If we don't @apthumb->apwin will be managed by its current parent and
   * we cannot force hiding it which would result in a full-size apwin
   * appearing in the background if it's added in switcher mode.
   * TODO This may not be true anymore.
   */
  clutter_actor_reparent(apthumb->apwin, apthumb->windows);
  if (apthumb->dialogs)
    g_ptr_array_foreach (apthumb->dialogs,
                         (GFunc)clutter_actor_reparent,
                         apthumb->windows);

  /* Load the video screenshot and place its actor in the hierarchy. */
  if (need_to_load_video (apthumb))
    {
      g_assert (!apthumb->video);
      apthumb->video = load_image (apthumb->video_fname,
                                   apthumb->inapwin.width,
                                   apthumb->inapwin.height);
      if (apthumb->video)
        {
          clutter_actor_set_name (apthumb->video, "video");
          clutter_actor_set_position (apthumb->video,
                                      apthumb->inapwin.x,
                                      apthumb->inapwin.y);
          clutter_container_add_actor (CLUTTER_CONTAINER (apthumb->prison),
                                       apthumb->video);
        }
    }

  if (!apthumb->video)
    /* Show .windows and its contents just in case they aren't. */
    clutter_actor_show_all (apthumb->windows);
  else
    /* Only show @apthumb->video. */
    clutter_actor_hide (apthumb->windows);

  /* Make sure @apthumb->title is up-to-date. */
  if (!thumb_has_notification (apthumb))
    reset_thumb_title (apthumb);

  /* Restore the opacity/visibility of the actors that have been faded out
   * while zooming, so we won't have trouble if we happen to to need to enter
   * the navigator directly. */
  clutter_actor_set_opacity (apthumb->plate, 255);
  clutter_actor_hide (apthumb->titlebar);
}

/* Stop managing @apthumb's application window and give it back
 * to its original parent. */
static void
release_win (const Thumbnail * apthumb)
{
  hd_render_manager_return_app (apthumb->apwin);
  if (apthumb->dialogs)
    g_ptr_array_foreach (apthumb->dialogs,
                         (GFunc)hd_render_manager_return_dialog, NULL);
}
/* Child adoption }}} */

/* Managing @Thumbnails {{{ */
/* Return the %Thumbnail whose application window is @apwin. */
static Thumbnail *
find_by_apwin (ClutterActor * apwin)
{
  GList *li;
  Thumbnail *thumb;

  for_each_appthumb (li, thumb)
    if (thumb->apwin == apwin)
      return thumb;

  g_critical ("find_by_apwin(%p): apwin not found", apwin);
  return NULL;
}

/*
 * Find @dialog in one of the @Thumbnails.  If @dialog is @for_removal
 * then its position is returned.  Otherwise @dialog is taken as the
 * parent of a dialog which is transient for @dialog and returns where
 * to insert the new dialog.  This case @dialog can be an application
 * window too.
 */
static Thumbnail *
find_dialog (guint * idxp, ClutterActor * dialog, gboolean for_removal)
{
  GList *li;
  Thumbnail *thumb;

  for_each_appthumb (li, thumb)
    {
      guint o;

      if (!for_removal && thumb->apwin == dialog)
        { /* The dialog will be transient for .apwin. */
          if (idxp)
            *idxp = 0;
          return thumb;
        }

      if (!thumb->dialogs)
        continue;

      for (o = 0; o < thumb->dialogs->len; o++)
          if (thumb->dialogs->pdata[o] == dialog)
            {
              if (idxp)
                *idxp = for_removal ? o : o+1;
              return thumb;
            }
    }

  if (idxp)
    /* If %NULL the caller was just testing. */
    /* This may not be a bug: some dialogs don't end up in the switcher,
     * if they are transient for a dialog, which is system-modal. */
    g_debug ("couldn't find application for dialog %p", dialog);
  return NULL;
}
/* Managing @Thumbnails }}} */

/* Zooming {{{ */
/* add_effect_closure() callback for hd_task_navigator_zoom_in()
 * to leave the navigator. */
static void
zoom_in_complete (ClutterActor * navigator, ClutterActor * apwin)
{
  g_signal_emit_by_name (Navigator, "zoom-in-complete", apwin);
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
  const Thumbnail *apthumb;

  g_assert (hd_task_navigator_is_active ());
  if (!(apthumb = find_by_apwin (win)))
    goto damage_control;

  /*
   * Zoom the navigator itself so that when the effect is complete
   * the non-decoration part of .apwin is in its regular position
   * and size in application view.
   *
   * @xpos, @ypos := .prison's absolute coordinates.
   */
  clutter_actor_get_position  (apthumb->thwin,  &xpos,    &ypos);
  clutter_actor_get_scale     (apthumb->prison, &xscale,  &yscale);
  ypos -= hd_scrollable_group_get_viewport_y (Navigator_area);
  xpos += PRISON_XPOS;
  ypos += PRISON_YPOS;

  clutter_effect_scale (Zoom_effect, Scroller,
                        1 / xscale, 1 / yscale, NULL, NULL);
  clutter_effect_move (Zoom_effect, Scroller,
                       -xpos / xscale + apthumb->inapwin.x,
                       -ypos / yscale + apthumb->inapwin.y,
                       NULL, NULL);

  /* Crossfade .plate with .titlebar. */
  clutter_actor_show (apthumb->titlebar);
  clutter_actor_set_opacity (apthumb->titlebar, 0);
  clutter_effect_fade (Zoom_effect, apthumb->titlebar, 255, NULL, NULL);
  clutter_effect_fade (Zoom_effect, apthumb->plate,      0, NULL, NULL);

  add_effect_closure (Zoom_effect_timeline,
                      (ClutterEffectCompleteFunc)zoom_in_complete,
                      CLUTTER_ACTOR (self), win);
  add_effect_closure (Zoom_effect_timeline,
                      (ClutterEffectCompleteFunc)fun, win, funparam);
  return;

damage_control:
  if (fun != NULL)
    fun (win, funparam);
}

/* Show the navigator and zoom out of @win into it.  @win must have previously
 * been added,  Unless @fun is %NULL @fun(@win, @funparam) is executed when the
 * effect completes. */
void
hd_task_navigator_zoom_out (HdTaskNavigator * self, ClutterActor * win,
                            ClutterEffectCompleteFunc fun, gpointer funparam)
{ g_debug (__FUNCTION__);
  const Thumbnail *apthumb;
  gdouble sxprison, syprison, sxscroller, syscroller;
  gint yarea, xthumb, ythumb, xprison, yprison, xscroller, yscroller;

  /* Our "show" callback will grab the butts of @win. */
  clutter_actor_show (Navigator);
  if (!(apthumb = find_by_apwin (win)))
    goto damage_control;

  /* @xthumb, @ythumb := intended real position of @apthumb */
  g_assert (Thumbsize != NULL);
  clutter_actor_get_position (apthumb->thwin,  &xthumb, &ythumb);

  /*
   * Scroll the @Navigator_area so that @apthumb is closest the middle
   * of the screen.  #HdScrollableGroup does not let us scroll the
   * viewport out of the real estate, but in return we need to ask
   * how much we actually managed to scroll.
   */
  yarea = ythumb - (SCREEN_HEIGHT - Thumbsize->height) / 2;
  hd_scrollable_group_set_viewport_y (Navigator_area, yarea);
  yarea = hd_scrollable_group_get_viewport_y (Navigator_area);

  /* Reposition and rescale the @Scroller so that .apwin is shown exactly
   * in the same position and size as in the application view. */

  /* Make @ythumb absolute (relative to the top of the screen). */
  ythumb -= yarea;

  /* @xprison, @yprison := absolute position of .prison. */
  xprison = xthumb + PRISON_XPOS;
  yprison = ythumb + PRISON_YPOS;
  clutter_actor_get_scale (apthumb->prison, &sxprison, &syprison);

  /* anchor of .prison      <- start of non-decoration area of .apwin
   * visual size of .prison <- width of non-decoration area of .apwin */
  xscroller = apthumb->inapwin.x - xprison/sxprison;
  yscroller = apthumb->inapwin.y - yprison/syprison;
  sxscroller = 1 / sxprison;
  syscroller = 1 / syprison;

  clutter_actor_set_scale     (Scroller, sxscroller, syscroller);
  clutter_actor_set_position  (Scroller,  xscroller,  yscroller);
  clutter_effect_scale (Zoom_effect, Scroller, 1, 1, NULL, NULL);
  clutter_effect_move  (Zoom_effect, Scroller, 0, 0, NULL, NULL);

  /* Crossfade .plate with .titlebar.  It's okay to leave .titlebar shown
   * but transparent. */
  clutter_actor_set_opacity (apthumb->plate, 0);
  clutter_effect_fade (Zoom_effect, apthumb->plate,    255, NULL, NULL);
  clutter_effect_fade (Zoom_effect, apthumb->titlebar,   0, NULL, NULL);
  clutter_actor_show (apthumb->titlebar);

  add_effect_closure (Zoom_effect_timeline, fun, win, funparam);
  return;

damage_control:
  if (fun != NULL)
    fun (win, funparam);
}
/* Zooming }}} */

/* Misc window commands {{{ */
/* Prepare for the client of @win being hibernated.
 * Undone when @win is replaced by the woken-up client's new actor. */
void
hd_task_navigator_hibernate_window (HdTaskNavigator * self,
                                    ClutterActor * win)
{
  Thumbnail *apthumb;
  const MBWMClientWindow *mbwmcwin;

  if (!(apthumb = find_by_apwin (win)))
    return;

  /* Hibernating clients twice is a nonsense. */
  g_return_if_fail (!apthumb->saved_title);

  mbwmcwin = actor_to_client_window (apthumb->apwin, NULL);
  g_return_if_fail (mbwmcwin != NULL);

  /* Save the window name for reset_thumb_title(). */
  g_return_if_fail (mbwmcwin->name);
  apthumb->saved_title = g_strdup (mbwmcwin->name);
}

/* Tells us to show @new_win in place of @old_win, and forget about
 * the latter one entirely.  Replaceing the window doesn't affect
 * its dialogs if any was set earlier. */
void
hd_task_navigator_replace_window (HdTaskNavigator * self,
                                  ClutterActor * old_win,
                                  ClutterActor * new_win)
{ g_debug (__FUNCTION__);
  Thumbnail *apthumb;
  gboolean showing;

  if (old_win == new_win || !(apthumb = find_by_apwin (old_win)))
    return;

  /* Discard the window name we saved when @old_win was hibernated,
   * refer to the new MBWMClientWindow from now on. */
  g_free (apthumb->saved_title);
  apthumb->saved_title = NULL;

  /* Discard current .apwin. */
  showing = hd_task_navigator_is_active ();
  if (showing)
    hd_render_manager_return_app (apthumb->apwin);
  g_object_unref (apthumb->apwin);

  /* Embrace @new_win. */
  apthumb->apwin = g_object_ref (new_win);
  if (showing)
    { /* Don't forget to update the title. */
      clutter_actor_reparent (apthumb->apwin, apthumb->windows);
      if (!thumb_has_notification (apthumb))
        reset_thumb_title (apthumb);
    }
}
/* Misc window commands }}} */

/* Add/remove windows {{{ */
static TNote *remove_nothumb (GList * li, gboolean destroy_tnote);
static Thumbnail *add_nothumb (TNote * tnote);
static gboolean tnote_matches_thumb (const TNote * tnote,
                                     const Thumbnail * thumb);

/* Returns the window whose client's clutter client's texture is @win.
 * If @hcmgrcp is not %NULL also returns the clutter client. */
static MBWMClientWindow *
actor_to_client_window (ClutterActor * win, const HdCompMgrClient **hcmgrcp)
{
  const MBWMCompMgrClient *cmgrc;

  cmgrc = g_object_get_data (G_OBJECT (win),
                             "HD-MBWMCompMgrClutterClient");

  if (!cmgrc || !cmgrc->wm_client || !cmgrc->wm_client->window)
    return NULL;

  if (hcmgrcp)
    *hcmgrcp = HD_COMP_MGR_CLIENT (cmgrc);
  return cmgrc->wm_client->window;
}

/* Set up the @thumb->inapwin-dependant properties of @thumb->prison. */
static void
setup_prison (const Thumbnail * apthumb)
{
  clutter_actor_set_anchor_point (apthumb->prison,
                                  apthumb->inapwin.x, apthumb->inapwin.y);
}

/* Called when a %Thumbnail.thwin is clicked. */
static gboolean
appthumb_clicked (const Thumbnail * apthumb)
{
  if (animation_in_progress (Fly_effect_timeline)
      || animation_in_progress (Zoom_effect_timeline))
    /* Clicking on the thumbnail while it's zooming would result in multiple
     * delivery of "thumbnail-clicked". */
    return TRUE;

  g_signal_emit_by_name (Navigator, "thumbnail-clicked", apthumb->apwin);

  return TRUE;
}

/* Called when a %Thumbnail.close (@thwin's close button) is clicked. */
static gboolean
appthumb_close_clicked (const Thumbnail * apthumb)
{
  if (animation_in_progress (Fly_effect_timeline)
      || animation_in_progress (Zoom_effect_timeline))
    /* Closing an application while it's zooming would crash us. */
    /* Maybe not anymore but let's play safe. */
    return TRUE;

  /* Report a regular click on the thumbnail (and make %HdSwitcher zoom in)
   * if the application has open dialogs. */
  g_signal_emit_by_name (Navigator,
                         apthumb->dialogs && apthumb->dialogs->len > 0
                           ? "thumbnail-clicked" : "thumbnail-closed",
                         apthumb->apwin);
  return TRUE;
}

/* Returns a %Thumbnail for @apwin, a window manager client actor.
 * If there is a notification for this application it will be removed
 * and added as the thumbnail title.  */
static Thumbnail *
create_appthumb (ClutterActor * apwin)
{
  GList *li;
  Thumbnail *apthumb, *nothumb;
  const HdLauncherApp *app;
  const HdCompMgrClient *hmgrc;
  const MBWMClientWindow *mbwmcwin;

  /* We're just in a MapNotify, it shouldn't happen. */
  mbwmcwin = actor_to_client_window (apwin, &hmgrc);
  g_assert (mbwmcwin != NULL);

  apthumb = g_new0 (Thumbnail, 1);
  apthumb->type = APPLICATION;

  /* @apwin related fields */
  apthumb->apwin   = g_object_ref (apwin);
  apthumb->inapwin = mbwmcwin->geometry;
  if ((app = hd_comp_mgr_client_get_launcher (HD_COMP_MGR_CLIENT (hmgrc))) != NULL)
    apthumb->video_fname = hd_launcher_app_get_switcher_icon (HD_LAUNCHER_APP (app));

  /* .nodest: try the property first then fall back to the WM_CLASS hint.
   * TODO This is temporary, just not to break the little functionality
   *      we already have. */
  apthumb->nodest = hd_util_get_x_window_string_property (
                                           mbwmcwin->wm, mbwmcwin->xwindow,
                                           HD_ATOM_NOTIFICATION_THREAD);
  if (!apthumb->nodest)
    {
      XClassHint xwinhint;

      if (XGetClassHint (mbwmcwin->wm->xdpy, mbwmcwin->xwindow, &xwinhint))
        {
          apthumb->nodest = xwinhint.res_class;
          XFree (xwinhint.res_name);
        }
      else
        g_warning ("XGetClassHint(%lx): failed", mbwmcwin->xwindow);
    }

  /* .titlebar */
  apthumb->titlebar = hd_title_bar_create_fake (NULL);

  /* .windows */
  apthumb->windows = clutter_group_new ();
  clutter_actor_set_name (apthumb->windows, "windows");

  /* .prison: anchor @apwin where it should be.
   * ie. let its (0, 0) coordinate be the top-left of @apwin. */
  apthumb->prison = clutter_group_new ();
  setup_prison (apthumb);
  clutter_container_add (CLUTTER_CONTAINER (apthumb->prison),
                         apthumb->titlebar, apthumb->windows, NULL);

  /* Do we have a notification for @apwin? */
  for_each_notification (li, nothumb)
    {
      if (tnote_matches_thumb (nothumb->tnote, apthumb))
        { /* Yes, steal it from the @Navigator_area. */
          apthumb->tnote = remove_nothumb (li, FALSE);
          break;
        }
    }

  /* Have .thwin created .*/
  create_thwin (apthumb);
  g_signal_connect_swapped (apthumb->thwin, "button-release-event",
                            G_CALLBACK (appthumb_clicked), apthumb);
  g_signal_connect_swapped (apthumb->close, "button-release-event",
                            G_CALLBACK (appthumb_close_clicked),
                            apthumb);


  return apthumb;
}

/* #ClutterEffectCompleteFunc for hd_task_navigator_remove_window()
 * called when the TV-turned-off effect of @apthumb finishes. */
static void
appthumb_turned_off_1 (ClutterActor * unused, Thumbnail * apthumb)
{ /* Byebye @apthumb! */
  release_win (apthumb);
  free_thumb (apthumb);
}

/* Likewise.  This is a separate function because it needs a different
 * user data pointer (@cmgrcc) and we don't feel like defining a new
 * struture only for this purpose. */
static void
appthumb_turned_off_2 (ClutterActor * unused,
                       MBWMCompMgrClutterClient * cmgrcc)
{
  /* Undo what we did to @cmgrcc in hd_task_navigator_remove_window(). */
  mb_wm_comp_mgr_clutter_client_unset_flags (cmgrcc,
                                 MBWMCompMgrClutterClientDontUpdate
                               | MBWMCompMgrClutterClientEffectRunning);
  mb_wm_object_unref (MB_WM_OBJECT (cmgrcc));
}

/* add_effect_closure() callback to to resume a delayed remove_window(). */
static void
remove_window_later (ClutterActor * win, EffectCompleteClosure * closure)
{
  hd_task_navigator_remove_window (HD_TASK_NAVIGATOR (Navigator), win,
                                   closure->fun, closure->funparam);
  g_slice_free (EffectCompleteClosure, closure);
}

/*
 * Asks the navigator to forget about @win.  If @fun is not %NULL it is
 * called when @win is actually removed from the screen; this may take
 * some time if there is an effect.  If not, it is called immedeately
 * with @funparam.  If there is zooming in progress the operation is
 * delayed until it's completed.
 */
void
hd_task_navigator_remove_window (HdTaskNavigator * self,
                                 ClutterActor * win,
                                 ClutterEffectCompleteFunc fun,
                                 gpointer funparam)
{ g_debug (__FUNCTION__);
  GList *li;
  Thumbnail *apthumb;
  ClutterActor *newborn;

  /* Postpone? */
  if (animation_in_progress (Zoom_effect_timeline))
    { g_debug ("delayed");
      EffectCompleteClosure *closure;

      closure = g_slice_new (EffectCompleteClosure);
      closure->fun = fun;
      closure->funparam = funparam;
      add_effect_closure (Zoom_effect_timeline,
                          (ClutterEffectCompleteFunc)remove_window_later,
                          win, closure);
      return;
    }

  /* Find @apthumb for @win.  We cannot use find_by_apiwin() because
   * we need @li to be able to remove @apthumb from @Thumbnails. */
  apthumb = NULL;
  for_each_appthumb (li, apthumb)
    if (apthumb->apwin == win)
      break;
  if (!apthumb)
    { /* Code bloat is your enemy, right? */
      g_critical("%s: window actor %p not found", __FUNCTION__, win);
      return;
    }

  /* If @win had a notification, add it to @Navigator_area
   * as a standalone thumbnail. */
  if (thumb_has_notification (apthumb))
    {
      newborn = add_nothumb (apthumb->tnote)->thwin;
      apthumb->tnote = NULL;
    }
  else
    newborn = NULL;

  /* If we're active let's do the TV-turned-off effect on @apthumb.
   * This effect is run in parallel with the flying effects. */
  if (hd_task_navigator_is_active ())
    {
      MBWMCompMgrClutterClient *cmgrcc;

      /* Hold a reference on @win's clutter client. */
      if (!(cmgrcc = g_object_get_data (G_OBJECT (apthumb->apwin),
                                        "HD-MBWMCompMgrClutterClient")))
        { /* No point in trying to animate, the actor is destroyed. */
          g_critical ("cmgrcc is already unset for %p", apthumb->apwin);
          goto damage_control;
        }

      mb_wm_object_ref (MB_WM_OBJECT (cmgrcc));
      mb_wm_comp_mgr_clutter_client_set_flags (cmgrcc,
                                 MBWMCompMgrClutterClientDontUpdate
                               | MBWMCompMgrClutterClientEffectRunning);

      /* At the end of effect free @apthumb and release @cmgrcc. */
      clutter_actor_raise_top (apthumb->thwin);
      turnoff_effect (Fly_effect_timeline, apthumb->thwin);
      add_effect_closure (Fly_effect_timeline,
                          (ClutterEffectCompleteFunc)appthumb_turned_off_1,
                          apthumb->thwin, apthumb);
      add_effect_closure (Fly_effect_timeline,
                          (ClutterEffectCompleteFunc)appthumb_turned_off_2,
                          apthumb->thwin, cmgrcc);
    }
  else
    {
damage_control:
      free_thumb (apthumb);
    }

  /* Do all (TV, windows flying, add notification thumbnail) effects at once. */
  Thumbnails = g_list_delete_link (Thumbnails, li);
  NThumbnails--;
  layout (newborn);

  /* Arrange for calling @fun(@funparam) if/when appropriate. */
  if (animation_in_progress (Fly_effect_timeline))
    add_effect_closure (Fly_effect_timeline,
                        fun, CLUTTER_ACTOR (self), funparam);
  else if (fun)
    fun (CLUTTER_ACTOR (self), funparam);

  /* Sync the Tasks button. */
  hd_render_manager_update();
}

/*
 * Tells the swicher to show @win in a thumbnail when active.  If the
 * navigator is active now it starts managing @win.  When @win is managed
 * by the navigator it is not changed in any means other than reparenting.
 * It is an error to add @win multiple times.
 */
void
hd_task_navigator_add_window (HdTaskNavigator * self,
                              ClutterActor * win)
{ g_debug (__FUNCTION__);
  Thumbnail *apthumb;

  g_return_if_fail (!hd_task_navigator_has_window (self, win));
  apthumb = create_appthumb (win);
  if (hd_task_navigator_is_active ())
    claim_win (apthumb);

  /* Add the @apthumb at the end of application thumbnails. */
  Thumbnails = Notifications
    ? g_list_insert_before (Thumbnails, Notifications, apthumb)
    : g_list_append (Thumbnails, apthumb);
  NThumbnails++;

  layout (apthumb->thwin);

  /* Do NOT sync the Tasks button because we may get it wrong and it's
   * done somewhere in the mist anyway. */
}

/* Remove @dialog from its application's thumbnail
 * and don't show it anymore. */
void
hd_task_navigator_remove_dialog (HdTaskNavigator * self,
                                 ClutterActor * dialog)
{ g_debug (__FUNCTION__);
  guint i;
  Thumbnail *apthumb;

  if (!(apthumb = find_dialog (&i, dialog, TRUE)))
    return;

  g_object_unref (dialog);
  g_ptr_array_remove_index (apthumb->dialogs, i);

  if (hd_task_navigator_is_active ())
    hd_render_manager_return_dialog (dialog);
}

/*
 * Register a @dialog to show on the top of an application's thumbnail.
 * @dialog needn't be a dialog, actually, it can be just about anything
 * you want to show along with the application, like menus.
 * @parent should be the actor of the window @dialog is transient for.
 * The @dialog is expected to have been positioned already.  It is an
 * error to add the same @dialog more than once.
 */
void
hd_task_navigator_add_dialog (HdTaskNavigator * self,
                              ClutterActor * parent,
                              ClutterActor * dialog)
{ g_debug (__FUNCTION__);
  guint i;
  Thumbnail *apthumb;

  /* Already have @dialog?  If not, find its place. */
  if (find_dialog (NULL, dialog, FALSE))
    {
      g_critical ("%s: dialog actor %s added twice",
                  __FUNCTION__, clutter_actor_get_name (dialog));
      return;
    }
  if (!(apthumb = find_dialog (&i, parent, FALSE)))
      return;

  /* Claim @dialog now if we're active. */
  if (hd_task_navigator_is_active ())
    clutter_actor_reparent (dialog, apthumb->windows);

  /* Add @dialog to @apthumb->dialogs. */
  if (!apthumb->dialogs)
    apthumb->dialogs = g_ptr_array_new ();
  g_ptr_array_add (apthumb->dialogs, g_object_ref(dialog));
}
/* Add/remove windows }}} */
/* }}} */

/* Notification thumbnails {{{ */
/* %TNote:s {{{ */
/* HdNote::HdNoteSignalChanged signal handler. */
static Bool
tnote_changed (HdNote * hdnote, int unused1, TNote * tnote)
{ g_debug(__FUNCTION__);
  GList *li;
  Thumbnail *thumb;

  thumb = NULL;
  for_each_thumbnail (li, thumb)
    if (thumb->tnote == tnote)
      break;

  if (thumb_is_notification (thumb))
    {
      const char *iname, *oname;

      set_label_text_and_color (thumb->time,
                                hd_note_get_time (tnote->hdnote),
                                NULL);
      set_label_text_and_color (thumb->message,
                                hd_note_get_message (tnote->hdnote),
                                NULL);

      if ((iname = hd_note_get_icon (tnote->hdnote)) != NULL)
        { /* Replace icon? */
          if (!(oname = clutter_actor_get_name (thumb->icon))
              || strcmp (iname, oname))
            { /* Ignore further errors. */
              clutter_container_remove_actor (CLUTTER_CONTAINER (thumb->prison),
                                              thumb->icon);
              thumb->icon = get_icon (iname, Thumbsize == &Thumbsizes.large
                                      ? ICON_FINGER : ICON_STYLUS);
              clutter_container_add_actor (CLUTTER_CONTAINER (thumb->prison),
                                           thumb->icon);
            }
        }
    }
  reset_thumb_title (thumb);

  /* TODO The change in a group event notification may actually
   *      signify that an event was removed from the group,
   *      in which case we should not pulse. */
  hd_title_bar_set_switcher_pulse (
                    HD_TITLE_BAR (hd_render_manager_get_title_bar ()),
                    TRUE);

  return False;
}

/* Returns a %TNote prepared for @hdnote. */
static TNote *
create_tnote (HdNote * hdnote)
{
  TNote * tnote;

  tnote = g_new0 (TNote, 1);
  tnote->hdnote = mb_wm_object_ref (MB_WM_OBJECT (hdnote));

  /* Be notified when @hdnote's icon/summary changes. */
  tnote->hdnote_changed_cb_id = mb_wm_object_signal_connect (
                        MB_WM_OBJECT (hdnote), HdNoteSignalChanged,
                        (MBWMObjectCallbackFunc)tnote_changed, tnote);

  return tnote;
}

/* Releases what was allocated by create_tnote(). */
static void
free_tnote (TNote * tnote)
{
  mb_wm_object_signal_disconnect (MB_WM_OBJECT (tnote->hdnote),
                                  tnote->hdnote_changed_cb_id);
  mb_wm_object_unref (MB_WM_OBJECT (tnote->hdnote));
  g_free (tnote);
}

/* Returns whether the notification represented by @tnote belongs to
 * the application represented by @thumb. */
static gboolean
tnote_matches_thumb (const TNote * tnote, const Thumbnail * thumb)
{
  char const * dest = hd_note_get_destination (tnote->hdnote);
  return dest && thumb->nodest && !strcmp (dest, thumb->nodest);
}
/* %TNote:s }}} */

/* nothumb:s {{{ */
/* Called when a notification %Thumbnail is clicked. */
static gboolean
nothumb_clicked (Thumbnail * nothumb)
{
  g_signal_emit_by_name (Navigator, "notification-clicked",
                         nothumb->tnote->hdnote);
  return TRUE;
}

/* Called thwn a notification %Thumbnail's close buttin is clicked. */
static gboolean
nothumb_close_clicked (Thumbnail * nothumb)
{
  g_assert (thumb_has_notification (nothumb));
  g_signal_emit_by_name (Navigator, "notification-closed",
                         nothumb->tnote->hdnote);
  return TRUE;
}

/* Returns a %Thumbnail for @tnote and adds it to @Thumbnails. */
static Thumbnail *
add_nothumb (TNote * tnote)
{
  Thumbnail *nothumb;

  /* Create the %Thumbnail. */
  nothumb = g_new0 (Thumbnail, 1);
  nothumb->type = NOTIFICATION;
  nothumb->tnote = tnote;

  /* .time */
  nothumb->time = set_label_text_and_color (clutter_label_new (),
                                       hd_note_get_time (tnote->hdnote),
                                       &DefaultTextColor);

  /* .message */
  nothumb->message = set_label_text_and_color (clutter_label_new (),
                                    hd_note_get_message (tnote->hdnote),
                                    &DefaultTextColor);
  clutter_label_set_font_name (CLUTTER_LABEL (nothumb->message),
                               SmallSystemFont);
  clutter_label_set_line_wrap (CLUTTER_LABEL (nothumb->message), TRUE);
  g_signal_connect (nothumb->message, "notify::allocation",
                    G_CALLBACK (clip_on_resize), NULL);

  /* .icon will be set by layout_thumbs(). */
  nothumb->prison = clutter_group_new();
  clutter_container_add (CLUTTER_CONTAINER (nothumb->prison),
                         nothumb->time, nothumb->message, NULL);

  create_thwin (nothumb);
  g_signal_connect_swapped (nothumb->thwin, "button-release-event",
                            G_CALLBACK (nothumb_clicked), nothumb);
  g_signal_connect_swapped (nothumb->close, "button-release-event",
                            G_CALLBACK (nothumb_close_clicked), nothumb);

  /* Add @nothumb at the end of @Notifications. */
  if (!Notifications)
    {
      Notifications = g_list_append (Notifications, nothumb);
      Thumbnails = g_list_concat (Thumbnails, Notifications);
    }
  else
    Notifications = g_list_append (Notifications, nothumb);
  NThumbnails++;

  return nothumb;
}

/* If @li is in @Notifications removes the notification's thumbnail
 * from @Navigator_area and deletes @li from the list, but it doesn't
 * free %TNote. */
static TNote *
remove_nothumb (GList * li, gboolean destroy_tnote)
{
  TNote *tnote;
  Thumbnail *nothumb;

  nothumb = li->data;
  g_assert (thumb_is_notification (nothumb));

  if (destroy_tnote)
    {
      free_tnote (nothumb->tnote);
      tnote = NULL;
    }
  else
    tnote = nothumb->tnote;
  nothumb->tnote = NULL;

  free_thumb (nothumb);
  if (li == Notifications)
    Notifications = li->next;
  Thumbnails = g_list_delete_link (Thumbnails, li);
  NThumbnails--;

  return tnote;
}
/* nothumb:s }}} */

/* Add/remove notifications {{{ */
/* Show a notification in the navigator, either in the @Navigator_area
 * or in the notification's thumbnail title area if it's running. */
void
hd_task_navigator_add_notification (HdTaskNavigator * self,
                                    HdNote * hdnote)
{ g_debug (__FUNCTION__);
  GList *li;
  TNote *tnote;
  Thumbnail *apthumb;

  /* Ringring the notification in any case. */
  g_return_if_fail (hdnote != NULL);
  hd_title_bar_set_switcher_pulse (
               HD_TITLE_BAR (hd_render_manager_get_title_bar ()), TRUE);

  /* Is @hdnote's destination application already open? */
  tnote = create_tnote (hdnote);
  for_each_appthumb (li, apthumb)
    {
      if (tnote_matches_thumb (tnote, apthumb))
        {
          if (thumb_has_notification (apthumb))
            {
              /* hildon-home should have replaced the summary of the existing
               * %HdNote instead; the easy way out is adding the new one to the
               * @Navigator_area. */
              g_critical ("%s: attempt to add more than one notification",
                          __FUNCTION__);
              break;
            }

          /* Okay, found it. */
          apthumb->tnote = tnote;
          reset_thumb_title (apthumb);
          recreate_thumb_frame (apthumb);
          return;
        }
    }

  layout (add_nothumb (tnote)->thwin);

  /* Make sure the Tasks button points to the switcher now. */
  hd_render_manager_update();
}

/* Remove a notification from the navigator, either if
 * it's shown on its own or in a thumbnail title area. */
void
hd_task_navigator_remove_notification (HdTaskNavigator * self,
                                       HdNote * hdnote)
{ g_debug (__FUNCTION__);
  GList *li;
  Thumbnail *thumb;

  g_return_if_fail (hdnote != NULL);

  /* Find @thumb for @hdnote. */
  thumb = NULL;
  for_each_thumbnail (li, thumb)
    if (thumb->tnote && thumb->tnote->hdnote == hdnote)
      break;
  if (!thumb || !thumb->tnote)
    /*
     * This used to be a grif().  Then that got disabled because it's evil.
     * Now the check is resurrected because of Coverity.  Except that now
     * we don't get a warning if this BOGUS condition occurrs.  Should i
     * readd the warning?  I cannot be bothered.
     */
    return;

  if (thumb_is_notification (thumb))
    { /* @hdinfo is displayed in a thumbnail on its own. */
      remove_nothumb (li, TRUE);
      layout (NULL);

      /* Sync the Tasks button, we might have just become empty. */
      hd_render_manager_update ();
    }
  else
    { /* @hdnote is in an application's title area. */
      free_tnote (thumb->tnote);
      thumb->tnote = NULL;
      reset_thumb_title (thumb);
      recreate_thumb_frame (thumb);
    }

  /* Reset the highlighting if no more notifications left. */
  if (!hd_task_navigator_has_notifications ())
    hd_title_bar_set_switcher_pulse (
                      HD_TITLE_BAR (hd_render_manager_get_title_bar ()),
                      FALSE);
}
/* Add/remove notifications }}} */
/* }}} */

/* %HdTaskNavigator {{{ */
/* Find which thumbnail represents the requested app. */
/* Callbacks {{{ */
G_DEFINE_TYPE (HdTaskNavigator, hd_task_navigator, CLUTTER_TYPE_GROUP);

/* @Navigator's "show" handler. */
static void
navigator_shown (ClutterActor * navigator, gpointer unused)
{ g_debug(__FUNCTION__);
  GList *li;
  Thumbnail *thumb;

  /* Reset the position of @Scroller, which may have been changed when
   * we zoomed in last time.  If the caller wants to zoom_out() it will
   * set them up properly. */
  clutter_actor_set_scale (Scroller, 1, 1);
  clutter_actor_set_position (Scroller, 0, 0);

  /* Take all application windows we know about into our care
   * because we are responsible for showing them now. */
  for_each_appthumb (li, thumb)
    claim_win (thumb);
}

/* @Navigator's "hide" handler. */
static void
navigator_hidden (ClutterActor * navigator, gpointer unused)
{ g_debug(__FUNCTION__);
  GList *li;
  Thumbnail *thumb;

  /* Finish in-progress animations, allowing for the final placement of
   * the involved actors. */
  if (animation_in_progress (Zoom_effect_timeline))
    {
      /* %HdSwitcher must make sure it doesn't do silly things if the
       * user cancelled the zooming. */
      stop_animation (Zoom_effect_timeline);
      g_assert (!animation_in_progress (Zoom_effect_timeline));
    }
  if (animation_in_progress (Fly_effect_timeline))
    {
      stop_animation (Fly_effect_timeline);
      g_assert (!animation_in_progress (Fly_effect_timeline));
    }

  /* Undo navigator_shown(). */
  for_each_appthumb (li, thumb)
    release_win (thumb);
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

/* Called when the screen's dimensions have changed. */
static void
screen_size_changed (void)
{
  GList *li;
  Thumbnail *apthumb;

  /*
   * Screen size has an influence on all @Thumbnails.inapwin.
   * This is the only time we care about changing @inapwin.
   * Since @apwin:s are generally not resized it's only significant
   * when the WM is started right into portrait mode and then it's
   * switched to landscape.
   */
  clutter_actor_set_size(Scroller,
                         HD_COMP_MGR_SCREEN_WIDTH, HD_COMP_MGR_SCREEN_HEIGHT);
  for_each_appthumb (li, apthumb)
    {
      const MBWMClientWindow * mbwmcwin;

      if (!(mbwmcwin = actor_to_client_window (apthumb->apwin, NULL)))
        continue;
      apthumb->inapwin = mbwmcwin->geometry;
      setup_prison (apthumb);
    }

  /* Relayout the inners of appthumbs. */
  Thumbsize = NULL;
  layout_thumbs (NULL);
}
/* Callbacks }}} */

/* Creates an effect template for @duration and also returns its timeline.
 * The timeline is usually needed to hook onto its "completed" signal. */
static ClutterEffectTemplate *
new_animation (ClutterTimeline ** timelinep, guint duration)
{
  ClutterEffectTemplate *effect;

  /*
   * It's necessary to do it this way because there's no way to retrieve
   * an effect's timeline after it's created.  Ask the template not to make
   * copies of the timeline but always use the very same one we provide,
   * otherwise it would have to be clutter_timeline_start()ed for every
   * animation to get its signals.
   */
  *timelinep = clutter_timeline_new_for_duration (duration);
  effect = clutter_effect_template_new (*timelinep,
                              CLUTTER_ALPHA_SMOOTHSTEP_INC);
  clutter_effect_template_set_timeline_clone (effect, FALSE);

  return effect;
}

/* The object we create is initially hidden. */
static void
hd_task_navigator_init (HdTaskNavigator * self)
{
  Navigator = CLUTTER_ACTOR (self);
  clutter_actor_set_reactive (Navigator, TRUE);
  g_signal_connect (Navigator, "show", G_CALLBACK (navigator_shown),  NULL);
  g_signal_connect (Navigator, "hide", G_CALLBACK (navigator_hidden), NULL);

  /* Actor hierarchy */
  /* Turn off visibility detection for @Scroller to it won't be clipped by it. */
  Scroller = tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);
  clutter_actor_set_name (Scroller, "Scroller");
  clutter_actor_set_size (Scroller, SCREEN_WIDTH, SCREEN_HEIGHT);
  clutter_actor_set_visibility_detect(Scroller, FALSE);
  clutter_container_add_actor (CLUTTER_CONTAINER (Navigator), Scroller);

  /*
   * When we zoom in we may need to move the @Scroller up or downwards.
   * If we leave clipping on that becomes visible then, by cutting one
   * half of the zooming window.  Circumvent it by removing clipping
   * at the same time it is set.  TODO This can be considered a hack.
   */
  Navigator_area = HD_SCROLLABLE_GROUP (hd_scrollable_group_new ());
  clutter_actor_set_name (CLUTTER_ACTOR (Navigator_area), "Navigator area");
  clutter_actor_set_reactive (CLUTTER_ACTOR (Navigator_area), TRUE);
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR (Navigator_area), FALSE);
  g_signal_connect (Navigator_area, "notify::has-clip",
                    G_CALLBACK (unclip), NULL);
  g_signal_connect_swapped (Navigator_area, "button-release-event",
                            G_CALLBACK (navigator_clicked), Navigator);
  clutter_container_add_actor (CLUTTER_CONTAINER (Scroller),
                               CLUTTER_ACTOR (Navigator_area));

  /* Make sure we're not broken if the WM is started in portrait mode. */
  g_signal_connect(clutter_stage_get_default(),
                   "notify::allocation", G_CALLBACK(screen_size_changed),
                   NULL);

  /* Effect timelines */
  Fly_effect  = new_animation (&Fly_effect_timeline,  FLY_EFFECT_DURATION);
  Zoom_effect = new_animation (&Zoom_effect_timeline, ZOOM_EFFECT_DURATION);

  /* Master pieces */
  SystemFont = resolve_logical_font ("SystemFont");
  SmallSystemFont = resolve_logical_font ("SmallSystemFont");
  resolve_clutter_color (&DefaultTextColor,  "DefaultTextColor");
  resolve_clutter_color (&ReversedTextColor, "ReversedTextColor");

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

  /* When the zoom in transition has finished */
  g_signal_new ("zoom-in-complete", G_TYPE_FROM_CLASS (klass),
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
