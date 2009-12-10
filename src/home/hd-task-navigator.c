/*
 * {{{
 * Implements a plate shown in task switcher view, which displays
 * the thumbnails of the running applications and incoming events.
 * Note that throughout the code "notification" refers to incoming
 * event notifications.
 *
 * Actor hierarchy:
 * @Navigator                   #ClutterGroup
 *   @Scroller                  #TidyFingerScroll
 *     @Grid                    #HdScrollableGroup
 *       @Thumbnails            #ClutterGroup:s
 *
 * Thumbnail.thwin hierarchy:
 *   .prison                    #ClutterGroup         applications
 *     .titlebar                #ClutterGroup
 *     .windows                 #ClutterGroup
 *       .apwin                 #ClutterActor
 *       .dialogs               #ClutterActor
 *     .video                   #ClutterTexture
 *   .notwin                    #ClutterGroup         notifications
 *     .background              #ClutterCloneTexture  or apps w/notifs
 *     .separator               #ClutterCloneTexture
 *     .icon                    #ClutterTexture
 *     .count, .time, .message  #ClutterLabel
 *   .plate                     #ClutterGroup
 *     .frame.all               #ClutterGroup         applications
 *       .frame.nw, .nm, .ne    #ClutterCloneTexture  applications
 *       .frame.mw,      .mw    #ClutterCloneTexture  applications
 *       .frame.sw, .sm, .sw    #ClutterCloneTexture  applications
 *     .title                   #ClutterLabel
 *     .close                   #ClutterGroup
 *       .icon_app, .icon_notif #ClutterCloneTexture
 *
 * FIXME 'apthumb' is sometimes 'appthumb'
 * }}}
 */

/* Include files {{{ */
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

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
#include "hd-gtk-style.h"
/* }}} */

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
#define THUMB_LARGE_WIDTH         (int)(HD_COMP_MGR_LANDSCAPE_WIDTH/2.32)
#define THUMB_LARGE_HEIGHT        (int)(THUMB_LARGE_WIDTH/1.6)
#define THUMB_MEDIUM_WIDTH        (int)(HD_COMP_MGR_LANDSCAPE_WIDTH/3.2)
#define THUMB_MEDIUM_HEIGHT       (int)(THUMB_MEDIUM_WIDTH/1.6)
#define THUMB_SMALL_WIDTH         (int)(HD_COMP_MGR_LANDSCAPE_WIDTH/5.5)
#define THUMB_SMALL_HEIGHT        (int)(THUMB_SMALL_WIDTH/1.6)

/* Metrics inside a thumbnail. */
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
#define TITLE_HEIGHT              32

/* These are NOT the dimensions of the frame graphics but marings. */
#define FRAME_TOP_HEIGHT          TITLE_HEIGHT
#define FRAME_WIDTH                2
#define FRAME_BOTTOM_HEIGHT        2
#define PRISON_XPOS               FRAME_WIDTH
#define PRISON_YPOS               FRAME_TOP_HEIGHT

/* Read: reduce by 4 pixels; only relevant if the time(label) wraps. */
#define NOTE_TIME_LINESPACING     pango_units_from_double(-4)
#define NOTE_MARGINS              MARGIN_DEFAULT
#define NOTE_BOTTOM_MARGIN        MARGIN_HALF
#define NOTE_ICON_GAP             MARGIN_HALF
#define NOTE_SEPARATOR_PADDING    0

/*
 * %ZOOM_EFFECT_DURATION:         Determines how many miliseconds should
 *                                it take to zoom thumbnails.  Tunable.
 *                                Increase for the better observation of
 *                                effects or decrase for faster feedback.
 * %FLY_EFFECT_DURATION:          Same for the flying animation, ie. when
 *                                the windows are repositioned.
 * %NOTIFADE_IN_DURATION,
 * %NOTIFADE_OUT_DURATION:        Milisecs to fade in and out notifications.
 *                                These effects are executed in an independent
 *                                timeline, except when zooming in/out, when
 *                                they are coupled with @Zoom_effect_timeline.
 */
#if 1
# define ZOOM_EFFECT_DURATION     \
  hd_transition_get_int("task_nav", "zoom_duration", 250)
# define FLY_EFFECT_DURATION      \
  hd_transition_get_int("task_nav", "fly_duration",  250)
#else
# define ZOOM_EFFECT_DURATION     1000
# define FLY_EFFECT_DURATION      1000
#endif

#define NOTIFADE_IN_DURATION      \
  hd_transition_get_int("task_nav", "notifade_in", 250)
#define NOTIFADE_OUT_DURATION     \
  hd_transition_get_int("task_nav", "notifade_out", 250)

/*
 *  These are based on the UX Guidance.
 *
 * %MIN_CLICK_TIME:   Clicks shorter than this...
 * %MAX_CLICK_TIME:   or longer than this microseconds are not clicks.
 *                    In practice this is 30-300 ms.
 */
#define MIN_CLICK_TIME           30000
#define MAX_CLICK_TIME          300000
/* Standard definitions }}} */

/* Macros {{{ */
#define for_each_thumbnail(li, thumb)                                   \
  for ((li) = Thumbnails;                                               \
       (li) ? ((thumb) = li->data) : ((thumb) = NULL);                  \
       (li) = (li)->next)
#define for_each_appthumb(li, thumb)                                    \
  for ((li) = Thumbnails;                                               \
       (li) != Notifications ? ((thumb) = li->data) : ((thumb) = NULL); \
       (li) = (li)->next)
#define for_each_notification(li, thumb)                                \
  for ((li) = Notifications;                                            \
       (li) ? ((thumb) = li->data) : ((thumb) = NULL);                  \
       (li) = (li)->next)

#define thumb_is_application(thumb)   ((thumb)->type == APPLICATION)
#define thumb_is_notification(thumb)  ((thumb)->type == NOTIFICATION)
#define thumb_has_notification(thumb) ((thumb)->tnote != NULL)
#define apthumb_has_dialogs(apthumb)  \
  ((apthumb)->dialogs && (apthumb)->dialogs->len > 0)

#define THUMBSIZE_IS(what)            (Thumbsize == &Thumbsizes.what)
#define FLY(ops, how)                 ((ops) == &Fly_##how)
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
   *                      (not in the last and incomplete row).
   * -- @xpos:            The horizontal position of the leftmost
   *                      thumbnails relative to the @Grid
   *                      (not in the last and incomplete row).
   * -- @last_row_xpos:   Likewise, but for the last and incomplete row.
   *                      By definition the last and incomplete row has
   *                      fewer thunmbnails than @cells_per_row.
   * -- @ypos:            The vertical position of the topmost thumbnails
   *                      relative to the @Grid.
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
/* Incoming event notification clients and thumbnails. {{{ */
typedef struct
{
  /*
   * -- @hdnote:               The notification client,
   *                           mb_wm_object_ref()ed.
   * -- @hdnote_changed_cb_id: %HdNoteSignalChanged callback id.
   */
  HdNote                      *hdnote;
  unsigned long                hdnote_changed_cb_id;

  /*
   * -- @notwin:              Wraps for all the rest, nothing more.
   *                          In purpose similar to %Thumbnail::prison.
   *                          These actors always need to be present
   *                          whenever there's a notification in order
   *                          to show it as a whole.
   */
  ClutterActor                *notwin;

  /*
   * Content:
   * -- @icon:                %NULL until .notwin is layed out first
   * -- @count:               Displays the message count (how many
   *                          notification does this thumbnail represent).
   * -- @time:                Displays the time elapsed since the last
   *                          notification this thumbnail stands for
   *                          arrived.  Anchored at NORTH.  May be wrapped.
   * -- @message:             Referred to as "secondary text" in UI Specs.
   *                          May be wrapped.
   */
  ClutterActor                *icon, *count, *time, *message;

  /*
   * Decoration:
   * -- @background:          Scaled prison-size.
   * -- @separator:           Placed right below the title area, centered
   *                          with a little padding and scaled horizontally.
   */
  ClutterActor                *background, *separator;
} TNote; /* }}} */

/* Our central object, the thumbnail. {{{ */
typedef struct
{
  /* What are we?  An %APPLICATION thumbnail can also show a notification
   * so a thumbnail essentially has three states: thumb_is_notification()
   * and thumb_has_notification(). */
  enum { APPLICATION, NOTIFICATION } type;

  /*
   * -- @thwin:       The @Grid's thumbnail window and event responder.
   *                  Can be faded in/out if the thumbnail is a notification.
   * -- @plate:       Groups the @title and the @frame graphics; used to fade
   *                  them all at once.  It sits on the top and can be thought
   *                  of as a boilerplate.
   * -- @title:       What to put in the thumbnail's title area.
   *                  Centered vertically within TITLE_HEIGHT.
   * -- @close:       An invisible actor reacting to user taps to close
   *                  the thumbnail.  Slightly reaches out of the thumbnail
   *                  bounds.  Also contains the icons.
   * -- @close_app_icon, @close_notif_icon: these.  They're included in
   *                  the generic structure in order to keep their position
   *                  together.  When there are neither adopt_notification()
   *                  nor orphane_notification() transitions going on at most
   *                  one of them is shown at a time according to one of the
   *                  three states of the thumbail.  Otherwise they are faded
   *                  in and out.
   *
   *                  When the thumbnail is a %NOTIFICATION both icons are
   *                  normally opaque.  Otherwise if thumb_has_notification()
   *                  @close_app_icon is normally transparent and hidden,
   *                  and the other is opaque.  Otherwise the opposit holds.
   */
  ClutterActor        *thwin, *plate;
  ClutterActor        *title, *close;
  ClutterActor        *close_app_icon, *close_notif_icon;

  /* TODO This should go to a dynamically allocated structure like .tnote. */
  union
  {
    /* Application-thumbnail-specific fields */
    struct
    {
      /*
       * -- @apwin:       The pristine #MBWMCompMgrClutterClient::actor,
       *                  not to be touched.
       * -- @dialogs:     The application's dialogs, popup menus and whatsnot
       *                  if it has or had any earlier, otherwise %NULL.
       *                  They are shown along with .apwin.  Hidden if
       *                  we have a .video.
       * -- @cemetery:    This is the resting place for .apwin:s that have
       *                  been hd_task_navigator_replace_window()ed.
       *                  When we're active they are hidden.  Otherwise
       *                  we don't care.  We don't keep a string reference
       *                  on them, but a weak reference callback removes
       *                  them from this array when the actor is destroyed.
       *                  The primary use of the cemetery is to grab all the
       *                  subviews of an application when we are activated,
       *                  so hd_render_set_visibilities() doesn't need to
       *                  worry.
       * -- @windows:     Just 0-dimension container for @apwin and @dialogs,
       *                  its sole purpose is to make it easier to hide them
       *                  when the %Thumbnail has a @video.  Also clips its
       *                  contents to @App_window_geometry, making sure that
       *                  really nothing is shown outside the thumbnail.
       * -- @titlebar:    An actor that looks like the original title bar.
       *                  Faded in/out when zooming in/out, but normally
       *                  transparent or not visible at all.
       * -- @prison:      Scales, positions and anchors all the above actors
       *                  and more.  It's surrounded closely by @frame and it
       *                  represents the application area of application view.
       *                  That is, whatever is shown in @prison is what you
       *                  see in that area.  Hidden if the thumbnails has a
       *                  notification.
       */
      ClutterActor        *apwin, *windows, *titlebar, *prison;
      GPtrArray           *dialogs, *cemetery;

      /* Frame decoration.  The graphics are updated automatically whenever
       * the theme changes.  Pieces in the middle are scaled horizontally
       * xor vertically. */
      struct
      {
        /* The container of all @pieces.  If the thumbnail is an APPLICATION
         * but it has a notification it's normally transparent and hidden,
         * otherwise it's normally opaque. */
        ClutterActor *all;

        union
        {
          struct
          { /* north: west, middle, east; middle; south */
            ClutterActor *nw, *nm, *ne;
            ClutterActor *mw,      *me;
            ClutterActor *sw, *sm, *se;
          };

          ClutterActor *pieces[8];
        };
      } frame;

      /*
       * -- @win_changed_cb_id: Tracks the window's name to keep the title
       *                        of the thumbnail up to date.  It's only
       *                        valid as long as @win is.
       * -- @win:               The window of the client @apwin belongs to.
       *                        Kept around in order to be able to disconnect
       *                        @win_changed_cb_id.  Replaced when @apwin is
       *                        replaced.  It's not ref'd specifically.
       *                        %NULL if the client is in hibernation.
       * -- @saved_title:       What the application window's title was when
       *                        it left for hibernation.  It's only use is
       *                        to know what to reset the thumb title to if
       *                        the client's notification was removed when
       *                        it was still in hibernation.  Cleared when
       *                        the window actor is replaced, presumably
       *                        because it's woken up.
       * -- @title_had_markup:  Mirrors @win::name_has_markup similarly to
       *                        @saved_title.
       */
      MBWMClientWindow    *win;
      unsigned long        win_changed_cb_id;
      gchar               *saved_title;
      gboolean             title_had_markup;

      /*
       * -- @nodest:      What notifications this thumbnails is destination for.
       *                  Taken from the _HILDON_NOTIFICATION_THREAD property
       *                  of the thumbnail's client or its WM_CLASS hint.
       *                  Once checked when the window is added then whenever
       *                  hd_task_navigator_notification_thread() is called.
       *                  TODO How about replace_window()?
       *                  Normally two or more application thumbnails should
       *                  not have the same @nodest.  It is undefined how to
       *                  handale this case but we'll try our best.  Used in
       *                  matching the appropriate TNote for this application.
       */
      gchar               *nodest;

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

    /* Currently we don't have notification-specific fields. */
  };

  /*
   * -- @tnote:       Notifications always have a %TNote in one of the
   *                  @Thumbnails: either in a %Thumbnail of their own,
   *                  or in the application's they belong to.
   */
  TNote               *tnote;
} Thumbnail; /* }}} */
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

/* For linear_effect(), resize_effect() and turnoff_effect(). */
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
    /* For linear_effect()s. */
    struct
    {
      /*
       * In each frame a number of properties of @actor is changed lineraly,
       * such as width and height.  Each property has its own structure.
       * The number of properties that can be controlled by such an effect
       * is limited by the the number of elements in the array.
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

      /* Auxiliary instruction for linear_effect()s. */
      union
      {
        /* For fade(). */
        struct
        {
          /* What to do when the effect completes. */
          enum final_fade_action_t
          {
            FINALLY_REST,   /* Nothing is necessary. */
            FINALLY_HIDE,   /* Hide @another_actor. */
            FINALLY_REMOVE, /* Remove EffectClosure::actor
                             * from @another_actor. */
          } finally;

          ClutterActor *another_actor;
        };
      };
    };

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
static struct { GtkRequisition small, medium, large; } Thumbsizes =
{
  .large  = { 0,  0  },
  .medium = { 0,  0  },
  .small  = { 0,  0  },
};

#define _setThumbSizes() \
if (G_LIKELY (Thumbsizes.large.width == 0))\
  {\
    Thumbsizes.large.width   = THUMB_LARGE_WIDTH;\
    Thumbsizes.large.height  = THUMB_LARGE_HEIGHT;\
    Thumbsizes.medium.width  = THUMB_MEDIUM_WIDTH;\
    Thumbsizes.medium.height = THUMB_MEDIUM_HEIGHT;\
    Thumbsizes.small.width   = THUMB_SMALL_WIDTH;\
    Thumbsizes.small.height  = THUMB_SMALL_HEIGHT;\
  }

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

/* The size and position of non-fullscreen application windows
 * in application view. */
static ClutterGeometry App_window_geometry =
{
  .x      = 0,
  .y      = HD_COMP_MGR_TOP_MARGIN,
  .width  = 0,
  .height = 0 - HD_COMP_MGR_TOP_MARGIN,
};
#define _setWindowGeometry()\
{\
  if (App_window_geometry.width == 0)\
    {\
      App_window_geometry.width = HD_COMP_MGR_LANDSCAPE_WIDTH;\
      App_window_geometry.height += HD_COMP_MGR_LANDSCAPE_HEIGHT;\
    }\
}

/* }}} */

/* Private variables {{{ */
/*
 * -- @Navigator:         Root group, event responder.
 * -- @Scroller:          Viewport of @Navigation_area and controls
 *                        its scrolling.  Moved and scaled when zooming.
 * -- @Grid:              Contains the complete layout and does a
 *                        little button-event management.
 */
static HdScrollableGroup *Grid;
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
/* Do we have notifications since we were last in task navigator? */
static gboolean UnseenNotifications = FALSE;

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
static const gchar *LargeSystemFont, *SystemFont, *SmallSystemFont;
static ClutterColor DefaultTextColor;
static ClutterColor NotificationTextColor, NotificationSecondaryTextColor;
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

  closure = g_slice_new0 (EffectClosure);
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
       *
       * As @timeline may not be the already running one ignore it.
       */
      gfloat now = clutter_timeline_get_progress (closure->timeline);
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
    {
      closure = new_effect (timeline, actor, NULL, resize_effect_complete);
      closure->effectid = resize_effect_complete;
    }
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
  else if ((closure = has_effect (actor, resize_effect_complete)) != NULL)
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

/* Fading effect {{{ */
/* frame_fun of fade() */
static void
fade_frame (ClutterTimeline * timeline, gint frame, EffectClosure * closure)
{
  clutter_actor_set_opacity (closure->actor, linear_effect_value (closure, 0,
    clutter_timeline_get_progress (timeline)));
}

/* complete_fun of fade() */
static void
fade_complete (ClutterTimeline * timeline, EffectClosure * closure)
{
  if (closure->finally == FINALLY_HIDE)
    clutter_actor_hide (closure->another_actor);
  else if (closure->finally == FINALLY_REMOVE)
    clutter_container_remove_actor (CLUTTER_CONTAINER (closure->another_actor),
                                    closure->actor);
  if (closure->another_actor)
    g_object_unref (closure->another_actor);
  free_effect (timeline, closure);
}

/*
 * Starts fading @actor to @opacity, and do @finally something to
 * @another_actor when it's complete.  If there's already such an
 * effect in progress it's overridden together with its @finally
 * action.
 */
static EffectClosure *
fade (ClutterTimeline * timeline, ClutterActor * actor, guint opacity,
      enum final_fade_action_t finally, ClutterActor * another_actor)
{
  EffectClosure *closure;

  g_assert ((finally == FINALLY_REST) == (another_actor == NULL));
  closure = linear_effect (timeline, actor, fade_frame, fade_complete,
                           (gdouble)clutter_actor_get_opacity(actor),
                           (gdouble)opacity, NAN);

  closure->finally = finally;
  if (another_actor)
    g_object_ref (another_actor);
  if (closure->another_actor)
    g_object_unref (closure->another_actor);
  closure->another_actor = another_actor;

  clutter_timeline_start (timeline);
  return closure;
}

/* The same as fade() except that it creates an independent disposable
 * %ClutterTimeline for $msecs for the effect. */
static EffectClosure *
fade_for_duration (guint msecs, ClutterActor * actor, guint opacity,
                   enum final_fade_action_t finally,
                   ClutterActor * another_actor)
{
  EffectClosure *closure;
  ClutterTimeline *timeline;

  timeline = clutter_timeline_new_for_duration (msecs);
  closure = fade (timeline, actor, opacity, finally, another_actor);
  g_object_unref (timeline);
  return closure;
}

/* Cancels the ongoing fade() effect on @actor if there one.
 * Then resets its @opacity and visibility to its normal state. */
static void
reset_opacity (ClutterActor * actor, guint opacity, gboolean be_shown)
{
  EffectClosure *closure;

  if ((closure = has_effect (actor, fade_frame)) != NULL)
    free_effect (closure->timeline, closure);
  clutter_actor_set_opacity (actor, opacity);
  if (be_shown)
    clutter_actor_show (actor);
  else
    clutter_actor_hide (actor);
}
/* Fading }}} */

/* Boom effect {{{ */
/* If @x0 <= @t <= @x1 returns the value of f(@t), where f()
 * goes from (@x0, @y0) to (@x1, @y1) following a cosine curve.
 * Do the math if you don't believe. */
static inline __attribute__((const)) gdouble
turnoff_fun (gdouble x0, gdouble y0, gdouble x1, gdouble y1, gdouble t)
{
  return ((y1-y0)*cos(t) + (y0*cos(x1)-y1*cos(x0))) / (cos(x1)-cos(x0));
}

/* ClutterTimeline::new-frame callback for turnoff_effect(). */
static void
turnoff_effect_frame (ClutterTimeline * timeline, gint frame,
                      EffectClosure * closure)
{
  gdouble now;

  // thwin scale-y    0.0 .. 0.4  cosine 1.0 .. 0.1
  // thwin scale-x    0.3 .. 0.64 cosine 1.0 .. 0.1
  // thwin opacity    0.5 .. 1.0  linear 255 .. 0.0
  // particle opacity 0.5 .. 1.0  sine   0.0 .. 1.0 .. 0.0
  // particle radius  0.5 .. 1.0  cosine 8.0 .. 72
  // particle angle   0.5 .. 1.0  linear 0.0 .. PI/2
  // particle scale   0.5 .. 1.0  linear 1.0 .. 0.5
  now = clutter_timeline_get_progress (timeline);

  /* @thwin */
  if (now <= 0.8)
    clutter_actor_set_scale (closure->actor,
                 now <= 0.3 ? 1.0 : turnoff_fun (0.3, 1, 0.64, 0.1, now),
                 now >= 0.4 ? 0.1 : turnoff_fun (0.0, 1, 0.4,  0.1, now));
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
          clutter_actor_set_scale (closure->particles[i].particle,
                                   1.5-now, 1.5-now);
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
  centery -= hd_scrollable_group_get_viewport_y (Grid);
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

/* %ClutterLabel utilities {{{ */
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

/* Query or set the line spacing of a #ClutterLabel if it's wrapping.
 * For some reason this attribute is lost after you've changed the
 * label's size so it needs to be restored. */
#define get_label_line_spacing(label) \
  pango_layout_get_spacing (clutter_label_get_layout (CLUTTER_LABEL (label)))
#define set_label_line_spacing(label, spacing) \
  pango_layout_set_spacing (clutter_label_get_layout (CLUTTER_LABEL (label)), \
                            spacing)

/* Returns pango's idea about the @label's height.
 * Unlike clutter_actor_get_height() this includes
 * linespace. */
static gint
get_real_label_height (ClutterActor * label)
{
  gint height;

  pango_layout_get_pixel_size(clutter_label_get_layout(CLUTTER_LABEL(label)),
                              NULL, &height);
  return height;
}

/* Returns the number of \n-s in @label. */
static guint
get_nlines_of_label (ClutterActor * label)
{
  guint i;
  const gchar *text;

  text = clutter_label_get_text (CLUTTER_LABEL (label));
  for (i = 1; *text; text++)
    if (*text == '\n')
      i++;
  return i;
}

/* Linespacing is lost every now and then. */
static void
preserve_linespacing (ClutterActor * actor, gpointer unused, gpointer spc)
{
  set_label_line_spacing (actor, GPOINTER_TO_INT (spc));
}
/* %ClutterLabel:s }}} */

/* Allocation callbacks {{{ */
/* #ClutterActor::notify::allocation callback to clip @actor to its size
 * whenever it changes.  Used to clip ClutterLabel:s to their allocated
 * size. */
static void
clip_on_resize (ClutterActor * actor)
{
  ClutterUnit width, height;

  clutter_actor_get_sizeu (actor, &width, &height);
  clutter_actor_set_clipu (actor, 0, 0, width, height);
}

/* Once you've specified an anchor point by gravity clutter forgets about it
 * and only remembers the coordinates.  Reinforce it according to the new size
 * of @actor. */
static void
reanchor_on_resize (ClutterActor * actor, gpointer unused, gpointer grv)
{
  clutter_actor_set_anchor_point_from_gravity (actor, GPOINTER_TO_INT (grv));
}
/* Allocation callbacks }}} */

/* %ClutterEffectCompleteFunc:s {{{ */
/* #ClutterEffectCompleteFunc to hide @other when the effect is complete.
 * Used when @actor fades in in the top of @other. */
static void
hide_when_complete (ClutterActor * actor, ClutterActor * other)
{
  clutter_actor_hide (other);
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

/* Can be called after show_when_complete() is done to fade in $actor,
 * probably a thwin of a nothumb. */
static void
fade_in_when_complete (ClutterActor * actor, gpointer msecs)
{
  if (has_effect (actor, fade_frame))
    /* A fade-out by free_thumb() must be in progress, don't override it. */
    return;
  clutter_actor_set_opacity (actor, 0);
  fade_for_duration (GPOINTER_TO_INT (msecs), actor, 255, FINALLY_REST, NULL);
}
/* %ClutterEffectCompleteFunc:s }}} */
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

/* Returns whether at least 2 thumbnails populate the switcher. */
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

/* Tells whether we have any notification, either in the
 * notification area or shown in the title area of some thumbnail,
 * and we haven't been to task navigator since then. */
gboolean
hd_task_navigator_has_unseen_notifications (void)
{
    return UnseenNotifications && hd_task_navigator_has_notifications();
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

/* Find which thumbnail represents the requested app. */
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

/* Scroll @Grid to the top. */
void
hd_task_navigator_scroll_back (HdTaskNavigator * self)
{
  hd_scrollable_group_set_viewport_y (Grid, 0);
}

/* Updates our #HdScrollableGroup's idea about the @Grid's height. */
static void
set_navigator_height (guint hnavigator)
{
  hd_scrollable_group_set_real_estate (Grid,
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
 
  _setThumbSizes();

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

  /*
   * Gaps are always the same, regardless of the number of thumbnails.
   * Leave the last row left-aligned.  Center the first pageful amount
   * of rows vertically, except when we have more than one pages; then
   * we know exactly where to start the first row.  This enables us to
   * show the titles of the thumbnails in the 4th row.
   */
  lout->xpos = layout_fun (SCREEN_WIDTH,
                           lout->thumbsize->width,
                           GRID_HORIZONTAL_GAP,
                           lout->cells_per_row);
  lout->last_row_xpos = lout->xpos;
  if (NThumbnails <= 12)
    lout->ypos = layout_fun (SCREEN_HEIGHT + GRID_TOP_MARGIN,
                             lout->thumbsize->height,
                             GRID_VERTICAL_GAP,
                             nrows_per_page);
  else
    lout->ypos = GRID_TOP_MARGIN + MARGIN_DEFAULT;
  lout->hspace = lout->thumbsize->width  + GRID_HORIZONTAL_GAP;
  lout->vspace = lout->thumbsize->height + GRID_VERTICAL_GAP;
}

/* Depending on the current @Thumbsize places the frame graphics
 * elements of @thumb where they should be. */
static void
layout_thumb_frame (const Thumbnail * thumb, const Flyops * ops)
{
  guint wt, ht, wb, hb;

  /* This is quite boring. */
  clutter_actor_get_size(thumb->frame.nw, &wt, &ht);
  clutter_actor_get_size(thumb->frame.sw, &wb, &hb);

  ops->move (thumb->frame.nm, wt, 0);
  ops->move (thumb->frame.ne, Thumbsize->width, 0);
  ops->move (thumb->frame.mw, 0, ht);
  ops->move (thumb->frame.me, Thumbsize->width, ht);
  ops->move (thumb->frame.sw, 0, Thumbsize->height);
  ops->move (thumb->frame.sm, wb, Thumbsize->height);
  ops->move (thumb->frame.se, Thumbsize->width, Thumbsize->height);

  ops->scale (thumb->frame.nm,
              (gdouble)(Thumbsize->width - 2*wt)
                / clutter_actor_get_width (thumb->frame.nm), 1);
  ops->scale (thumb->frame.sm,
              (gdouble)(Thumbsize->width - 2*wb)
                / clutter_actor_get_width (thumb->frame.sm), 1);
  ops->scale (thumb->frame.mw, 1,
              (gdouble)(Thumbsize->height - (ht+hb))
                / clutter_actor_get_height (thumb->frame.mw));
  ops->scale (thumb->frame.me, 1,
              (gdouble)(Thumbsize->height - (ht+hb))
                / clutter_actor_get_height (thumb->frame.me));
}

/* Lays out the inners of a notwin belonging to @thumb.
 * @oldsize tells the previous (to-be-changed) size of @thumb
 * and can be %NULL for the initial layout. */
static void
layout_notwin (Thumbnail * thumb, const GtkRequisition * oldthsize,
               const Flyops * ops)
{
  TNote *tnote;
  gboolean reload_icon;
  guint isize, msgdiv, maxmsg;
  gint x = 0, y = 0;
  guint width, height;
  guint xicon, ycount, xmsg;
  guint worig = 0, horig = 0, wmax, hmax;
  guint htime, hleft, hleftforme;

  tnote = thumb->tnote;
  if (!ops)
    ops = &Fly_at_once;

  /* How much space can we use in the prison?  There are fix margins
   * at the sides and at the bottom. */
  wmax = Thumbsize->width - 2*NOTE_MARGINS;
  hmax = Thumbsize->height - TITLE_HEIGHT;
  hleft = hmax - NOTE_BOTTOM_MARGIN;

  /* Determine the icon size and the gap between .time and .message. */
  if (THUMBSIZE_IS (large))
    {
      isize  = ICON_FINGER;
      msgdiv = MARGIN_DEFAULT;
      maxmsg = 3;
    }
  else if (THUMBSIZE_IS (medium))
    {
      isize  = ICON_STYLUS;
      msgdiv = MARGIN_HALF;
      maxmsg = 2;
    }
  else
    {
      isize  = ICON_STYLUS;
      msgdiv = 0;
      maxmsg = 1;
    }

  /* (Re)load the icon it if it hasn't been or its size is changing. */
  reload_icon = !tnote->icon
    || oldthsize == &Thumbsizes.large
    || Thumbsize == &Thumbsizes.large;
  if (reload_icon && tnote->icon)
    { /* Kill the icon and reload a different size. */
      clutter_actor_get_position (tnote->icon, &x, &y);
      clutter_container_remove_actor (
                       CLUTTER_CONTAINER (tnote->notwin),
                       tnote->icon);
      tnote->icon = NULL;
    }
  if (!tnote->icon)
    {
      tnote->icon = get_icon (hd_note_get_icon (tnote->hdnote),
                              isize);
      clutter_container_add_actor (
                       CLUTTER_CONTAINER (tnote->notwin),
                       tnote->icon);
      if (reload_icon)
        clutter_actor_set_position (tnote->icon, x, y);
    }

  /* Size and locate the notwin's guts. */
  /* .icon, .count: separated by a margin and centered together. */
  clutter_label_set_font_name (CLUTTER_LABEL (tnote->count),
                   THUMBSIZE_IS (large) ? LargeSystemFont : SystemFont);
  clutter_actor_get_size (tnote->count, &width, &height);
  xicon  = (wmax - (isize + NOTE_ICON_GAP + width)) / 2;
  ycount = (isize-height) / 2;
  hleft -= isize;

  /* .time: it can wrap, take care not to lose the line spacing.
   * See its natural (unwrapped) size then determine the height. */
  if (FLY (ops, smoothly))
    clutter_actor_get_size (tnote->time, &worig, &horig);
  clutter_label_set_font_name (CLUTTER_LABEL (tnote->time),
                   THUMBSIZE_IS (large) ? SystemFont : SmallSystemFont);
  clutter_actor_set_size (tnote->time, -1, -1);
  clutter_actor_get_size (tnote->time, &width, &htime);
  if (width > wmax)
    {
      width = wmax;
      clutter_actor_set_width (tnote->time, width);
      htime = get_real_label_height (tnote->time);
    }
  if (hleft < htime)
    { /* We must have lost linespacing or we have a 3-line-ling text. */
      g_critical ("$time too high (%u > %u", htime, hleft);
      hleft = 0;
    }
  else
    hleft -= htime;
  if (FLY (ops, smoothly))
    {
      clutter_actor_set_size (tnote->time, worig, horig);
      ops->resize (tnote->time, width, htime);
    }

  /*
   * .message: like with .time but make sure not to exceed the space left
   * in the prison.  It's not shown on @Thumbsizes.small nevertheless
   * they need to be sized/positioned so we can animate when we're moving
   * to a larger @Thumbsize.
   */
  hleft -= msgdiv;
  if (FLY (ops, smoothly))
    /* Only needed for flying. */
    clutter_actor_get_size (tnote->message, &worig, &horig);
  clutter_actor_set_size (tnote->message, -1, -1);
  clutter_actor_get_size (tnote->message, &width, &height);
  if (width > wmax || height > hleft
      || get_nlines_of_label (tnote->message) > maxmsg)
    {
      PangoLayout *lout;

      /* Make it wrapped if necessary. */
      if (width > wmax)
        {
          width = wmax;
          clutter_actor_set_width (tnote->message, width);
        }

      /* Check the line count and restrict to @maxmsg. */
      lout = clutter_label_get_layout(CLUTTER_LABEL(tnote->message));
      if (pango_layout_get_line_count (lout) > maxmsg)
        {
          PangoRectangle r;
          PangoLayoutIter *iter;

          /* Cut at the bottom of the @maxmsg:th line. */
          for (iter = pango_layout_get_iter (lout); maxmsg > 1;
               pango_layout_iter_next_line (iter))
            maxmsg--;

          pango_layout_iter_get_line_extents (iter, NULL, &r);
          pango_extents_to_pixels (&r, NULL);
          pango_layout_iter_free (iter);
          height = r.y + r.height;
        }
      else /* We can show all lines. */
        height = clutter_actor_get_height (tnote->message);
    }
  hleftforme = THUMBSIZE_IS (small) ? hleft/2 : hleft;
  if (height > hleftforme)
    height = hleftforme;
  xmsg = (wmax-width) / 2;
  if (!THUMBSIZE_IS (small))
    hleft -= height;
  if (FLY (ops, smoothly))
    { /* Don't fly if it's (will be shortly) hidden. */
      clutter_actor_set_size (tnote->message, worig, horig);
      ops->resize (tnote->message, width, height);
    }
  else
    clutter_actor_set_size (tnote->message, width, height);

  /* Finished with resizing, move them now. */
  y = TITLE_HEIGHT + hleft / 2;
  ops->move (tnote->icon,  NOTE_MARGINS + xicon, y);
  ops->move (tnote->count, NOTE_MARGINS + xicon + NOTE_ICON_GAP + isize,
             y+ycount);
  y += isize;
  ops->move (tnote->time, NOTE_MARGINS+wmax/2, y);
  y += htime + msgdiv;
  if (!THUMBSIZE_IS (small))
    {
      clutter_actor_show (tnote->message);
      ops->move (tnote->message, NOTE_MARGINS + xmsg, y);
    }
  else
    {
      clutter_actor_hide (tnote->message);
      clutter_actor_set_position (tnote->message, NOTE_MARGINS + xmsg, y);
    }

  /* The difficult part is over, let's finish with the decoration:
   * .background, .separator. */
  clutter_actor_get_size (tnote->background, &width, &height);
  ops->scale (tnote->background,
              (gdouble)Thumbsize->width / width,
              (gdouble)Thumbsize->height / height);
  ops->move (tnote->separator, NOTE_SEPARATOR_PADDING, 0);
  ops->scale (tnote->separator,
              (gdouble)(Thumbsize->width - 2*NOTE_SEPARATOR_PADDING)
                / clutter_actor_get_width (tnote->separator), 1);
}

/*
 * Lays out @Thumbnails on @Grid, and their inner portions.  Makes actors fly
 * if it's appropriate.  @newborn is either a new thumbnail or notification
 * to be displayed; it won't be animated.  Returns the position of the bottom
 * of the lowest thumbnail.  Also sets @Thumbsize.
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

  _setWindowGeometry();

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

      /* If it's a new row re/set @ythumb and @xthumb. */
      g_assert (lout.cells_per_row > 0);
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

      if (thumb_has_notification (thumb))
        /* nothumb or apthumb with a notification,
         * show it as a notification */
        layout_notwin (thumb, oldthsize, ops);

      if (thumb_is_application (thumb))
        {
          guint wprison, hprison;

          /* Whether it's visible or not set the scale so we can just
           * show the prison later. */
          wprison = Thumbsize->width  - 2*FRAME_WIDTH;
          hprison = Thumbsize->height - (FRAME_TOP_HEIGHT+FRAME_BOTTOM_HEIGHT);
          ops->scale (thumb->prison,
                      (gdouble)wprison / App_window_geometry.width,
                      (gdouble)hprison / App_window_geometry.height);
          layout_thumb_frame (thumb, ops);
        }

skip_the_circus:
      xthumb += lout.hspace;
    }

  return ythumb + Thumbsize->height;
}

/* Lays out the @Thumbnails in the @Grid. */
static void
layout (ClutterActor * newborn, gboolean newborn_is_notification)
{
  /* This layout machinery is based on invariants, which basically
   * means we don't pay much attention to what caused the layout
   * update, but we rely on the current state of matters. */
  set_navigator_height (layout_thumbs (newborn));

  if (newborn && animation_in_progress (Fly_effect_timeline))
    {
      show_when_complete (newborn);
      if (newborn_is_notification)
        add_effect_closure (Fly_effect_timeline, fade_in_when_complete,
                            newborn, GINT_TO_POINTER (NOTIFADE_IN_DURATION));
    }
}
/* Layout engine }}} */

/* %Thumbnail:s {{{ */
static MBWMClientWindow *actor_to_client_window (ClutterActor *win,
                                       const HdCompMgrClient **hcmgrcp);
static Bool
win_title_changed (MBWMClientWindow *win, int unused1, Thumbnail *thumb);

/*
 * Reset @thumb's title to the application's name.  Called to set the
 * initial title of a thumbnail (if it has no notifications otherwise)
 * or when it had, but that notification is removed from the switcher.
 */
static void
reset_thumb_title (Thumbnail * thumb)
{
  gboolean use_markup;
  const gchar *new_title;

  /* What to reset the title to? */
  if (thumb_has_notification (thumb))
    { /* To the notification summary. */
      new_title = hd_note_get_summary (thumb->tnote->hdnote);
      use_markup = FALSE;
    }
  else if (thumb->win)
    { /* Normal case. */
      new_title = thumb->win->name;
      use_markup = thumb->win->name_has_markup;
    }
  else
    { /* Client must be having its sweet dreams in hibernation. */
      new_title = thumb->saved_title;
      use_markup = thumb->title_had_markup;
    }

  g_assert (thumb->title != NULL);
  set_label_text_and_color (thumb->title, new_title,
                            thumb_has_notification (thumb)
                              ? &NotificationTextColor : &DefaultTextColor);
  clutter_label_set_use_markup (CLUTTER_LABEL(thumb->title), use_markup);
}

/* Creates @thumb->thwin.  The exact position of the inner actors is decided
 * by layout_thumbs().  Only the .title actor is created, which you'll need
 * to fill with content. */
static void
create_thwin (Thumbnail * thumb, ClutterActor * prison)
{
  /* .title */
  thumb->title = clutter_label_new ();
  clutter_label_set_font_name (CLUTTER_LABEL (thumb->title), SmallSystemFont);
  clutter_label_set_use_markup(CLUTTER_LABEL(thumb->title), TRUE);
  clutter_label_set_ellipsize(CLUTTER_LABEL(thumb->title), PANGO_ELLIPSIZE_END);
  clutter_actor_set_anchor_point_from_gravity (thumb->title, CLUTTER_GRAVITY_WEST);
  clutter_actor_set_position (thumb->title,
                              TITLE_LEFT_MARGIN, TITLE_HEIGHT / 2);

  /* .close, anchored at the top-right corner of the close graphics. */
  thumb->close = clutter_group_new ();
  clutter_actor_set_name (thumb->close, "close area");
  clutter_actor_set_size (thumb->close, CLOSE_AREA_SIZE, CLOSE_AREA_SIZE);
  clutter_actor_set_anchor_point (thumb->close,
                                  CLOSE_AREA_SIZE/2 + CLOSE_ICON_SIZE/2,
                                  CLOSE_AREA_SIZE/2 - CLOSE_ICON_SIZE/2);
  clutter_actor_set_reactive (thumb->close, TRUE);

  /* .close_app_icon, .close_notif_icon: anchor them at top-right. */
  thumb->close_app_icon   = hd_clutter_cache_get_texture (
                       "TaskSwitcherThumbnailTitleCloseIcon.png", TRUE);
  clutter_actor_set_anchor_point (thumb->close_app_icon,
                                  CLOSE_ICON_SIZE/2 - CLOSE_AREA_SIZE/2,
                                  CLOSE_ICON_SIZE/2 - CLOSE_AREA_SIZE/2);
  thumb->close_notif_icon = hd_clutter_cache_get_texture (
                "TaskSwitcherNotificationThumbnailCloseIcon.png", TRUE);
  clutter_actor_set_anchor_point (thumb->close_notif_icon,
                                  CLOSE_ICON_SIZE/2 - CLOSE_AREA_SIZE/2,
                                  CLOSE_ICON_SIZE/2 - CLOSE_AREA_SIZE/2);
  clutter_container_add (CLUTTER_CONTAINER (thumb->close),
                         thumb->close_app_icon, thumb->close_notif_icon,
                         NULL);
  if (thumb_has_notification (thumb))
    clutter_actor_hide (thumb->close_app_icon);
  else
    clutter_actor_hide (thumb->close_notif_icon);

  /* .plate */
  thumb->plate = clutter_group_new ();
  clutter_actor_set_name (thumb->plate, "plate");
  clutter_container_add (CLUTTER_CONTAINER (thumb->plate),
                         thumb->title, thumb->close, NULL);

  /* .thwin */
  thumb->thwin = clutter_group_new ();
  clutter_actor_set_name (thumb->thwin, "thumbnail");
  clutter_actor_set_reactive (thumb->thwin, TRUE);
  clutter_container_add (CLUTTER_CONTAINER (thumb->thwin),
                         prison, thumb->plate, NULL);
  clutter_container_add_actor (CLUTTER_CONTAINER (Grid), thumb->thwin);
}

/* Release everything related to @thumb.  If you want it can @animate the
 * death of @thumb by a simple fading out. */
static void
free_thumb (Thumbnail * thumb, gboolean animate)
{
  /* This will kill the entire actor hierarchy. */
  if (animate && CLUTTER_ACTOR_IS_VISIBLE (thumb->thwin))
    { /* We may be adding it, no point of animation then. */
      fade_for_duration (NOTIFADE_OUT_DURATION, thumb->thwin, 0,
                         FINALLY_REMOVE, CLUTTER_ACTOR (Grid));

      /* @thumb will be invalid very soon so prevent any signal handlers
       * doing harm while we're fading out. */
      g_signal_handlers_disconnect_matched (thumb->thwin,
                          G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, thumb);
      g_signal_handlers_disconnect_matched (thumb->close,
                          G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, thumb);
    }
  else
    clutter_container_remove_actor (CLUTTER_CONTAINER (Grid), thumb->thwin);

  /* The caller must have taken care of .tnote already. */
  g_assert (!thumb_has_notification (thumb));

  if (thumb_is_application (thumb))
    {
      if (thumb->apwin)
        g_object_unref (thumb->apwin);

      if (thumb->cemetery)
        {
          guint i;

          for (i = 0; i < thumb->cemetery->len; i++)
            g_object_weak_unref (thumb->cemetery->pdata[i],
                                 (GWeakNotify)g_ptr_array_remove_fast,
                                 thumb->cemetery);
          g_ptr_array_free (thumb->cemetery, TRUE);
          thumb->cemetery = NULL;
        }

      if (thumb->dialogs)
        {
          g_ptr_array_foreach (thumb->dialogs, (GFunc)g_object_unref, NULL);
          g_ptr_array_free (thumb->dialogs, TRUE);
          thumb->dialogs = NULL;
        }

      /* Releases thumb->win too. */
      if (thumb->win)
        mb_wm_object_signal_disconnect (MB_WM_OBJECT (thumb->win),
                                        thumb->win_changed_cb_id);

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

  _setWindowGeometry();
  /*
   * Take @apthumb->apwin into our care even if there is a video screenshot.
   * If we don't @apthumb->apwin will be managed by its current parent and
   * we cannot force hiding it which would result in a full-size apwin
   * appearing in the background if it's added in switcher mode.
   * TODO This may not be true anymore.
   */
  clutter_actor_reparent(apthumb->apwin, apthumb->windows);
  if (apthumb->cemetery)
    {
      guint i;

      for (i = 0; i < apthumb->cemetery->len; i++)
        {
          clutter_actor_reparent (apthumb->cemetery->pdata[i],
                                  apthumb->windows);
          clutter_actor_hide (apthumb->cemetery->pdata[i]);
        }
    }

  if (apthumb->dialogs)
    g_ptr_array_foreach (apthumb->dialogs,
                         (GFunc)clutter_actor_reparent,
                         apthumb->windows);

  /* Load the video screenshot and place its actor in the hierarchy. */
  if (need_to_load_video (apthumb))
    {
      /* Make it appear as if .video were .apwin,
       * having the same geometry. */
      g_assert (!apthumb->video);
      apthumb->video = load_image (apthumb->video_fname,
                                   App_window_geometry.width,
                                   App_window_geometry.height);
      if (apthumb->video)
        {
          clutter_actor_set_name (apthumb->video, "video");
          clutter_actor_set_position (apthumb->video,
                                      App_window_geometry.x,
                                      App_window_geometry.y);
          clutter_container_add_actor (CLUTTER_CONTAINER (apthumb->prison),
                                       apthumb->video);
        }
    }

  if (!apthumb->video)
    /* Needn't bother with show_all() the contents of .windows,
     * they are shown anyway because of reparent(). */
    clutter_actor_show (apthumb->windows);
  else
    /* Only show @apthumb->video. */
    clutter_actor_hide (apthumb->windows);

  /* Restore the opacity/visibility of the actors that have been faded out
   * while zooming, so we won't have trouble if we happen to to need to enter
   * the navigator directly. */
  clutter_actor_hide (apthumb->titlebar);
  clutter_actor_set_opacity (apthumb->plate, 255);
  if (thumb_has_notification (apthumb))
    {
      clutter_actor_hide (apthumb->prison);
      clutter_actor_set_opacity (apthumb->tnote->notwin, 255);
    }
}

/* Stop managing @apthumb's application window and give it back
 * to its original parent. */
static void
release_win (const Thumbnail * apthumb)
{
  hd_render_manager_return_app (apthumb->apwin);
  if (apthumb->cemetery)
    g_ptr_array_foreach (apthumb->cemetery,
                         (GFunc)hd_render_manager_return_app, NULL);
  if (apthumb->dialogs)
    g_ptr_array_foreach (apthumb->dialogs,
                         (GFunc)hd_render_manager_return_dialog, NULL);
}

/* Hide our .prison behind @tnote->notwin and reset_thumb_title(). */
static void
adopt_notification (Thumbnail * apthumb, TNote * tnote)
{
  g_assert (!apthumb->tnote);
  apthumb->tnote = tnote;
  clutter_container_add_actor (CLUTTER_CONTAINER (apthumb->thwin),
                               tnote->notwin);
  clutter_container_raise_child (CLUTTER_CONTAINER (apthumb->thwin),
                                 tnote->notwin, apthumb->prison);

  g_assert (CLUTTER_ACTOR_IS_VISIBLE (apthumb->prison));
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (tnote->notwin));
  if (hd_task_navigator_is_active ())
    {
      ClutterTimeline *timeline;

      /* Fade in @notwin and the decoration. */
      timeline = clutter_timeline_new_for_duration (NOTIFADE_IN_DURATION);
      clutter_actor_set_opacity (tnote->notwin, 0);
      fade (timeline, tnote->notwin, 255, FINALLY_HIDE, apthumb->prison);
      fade (timeline, apthumb->frame.all, 0, FINALLY_HIDE, apthumb->frame.all);
      clutter_actor_show (apthumb->close_notif_icon);
      fade (timeline, apthumb->close_notif_icon, 255, FINALLY_REST, NULL);
      if (CLUTTER_ACTOR_IS_VISIBLE (apthumb->close_app_icon))
        fade (timeline, apthumb->close_app_icon, 0,
              FINALLY_HIDE, apthumb->close_app_icon);
      else /* Reset its opacity to normal. */
        clutter_actor_set_opacity (apthumb->close_app_icon, 0);
      g_object_unref (timeline);
    }
  else
    { /* Make sure all opacities are reset to the normal values. */
      g_assert (!has_effect (tnote->notwin, fade_frame));
      clutter_actor_hide (apthumb->prison);
      reset_opacity (apthumb->frame.all, 0, FALSE);
      reset_opacity (apthumb->close_notif_icon, 255, TRUE);
      reset_opacity (apthumb->close_app_icon, 0, FALSE);
    }

  reset_thumb_title (apthumb);
}

/* Restore .prison. */
static TNote *
orphan_notification (Thumbnail * apthumb, gboolean animate)
{
  TNote *tnote;

  tnote = apthumb->tnote;
  g_assert (tnote != NULL);

 /* Take care not to blow @notwin. */
  g_object_ref (tnote->notwin);
  if (!animate || !hd_task_navigator_is_active ())
    clutter_container_remove_actor (CLUTTER_CONTAINER (apthumb->thwin),
                                    tnote->notwin);

  clutter_actor_show (apthumb->prison);
  clutter_actor_show (apthumb->frame.all);
  g_assert (CLUTTER_ACTOR_IS_VISIBLE (apthumb->close_notif_icon));
  if (hd_task_navigator_is_active ())
    {
      ClutterTimeline *timeline;

      /* If @animate fade out @notwin, otherwise just the decoration.
       * Couple with the fly effect so they will be synchronized. */
      timeline = clutter_timeline_new_for_duration (NOTIFADE_OUT_DURATION);
      if (animate)
        fade (timeline, tnote->notwin, 0, FINALLY_REMOVE, apthumb->thwin);
      fade (timeline, apthumb->frame.all, 255, FINALLY_REST, NULL);
      fade (timeline, apthumb->close_notif_icon, 0,
            FINALLY_HIDE, apthumb->close_notif_icon);
      if (!apthumb_has_dialogs (apthumb))
        {
          clutter_actor_show (apthumb->close_app_icon);
          fade (timeline, apthumb->close_app_icon, 255, FINALLY_REST, NULL);
        }
      g_object_unref(timeline);
    }
  else
    { /* Reset opacities to normal values. */
      reset_opacity (tnote->notwin, 255, TRUE);
      reset_opacity (apthumb->frame.all, 255, TRUE);
      reset_opacity (apthumb->close_notif_icon, 0, FALSE);
      reset_opacity (apthumb->close_app_icon, 255,
                     !apthumb_has_dialogs(apthumb));
    }

  apthumb->tnote = NULL;
  reset_thumb_title (apthumb);
  return tnote;
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
/*
 * Tells how to scale and position @Scroller to show a prison
 * appearing the same as in application view (at the start of
 * a zoom-out or at the end of a zoom-in).  The prisons screen
 * coordinates and scaling are given in switcher view.
 */
static void
zoom_fun (gint * xposp, gint * yposp,
          gdouble * xscalep, gdouble * yscalep)
{
  _setWindowGeometry();
  /* The prison represents what's in @App_window_geometry in app view. */
  *xscalep = 1 / *xscalep;
  *yscalep = 1 / *yscalep;
  *xposp = -*xposp * *xscalep + App_window_geometry.x;
  *yposp = -*yposp * *yscalep + App_window_geometry.y;
}

/* Zoom the navigator itself so that when the effect is complete the
 * non-decoration part of @apthumb->apwin is in its regular position
 * and size in application view. */
static void
zoom_in (const Thumbnail * apthumb)
{
  gint xpos, ypos;
  gdouble xscale, yscale;

  /* @xpos, @ypos := .prison's absolute coordinates. */
  clutter_actor_get_position  (apthumb->thwin,  &xpos,    &ypos);
  clutter_actor_get_scale     (apthumb->prison, &xscale,  &yscale);
  ypos -= hd_scrollable_group_get_viewport_y (Grid);
  xpos += PRISON_XPOS;
  ypos += PRISON_YPOS;

  /* If zoom-in is already in progress this will just change its direction
   * such that it will focus on @apthumb's current position. */
  zoom_fun (&xpos, &ypos, &xscale, &yscale);
  scale_effect (Zoom_effect_timeline, Scroller, xscale, yscale);
  move_effect  (Zoom_effect_timeline, Scroller, xpos,   ypos);
}

/* Called when the position or the size (scale) of @apthumb has changed,
 * most probably as a consequence of a new or removed thumbnail.  Make
 * sure @apthumb remains in focus. */
static void
rezoom (ClutterActor * actor, GParamSpec * unused, const Thumbnail * apthumb)
{
  zoom_in (apthumb);
}

/* add_effect_closure() callback for hd_task_navigator_zoom_in()
 * to leave the navigator. */
static void
zoom_in_complete (ClutterActor * navigator, const Thumbnail * apthumb)
{
  g_signal_handlers_disconnect_by_func (apthumb->prison,
                                        rezoom, (Thumbnail *)apthumb);
  g_signal_handlers_disconnect_by_func (apthumb->thwin,
                                        rezoom, (Thumbnail *)apthumb);
  g_signal_emit_by_name (Navigator, "zoom-in-complete", apthumb->apwin);
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
  const Thumbnail *apthumb;

  g_assert (hd_task_navigator_is_active ());
  if (!(apthumb = find_by_apwin (win)))
    goto damage_control;

  /* Must have gotten a gtk_window_present() during a zooming, ignore it. */
  if (animation_in_progress (Zoom_effect_timeline))
    goto damage_control;

  /* This is the actual zooming, but we do other effects as well. */
  hd_render_manager_unzoom_background ();
  zoom_in (apthumb);

  /* Crossfade .plate with .titlebar. */
  clutter_actor_show (apthumb->titlebar);
  clutter_actor_set_opacity (apthumb->titlebar, 0);
  clutter_effect_fade (Zoom_effect, apthumb->titlebar, 255, NULL, NULL);
  clutter_effect_fade (Zoom_effect, apthumb->plate,      0, NULL, NULL);

  /* Fade out our notification smoothly if we have one. */
  if (thumb_has_notification (apthumb))
    { /* fade() robustly to be friendly with ongoing adopt/
       * orphan_notification() */
      clutter_actor_show (apthumb->prison);
      fade (Zoom_effect_timeline, apthumb->tnote->notwin, 0,
            FINALLY_REST, NULL);
    }

  /*
   * rezoom() if @apthumb changes its position while we're trying to
   * zoom it in.  This may happen if somebody adds a window or adds
   * or removes a notification.  While zooming out it's not important
   * but when zooming in we want @win to be in the right position.
   * During the animation @apthumb is valid.  The only way it could
   * disappear is by removing it, but remove_window() is deferred
   * until zomming is finished.
   */
  g_signal_connect (apthumb->thwin, "notify::allocation",
                    G_CALLBACK(rezoom), (Thumbnail *)apthumb);
  g_signal_connect (apthumb->prison, "notify::scale-x",
                    G_CALLBACK(rezoom), (Thumbnail *)apthumb);
  g_signal_connect (apthumb->prison, "notify::scale-y",
                    G_CALLBACK(rezoom), (Thumbnail *)apthumb);

  /* Clean up, exit and call @fun when then animation is finished. */
  add_effect_closure (Zoom_effect_timeline,
                      (ClutterEffectCompleteFunc)zoom_in_complete,
                      CLUTTER_ACTOR (self), (Thumbnail *)apthumb);
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
  gdouble xscale, yscale;
  gint yarea, xpos, ypos;

  /* Our "show" callback will grab the butts of @win. */
  clutter_actor_show (Navigator);
  if (!(apthumb = find_by_apwin (win)))
    goto damage_control;

  /* @xpos, @ypos:= intended real position of @apthumb */
  g_assert (Thumbsize != NULL);
  clutter_actor_get_position (apthumb->thwin,  &xpos, &ypos);

  /*
   * Scroll the @Grid so that @apthumb is closest the middle of the screen.
   * #HdScrollableGroup does not let us scroll the viewport out of the real
   * estate, but in return we need to ask how much we actually managed to
   * scroll.
   */
  yarea = ypos - (SCREEN_HEIGHT - Thumbsize->height) / 2;
  hd_scrollable_group_set_viewport_y (Grid, yarea);
  yarea = hd_scrollable_group_get_viewport_y (Grid);

  /* Make @ypos absolute (relative to the top of the screen). */
  ypos -= yarea;

  /* @xpos, @ypos := absolute position of .prison. */
  xpos += PRISON_XPOS;
  ypos += PRISON_YPOS;
  clutter_actor_get_scale (apthumb->prison, &xscale, &yscale);

  /* Reposition and rescale the @Scroller so that .apwin is shown exactly
   * in the same position and size as in the application view. */
  zoom_fun (&xpos, &ypos, &xscale, &yscale);
  clutter_actor_set_scale     (Scroller, xscale,  yscale);
  clutter_actor_set_position  (Scroller, xpos,    ypos);
  clutter_effect_scale (Zoom_effect, Scroller, 1, 1, NULL, NULL);
  clutter_effect_move  (Zoom_effect, Scroller, 0, 0, NULL, NULL);

  /* Crossfade .plate with .titlebar.  (Earlier i said "It's okay to leave
   * .titlebar shown but transparent." but i can't recall why.  Anyway,
   * let's hide it afterwards.) */
  clutter_actor_show (apthumb->titlebar);
  clutter_actor_set_opacity (apthumb->titlebar, 255);
  clutter_effect_fade (Zoom_effect, apthumb->titlebar,   0,
                       (ClutterEffectCompleteFunc)hide_when_complete,
                       apthumb->titlebar);
  clutter_actor_set_opacity (apthumb->plate,      0);
  clutter_effect_fade (Zoom_effect, apthumb->plate,    255, NULL, NULL);

  /* Fade in .notwin smoothly. */
  if (thumb_has_notification (apthumb))
    { /* Use robust fade()ing for the same reason as in *_zoom_in(). */
      clutter_actor_show (apthumb->prison);
      clutter_actor_set_opacity (apthumb->tnote->notwin, 0);
      fade (Zoom_effect_timeline, apthumb->tnote->notwin, 255,
            FINALLY_HIDE, apthumb->prison);
    }

  add_effect_closure (Zoom_effect_timeline, fun, win, funparam);
  return;

damage_control:
  if (fun != NULL)
    fun (win, funparam);
}
/* Zooming }}} */

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

/* Called when a %Thumbnail.thwin is clicked. */
static gboolean
appthumb_clicked (const Thumbnail * apthumb)
{
  if (hd_render_manager_get_state () != HDRM_STATE_TASK_NAV)
    /* Bloke clicked a home applet while exiting the switcher,
     * which got through the input viewport and would mess up
     * things in hd_switcher_zoom_in_complete(). */
    return TRUE;

  if (animation_in_progress (Fly_effect_timeline)
      || animation_in_progress (Zoom_effect_timeline))
    /* Clicking on the thumbnail while it's zooming would result in multiple
     * delivery of "thumbnail-clicked". */
    return TRUE;

  /* Behave like a notification if we have one. */
  if (thumb_has_notification (apthumb))
    g_signal_emit_by_name (Navigator, "notification-clicked",
                           apthumb->tnote->hdnote);
  else
    g_signal_emit_by_name (Navigator, "thumbnail-clicked", apthumb->apwin);

  return TRUE;
}

/* Called when a %Thumbnail.close (@thwin's close button) is clicked. */
static gboolean
appthumb_close_clicked (const Thumbnail * apthumb)
{
  if (hd_render_manager_get_state () != HDRM_STATE_TASK_NAV)
    /* Be consistent with appthumb_clicked(). */
    return TRUE;

  if (animation_in_progress (Fly_effect_timeline)
      || animation_in_progress (Zoom_effect_timeline))
    /* Closing an application while it's zooming would crash us. */
    /* Maybe not anymore but let's play safe. */
    return TRUE;

  if (thumb_has_notification (apthumb))
    g_signal_emit_by_name (Navigator, "notification-closed",
                           apthumb->tnote->hdnote);
  else
    /* Report a regular click on the thumbnail (and make %HdSwitcher zoom in)
     * if the application has open dialogs. */
    g_signal_emit_by_name (Navigator,
                           apthumb_has_dialogs (apthumb)
                             ? "thumbnail-clicked" : "thumbnail-closed",
                           apthumb->apwin);
  return TRUE;
}

/* Called when the %MBWM_WINDOW_PROP_NAME of @win has changed. */
static Bool
win_title_changed (MBWMClientWindow * win, int unused1, Thumbnail * apthumb)
{
  if (!thumb_has_notification (apthumb))
    reset_thumb_title (apthumb);
  return True;
}

/* Dress a %Thumbnail: create @thumb->frame.all and populate it
 * with frame graphics. */
static void
create_apthumb_frame (Thumbnail * apthumb)
{
  static struct
  {
    const gchar *fname;
    ClutterGravity gravity;
  } frames[] =
  {
    { "TaskSwitcherThumbnailTitleLeft.png",    CLUTTER_GRAVITY_NORTH_WEST },
    { "TaskSwitcherThumbnailTitleCenter.png",  CLUTTER_GRAVITY_NORTH_WEST },
    { "TaskSwitcherThumbnailTitleRight.png",   CLUTTER_GRAVITY_NORTH_EAST },
    { "TaskSwitcherThumbnailBorderLeft.png",   CLUTTER_GRAVITY_NORTH_WEST },
    { "TaskSwitcherThumbnailBorderRight.png",  CLUTTER_GRAVITY_NORTH_EAST },
    { "TaskSwitcherThumbnailBottomLeft.png",   CLUTTER_GRAVITY_SOUTH_WEST },
    { "TaskSwitcherThumbnailBottomCenter.png", CLUTTER_GRAVITY_SOUTH_WEST },
    { "TaskSwitcherThumbnailBottomRight.png",  CLUTTER_GRAVITY_SOUTH_EAST },
  };

  guint i;

  apthumb->frame.all = clutter_group_new ();
  clutter_actor_set_name (apthumb->frame.all, "apthumb frame");

  for (i = 0; i < G_N_ELEMENTS (frames); i++)
    {
      apthumb->frame.pieces[i] = hd_clutter_cache_get_texture (
                                                 frames[i].fname, TRUE);
      clutter_actor_set_anchor_point_from_gravity (apthumb->frame.pieces[i],
                                                   frames[i].gravity);
      clutter_container_add_actor (CLUTTER_CONTAINER (apthumb->frame.all),
                                   apthumb->frame.pieces[i]);
    }
}

/* Returns a %Thumbnail for @apwin, a window manager client actor.
 * If there is a notification for this application it will be removed
 * and added as the thumbnail title. */
static Thumbnail *
create_appthumb (ClutterActor * apwin)
{
  GList *li;
  Thumbnail *apthumb, *nothumb;
  const HdLauncherApp *app;
  const HdCompMgrClient *hmgrc;

  apthumb = g_new0 (Thumbnail, 1);
  apthumb->type = APPLICATION;

  _setWindowGeometry();

  /* We're just in a MapNotify, it shouldn't happen.
   * mb_wm_object_signal_connect() will take reference
   * of apthumb->win. */
  apthumb->win = actor_to_client_window (apwin, &hmgrc);
  g_assert (apthumb->win != NULL);
  apthumb->win_changed_cb_id = mb_wm_object_signal_connect (
                     MB_WM_OBJECT (apthumb->win), MBWM_WINDOW_PROP_NAME,
                     (MBWMObjectCallbackFunc)win_title_changed, apthumb);

  /* .nodest: try the property first then fall back to the WM_CLASS hint.
   * TODO This is temporary, just not to break the little functionality
   *      we already have. */
  mb_wm_util_async_trap_x_errors (apthumb->win->wm->xdpy);
  apthumb->nodest = hd_util_get_x_window_string_property (
                                apthumb->win->wm, apthumb->win->xwindow,
                                HD_ATOM_NOTIFICATION_THREAD);
  if (!apthumb->nodest)
    {
      XClassHint xwinhint;

      if (XGetClassHint (apthumb->win->wm->xdpy, apthumb->win->xwindow,
                         &xwinhint))
        {
          apthumb->nodest = xwinhint.res_class;
          XFree (xwinhint.res_name);
        }
      else
        g_warning ("XGetClassHint(%lx): failed", apthumb->win->xwindow);
    }
  mb_wm_util_async_untrap_x_errors ();

  /* .video_fname */
  if ((app = hd_comp_mgr_client_get_launcher (HD_COMP_MGR_CLIENT (hmgrc))) != NULL)
    apthumb->video_fname = hd_launcher_app_get_switcher_icon (HD_LAUNCHER_APP (app));

  /* Now the actors: .apwin, .titlebar, .windows. */
  apthumb->apwin = g_object_ref (apwin);
  apthumb->titlebar = hd_title_bar_create_fake(HD_COMP_MGR_LANDSCAPE_WIDTH);
  apthumb->windows = clutter_group_new ();
  clutter_actor_set_name (apthumb->windows, "windows");
  clutter_actor_set_clip (apthumb->windows,
                          App_window_geometry.x, App_window_geometry.y,
                          App_window_geometry.width, App_window_geometry.height);
  /* See mb_wm_comp_mgr_clutter_client_actor_reparent_cb - we check this to
   * see if we should linear filter the actor or not */
  g_object_set_data(G_OBJECT(apthumb->windows), "FILTER_LINEAR", (void*)1);

  /* .prison: anchor it so that we can ignore the UI framework area
   * of its contents.  Do so even if @apwin is really fullscreen,
   * ie. ignore the parts that would be in place of the title bar. */
  apthumb->prison = clutter_group_new ();
  clutter_actor_set_name (apthumb->prison, "prison");
  clutter_actor_set_anchor_point (apthumb->prison,
                                  App_window_geometry.x,
                                  App_window_geometry.y);
  clutter_actor_set_position (apthumb->prison, PRISON_XPOS, PRISON_YPOS);
  clutter_container_add (CLUTTER_CONTAINER (apthumb->prison),
                         apthumb->titlebar, apthumb->windows, NULL);

  /* Have .thwin created. */
  create_thwin (apthumb, apthumb->prison);
  g_signal_connect_swapped (apthumb->thwin, "button-release-event",
                            G_CALLBACK (appthumb_clicked), apthumb);
  g_signal_connect_swapped (apthumb->close, "button-release-event",
                            G_CALLBACK (appthumb_close_clicked),
                            apthumb);

  /* Add our .frame. */
  create_apthumb_frame (apthumb);
  clutter_container_add_actor (CLUTTER_CONTAINER (apthumb->plate),
                               apthumb->frame.all);
  clutter_actor_lower_bottom (apthumb->frame.all);

  /* Do we have a notification for @apwin? */
  for_each_notification (li, nothumb)
    {
      if (tnote_matches_thumb (nothumb->tnote, apthumb))
        { /* Yes, steal it from the the @Grid. */
          adopt_notification (apthumb, remove_nothumb (li, FALSE));
          break;
        }
    }

  /* Finally, have a title. */
  if (!thumb_has_notification (apthumb))
    reset_thumb_title (apthumb);

  return apthumb;
}

/* #ClutterEffectCompleteFunc for hd_task_navigator_remove_window()
 * called when the TV-turned-off effect of @apthumb finishes. */
static void
appthumb_turned_off_1 (ClutterActor * unused, Thumbnail * apthumb)
{ /* Byebye @apthumb! */
  release_win (apthumb);
  free_thumb (apthumb, FALSE);
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
      g_critical ("%s: window actor %p not found.  This is most likely "
                  "not a switcher bug, but indicates a stacking problem. "
                  "See? Good.", __FUNCTION__, win);
      return;
    }

  /*
   * If @win had a notification, add it to @Grid as a standalone thumbnail.
   * TODO It'd be nice to show the notification squeezing but we can't
   * because %TNote must belong to exactly one %Thumbnail a time.
   * Maybe %ClutterTexture could be used to work it around.
   */
  newborn = thumb_has_notification (apthumb)
    ? add_nothumb (orphan_notification (apthumb, FALSE))->thwin
    : NULL;

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
      release_win (apthumb);
      free_thumb (apthumb, FALSE);
    }

  /* Do all (TV, windows flying, add notification thumbnail) effects at once. */
  Thumbnails = g_list_delete_link (Thumbnails, li);
  NThumbnails--;
  layout (newborn, TRUE);

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

  layout (apthumb->thwin, FALSE);

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
  if (!apthumb->dialogs->len && !thumb_has_notification (apthumb))
    clutter_actor_show (apthumb->close_app_icon);

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
  clutter_actor_hide (apthumb->close_app_icon);
}
/* Add/remove windows }}} */

/* Misc window commands {{{ */
/* Prepare for the client of @win being hibernated.
 * Undone when @win is replaced by the woken-up client's new actor. */
void
hd_task_navigator_hibernate_window (HdTaskNavigator * self,
                                    ClutterActor * win)
{
  Thumbnail *apthumb;

  if (!(apthumb = find_by_apwin (win)))
    return;

  /* Hibernating clients twice is a nonsense. */
  g_return_if_fail (apthumb->win != NULL);

  /* Save the window name and markupability for reset_thumb_title(). */
  g_return_if_fail (apthumb->win->name);
  apthumb->saved_title = g_strdup (apthumb->win->name);
  apthumb->title_had_markup = apthumb->win->name_has_markup;

  /* Release .win. */
  mb_wm_object_signal_disconnect (MB_WM_OBJECT (apthumb->win),
                                  apthumb->win_changed_cb_id);
  apthumb->win = NULL;
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

  /* Resurrect @new_win in the .cemetery. */
  if (apthumb->cemetery)
    {
      guint i;

      /* Verify that @old_win (current .apwin) is not in .cemetery yet. */
      for (i = 0; i < apthumb->cemetery->len; i++)
        g_assert (apthumb->cemetery->pdata[i] != old_win);
      if (g_ptr_array_remove_fast (apthumb->cemetery, new_win))
        g_object_weak_unref (G_OBJECT (new_win),
                             (GWeakNotify)g_ptr_array_remove_fast,
                             apthumb->cemetery);
    }
  else
    apthumb->cemetery = g_ptr_array_new ();

  /* Add @old_win to .cemetery.  Do it before we unref @old_win,
   * so we don't possibly add a dangling pointer there. */
  g_ptr_array_add (apthumb->cemetery, old_win);
  g_object_weak_ref (G_OBJECT (old_win),
                         (GWeakNotify)g_ptr_array_remove_fast,
                         apthumb->cemetery);

  /* Replace .apwin */
  showing = hd_task_navigator_is_active ();
  if (showing) /* .apwin is in the cemetery */
    clutter_actor_hide (apthumb->apwin);
  g_object_unref (apthumb->apwin);
  apthumb->apwin = g_object_ref (new_win);
  if (showing)
    clutter_actor_reparent (apthumb->apwin, apthumb->windows);

  /* Replace the client window structure with @new_win's. */
  if (apthumb->win)
    mb_wm_object_signal_disconnect (MB_WM_OBJECT (apthumb->win),
                                    apthumb->win_changed_cb_id);
  apthumb->win = actor_to_client_window (new_win, NULL);
  if (apthumb->win)
    {
      g_free (apthumb->saved_title);
      apthumb->saved_title = NULL;
      apthumb->win_changed_cb_id = mb_wm_object_signal_connect (
                     MB_WM_OBJECT (apthumb->win), MBWM_WINDOW_PROP_NAME,
                     (MBWMObjectCallbackFunc)win_title_changed, apthumb);

      /* Update the title now if it's shown (and not a notification). */
      if (!thumb_has_notification (apthumb))
        reset_thumb_title (apthumb);
    }
}

/*
 * Sets the @win's %Thumbnail's @nodest.  @nodest == %NULL clears it.
 * It is an error if the associated %Thumbnail cannot be found, but
 * we'll do nothing then.  If it already has a notification it will be
 * swapped out.  If there's a notification destined for @nothread it
 * will be taken by @win's thumbnail.  If other application thumbnail
 * has such a notification it will be taken away and the other thumbnail
 * will be deprived of its @nodest.  This is designed to allow clients
 * to change threads in arbitrary order to transfer notifications from
 * one thumbnail to another.
 *
 * The callee is responsible for managing @nothread.
 */
void
hd_task_navigator_notification_thread_changed (HdTaskNavigator * self,
                                               ClutterActor * win,
                                               char * nothread)
{
  GList *li;
  gboolean relayout;
  ClutterActor *newborn;
  Thumbnail *apthumb, *thumb;

  /* Get @apthumb. */
  if (!(apthumb = find_by_apwin (win)))
    {
      if (nothread)
        XFree (nothread);
      return;
    }

  /* Has anything changed? */
  if (!nothread && !apthumb->nodest)
    return;
  if (nothread && apthumb->nodest && !strcmp (nothread, apthumb->nodest))
    {
      XFree (nothread);
      return;
    }

  /* Drop our notification if we have any.  layout() later,
   * when we've finished recreating frames as necessary. */
  newborn = NULL;
  relayout = FALSE;
  if (thumb_has_notification (apthumb))
    newborn = add_nothumb (orphan_notification (apthumb, FALSE))->thwin;
  if (apthumb->nodest)
    {
      XFree (apthumb->nodest);
      apthumb->nodest = NULL;
    }

  /* @nothread -> @apthumb. */
  if (!nothread)
    goto finito;
  apthumb->nodest = nothread;

  /* Search for notifications destined to @nothread and claim
   * the first one we can find. */
  relayout = FALSE;
  for_each_thumbnail (li, thumb)
    if (thumb_has_notification (thumb)
        && tnote_matches_thumb (thumb->tnote, apthumb))
      {
        if (thumb_is_application (thumb))
          { /* It's another application's, take it away. */
            adopt_notification (apthumb, orphan_notification (thumb, FALSE));
            if (thumb->nodest)
              { /* Make sure @thumb won't receive our notifications. */
                XFree (thumb->nodest);
                thumb->nodest = NULL;
              }
          }
        else
          { /* Individual notification thumbnail, kidnap it. */
            adopt_notification (apthumb, remove_nothumb (li, FALSE));
            relayout = TRUE;
          }
        break;
      }

finito:
  if (newborn || relayout)
    layout (newborn, TRUE);
}
/* Misc window commands }}} */
/* }}} */

/* Notification thumbnails {{{ */
/* %TNote:s {{{ */
/* Compares two stringified base-10 numbers. */
static gint
numstrcmp(const gchar * a, const gchar * b)
{
  gint decision;

  if (!a || !b)
    return 0;
  decision = 0;

  for (; ; a++, b++)
    {
      if ( *a && !*b)
              return  1;
      if (!*a &&  *b)
              return -1;
      if (!*a && !*b)
              return  decision;
      if (decision)
              continue;
      if (*a < *b)
              decision = -1;
      if (*a > *b)
              decision =  1;
    }
}

/* HdNote::HdNoteSignalChanged signal handler. */
static Bool
tnote_changed (HdNote * hdnote, int unused1, TNote * tnote)
{ g_debug(__FUNCTION__);
  GList *li;
  Thumbnail *thumb;
  gboolean is_more;
  const char *iname, *oname;

  for_each_thumbnail (li, thumb)
    if (thumb->tnote == tnote)
      break;
  g_assert (thumb != NULL);

  is_more = numstrcmp (clutter_label_get_text (CLUTTER_LABEL (tnote->count)),
                       hd_note_get_count (tnote->hdnote)) < 0;
  set_label_text_and_color (tnote->time,
                            hd_note_get_time (tnote->hdnote),
                            NULL);
  set_label_text_and_color (tnote->count,
                            hd_note_get_count (tnote->hdnote),
                            NULL);
  set_label_text_and_color (tnote->message,
                            hd_note_get_message (tnote->hdnote),
                            NULL);

  if ((iname = hd_note_get_icon (tnote->hdnote)) != NULL)
    { /* Replace icon? */
      if (!(oname = clutter_actor_get_name (tnote->icon))
          || strcmp (iname, oname))
        { /* Ignore further errors. */
          clutter_container_remove_actor (CLUTTER_CONTAINER (tnote->notwin),
                                          tnote->icon);
          tnote->icon = get_icon (iname, THUMBSIZE_IS (large)
                                  ? ICON_FINGER : ICON_STYLUS);
          clutter_container_add_actor (CLUTTER_CONTAINER (tnote->notwin),
                                       tnote->icon);
        }
    }

  layout_notwin (thumb, NULL, NULL);

  reset_thumb_title (thumb);
  if (is_more)
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

  /* Decoration */
  /* .background */
  tnote->background = hd_clutter_cache_get_texture (
                         "TaskSwitcherNotificationThumbnail.png", TRUE);
  tnote->separator = hd_clutter_cache_get_texture (
                "TaskSwitcherNotificationThumbnailSeparator.png", TRUE);

  /* Inners */
  /* .count */
  tnote->count = set_label_text_and_color (clutter_label_new (),
                                      hd_note_get_count (tnote->hdnote),
                                      &NotificationTextColor);

  /* .time */
  tnote->time = set_label_text_and_color (clutter_label_new (),
                                       hd_note_get_time (tnote->hdnote),
                                       &NotificationSecondaryTextColor);
  clutter_label_set_line_wrap (CLUTTER_LABEL (tnote->time), TRUE);
  clutter_label_set_alignment (CLUTTER_LABEL(tnote->time),
                               PANGO_ALIGN_CENTER);
  set_label_line_spacing (tnote->time, NOTE_TIME_LINESPACING);
  g_signal_connect (tnote->time, "notify::allocation",
                    G_CALLBACK (reanchor_on_resize),
                    GINT_TO_POINTER (CLUTTER_GRAVITY_NORTH));
  if (NOTE_TIME_LINESPACING != 0)
    g_signal_connect (tnote->time, "notify::allocation",
                      G_CALLBACK (preserve_linespacing),
                      GINT_TO_POINTER (NOTE_TIME_LINESPACING));

  /* .message */
  tnote->message = set_label_text_and_color (clutter_label_new (),
                                    hd_note_get_message (tnote->hdnote),
                                    &NotificationTextColor);
  clutter_label_set_font_name (CLUTTER_LABEL (tnote->message),
                               SmallSystemFont);
  clutter_label_set_line_wrap (CLUTTER_LABEL (tnote->message), TRUE);
  clutter_label_set_alignment (CLUTTER_LABEL(tnote->message),
                               PANGO_ALIGN_CENTER);
  g_signal_connect (tnote->message, "notify::allocation",
                    G_CALLBACK (clip_on_resize), NULL);

  /* .icon will be set by layout_thumbs() because we can't decide yet
   * what size to use because we don't know whether we'll get our own
   * thumbnail. */
  tnote->notwin = clutter_group_new();
  clutter_actor_set_name (tnote->notwin, "notwin");
  clutter_container_add (CLUTTER_CONTAINER (tnote->notwin),
                         tnote->background, tnote->separator,
                         tnote->count, tnote->time, tnote->message,
                         NULL);

  return tnote;
}

/* Releases what was allocated by create_tnote() except the #ClutterActors,
 * which are taken care of by free_thumb(). */
static void
free_tnote (TNote * tnote)
{
  mb_wm_object_signal_disconnect (MB_WM_OBJECT (tnote->hdnote),
                                  tnote->hdnote_changed_cb_id);
  mb_wm_object_unref (MB_WM_OBJECT (tnote->hdnote));
  g_object_unref (tnote->notwin);
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
  if (hd_render_manager_get_state () != HDRM_STATE_TASK_NAV)
    /* Be consistent with appthumb_clicked(). */
    return TRUE;

  g_signal_emit_by_name (Navigator, "notification-clicked",
                         nothumb->tnote->hdnote);
  return TRUE;
}

/* Called thwn a notification %Thumbnail's close buttin is clicked. */
static gboolean
nothumb_close_clicked (Thumbnail * nothumb)
{
  g_assert (thumb_has_notification (nothumb));
  if (hd_render_manager_get_state () != HDRM_STATE_TASK_NAV)
    /* Be consistent with appthumb_clicked(). */
    return TRUE;
  g_signal_emit_by_name (Navigator, "notification-closed",
                         nothumb->tnote->hdnote);
  return TRUE;
}

/* Returns a %Thumbnail for @tnote and adds it to @Thumbnails. */
static Thumbnail *
add_nothumb (TNote * tnote)
{
  Thumbnail *nothumb;

  UnseenNotifications = TRUE;
  /* Create the %Thumbnail. */
  nothumb = g_new0 (Thumbnail, 1);
  nothumb->type = NOTIFICATION;
  nothumb->tnote = tnote;

  /* Reset @notwin's opacity, it might have belonged to an application,
   * which was zoomed in then closed. */
  create_thwin (nothumb, tnote->notwin);
  reset_thumb_title (nothumb);
  clutter_actor_set_opacity (tnote->notwin, 255);
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

/* If @li is in @Notifications removes the notification's thumbnail from
 * the @Grid and deletes @li from the list, but it doesn't free %TNote. */
static TNote *
remove_nothumb (GList * li, gboolean destroy_tnote)
{
  TNote *tnote;
  Thumbnail *nothumb;

  nothumb = li->data;
  g_assert (thumb_is_notification (nothumb));

  g_object_ref (nothumb->tnote->notwin);
  if (destroy_tnote)
    {
      /* Don't kill .notwin yet, but let free_thumb() do it. */
      free_tnote (nothumb->tnote);
      tnote = NULL;
    }
  else
    {
      /* The caller will want to use @tnote afterwards,
       * make sure free_thumb() doesn't kill .notwin. */
      clutter_container_remove_actor (CLUTTER_CONTAINER (nothumb->thwin),
                                      nothumb->tnote->notwin);
      tnote = nothumb->tnote;
    }
  nothumb->tnote = NULL;

  free_thumb (nothumb, destroy_tnote);
  if (li == Notifications)
    Notifications = li->next;
  Thumbnails = g_list_delete_link (Thumbnails, li);
  NThumbnails--;

  return tnote;
}
/* nothumb:s }}} */

/* Add/remove notifications {{{ */
/* Show a notification in the navigator, either in the @Grid
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
               * @Grid. */
              g_critical ("%s: attempt to add more than one notification",
                          __FUNCTION__);
              break;
            }

          /* Okay, found it. */
          adopt_notification (apthumb, tnote);
          layout_notwin (apthumb, NULL, NULL);
          return;
        }
    }

  layout (add_nothumb (tnote)->thwin, TRUE);

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
  for_each_thumbnail (li, thumb)
    if (thumb->tnote && thumb->tnote->hdnote == hdnote)
      break;
  if (!thumb || !thumb->tnote)
    /* This would be a bug somewhere, but who cares. */
    return;

  if (thumb_is_notification (thumb))
    { /* @hdinfo is displayed in a thumbnail on its own. */
      remove_nothumb (li, TRUE);
      layout (NULL, FALSE);

      /* Sync the Tasks button, we might have just become empty. */
      hd_render_manager_update ();
    }
  else /* @hdnote is in an application's title area. */
    free_tnote (orphan_notification (thumb, TRUE));

  /* Reset the highlighting if no more notifications left. */
  if (!hd_task_navigator_has_notifications ())
    hd_title_bar_set_switcher_pulse (
                      HD_TITLE_BAR (hd_render_manager_get_title_bar ()),
                      FALSE);
}
/* Add/remove notifications }}} */
/* }}} */

/* %HdTaskNavigator {{{ */
G_DEFINE_TYPE (HdTaskNavigator, hd_task_navigator, CLUTTER_TYPE_GROUP);

/* Private functions {{{ */
/*
 * Returns whether the @event coordinates fall within the tight
 * grid of thumbnails.  If @Thumbnails have an incomplete row at
 * the end the cells not occupied by thumbnails are not considered
 * part of the tight grid.
 */
static gboolean
within_grid (const ClutterButtonEvent * event)
{
  Layout lout;
  gint x, y, n, m;

  if (!NThumbnails)
    return FALSE;
  calc_layout (&lout);

  /* y := top of the first row */
  y = lout.ypos - hd_scrollable_group_get_viewport_y (Grid);
  if (event->y < y)
    /* Clicked above the first row. */
    return FALSE;

  /* y := the bottom of the last complete row */
  n  = NThumbnails / lout.cells_per_row;
  m  = NThumbnails % lout.cells_per_row;
  y += lout.vspace*(n-1) + lout.thumbsize->height;

  if (event->y <= y)
    { /* Clicked somewhere in the complete rows. */
      x = lout.xpos;
      n = lout.cells_per_row;
    }
  else if (m && event->y <= y + lout.vspace)
    { /* Clicked somewhere in the incomplete row. */
      x = lout.last_row_xpos;
      n = m;
    }
  else /* Clicked below the last row. */
    return FALSE;

  /* Clicked somehere in the last (either complete or incomplete) row. */
  g_assert (n > 0);
  if (event->x < x)
    return FALSE;
  if (event->x > x + lout.hspace*(n-1) + lout.thumbsize->width)
    return FALSE;

  /* Clicked between the thumbnails. */
  return TRUE;
}

/* Returns the widget that was clicked in @event.  It differentiates
 * between thumbnails, the @Grid (the thumbnailed area, effectively
 * the space between the thumbnails) and @Navigator (all the rest). */
static ClutterActor *
clicked_widget (const ClutterButtonEvent * event)
{
  if (event->source != CLUTTER_ACTOR (Navigator)
      && event->source != CLUTTER_ACTOR (Scroller)
      && event->source != CLUTTER_ACTOR (Grid))
    return event->source;
  else if (within_grid (event))
    return CLUTTER_ACTOR (Grid);
  else
    return CLUTTER_ACTOR (Navigator);
}
/* Private functions }}} */

/* Called when the transition showing the task navigator has ended */
void
hd_task_navigator_transition_done(HdTaskNavigator *self)
{
  /* Flash the scrollbar.  %TidyFingerScroll will do the right thing. */
  tidy_finger_scroll_show_scrollbars (Scroller);
}

/* Callbacks {{{ */
/* Entering and exiting @Navigator {{{ */
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

  /* Because we're just about to show them */
  UnseenNotifications = FALSE;
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
/* Entering and exiting @Navigator }}} */

/* Click-handling {{{
 * {{{
 * Is easy, but we need to consider a few circumstances:
 * a) The "grid" (the thumbnailed area) may be non-rectangular
 *    and it has its own behavior.  within_grid() help us in this.
 * b) We need to filter out clicks which are not clicks per UX Guidance.
 *    We have three criteria:
 *    1. the clicked widget (must be unambigous)
 *    2. click time (neither too fast or too slow)
 *    3. panning (differentiate between panning and clicking)
 *    The first two are guarded by a low level captured-event handler,
 *    which simply removes unwanted release-events.  The third is
 *    guarded by the captured-event handler of @Grid.
 *
 * Talking high-level our widgets have these behaviors:
 * -- close:      close thumbnails
 * -- thumbnail:  zoom in
 * -- grid:       do nothing
 * -- navigator:  exit
 * }}}
 */
/*
 * @Navigator::captured-event handler.  Its purpose is to filter out
 * non-clicks, ie. button-release-events which together with their
 * button-press-event counterpart didn't meet the click-critiera.
 *
 * In order for a press-release to be considered a click:
 * -- the button must be released above the pressed widget
 * -- the time between press and release must satisfy a static
 *    constraint.
 *
 * If these conditions remain unmet this handler prevents the propagation
 * of the button-release-event.  This does not interfere with scrolling
 * (%TidyFingerScroll grabs the pointer when it needs it) but would break
 * things if, for example, some widget wanted to have a highlighted state.
 */
static gboolean
navigator_touched (ClutterActor * navigator, ClutterEvent * event)
{
  static struct timeval last_press_time;
  static ClutterActor *pressed_widget;

  if (event->type == CLUTTER_BUTTON_PRESS)
    {
      gettimeofday (&last_press_time, NULL);
      pressed_widget = clicked_widget (&event->button);
    }
  else if (event->type == CLUTTER_BUTTON_RELEASE && pressed_widget)
    {
      struct timeval now;
      ClutterActor *widget;
      int dt;

      /* Don't interfere if we somehow get release events without
       * corresponding press events. */
      widget = pressed_widget;
      pressed_widget = NULL;

      /* Check press-release time. */
      gettimeofday (&now, NULL);
      if (now.tv_usec > last_press_time.tv_usec)
        dt = now.tv_usec-last_press_time.tv_usec
          + (now.tv_sec-last_press_time.tv_sec) * 1000000;
      else /* now.sec > last.sec */
        dt = (1000000-last_press_time.tv_usec)+now.tv_usec
          + (now.tv_sec-last_press_time.tv_sec-1) * 1000000;
      if (!(MIN_CLICK_TIME <= dt && dt <= MAX_CLICK_TIME))
        return TRUE;

      /* Verify that the pressed widget is the same as clicked_widget(). */
      if (widget != clicked_widget (&event->button))
        return TRUE;
    }

  return FALSE;
}

/* @Navigator::button-release-event handler.  Handles events propagated
 * by grid_clicked(). */
static gboolean
navigator_clicked (ClutterActor * navigator, ClutterButtonEvent * event)
{ /* Tell %HdSwitcher about it, which will hide us. */
  g_signal_emit_by_name (navigator, "background-clicked");
  return TRUE;
}

/* @Scroller::captured-event handler */
static gboolean
scroller_touched (ClutterActor * scroller, const ClutterEvent * event)
{ /* Don't start scrolling outside the grid. */
  if (event->type == CLUTTER_BUTTON_PRESS && !within_grid (&event->button))
    g_signal_stop_emission_by_name (scroller, "captured-event");
  return FALSE;
}

/* @Grid::captured-event handler */
static gboolean
grid_touched (ClutterActor * grid, ClutterEvent * event)
{ /* Don't propagate non-clicks to things within_grid(). */
  return   event->type == CLUTTER_BUTTON_RELEASE
        && within_grid (&event->button)
        && !hd_scrollable_group_is_clicked (HD_SCROLLABLE_GROUP (grid));
}

/* @Grid::button-release-event handler */
static gboolean
grid_clicked (ClutterActor * grid, ClutterButtonEvent * event)
{ /* Don't propagate the signal to @Navigator if it happened within_grid(). */
  return within_grid (event);
}
/* Click handling }}} */

/* @Grid's notify::has-clip callback to prevent clipping.
 * TODO Who clips and why? */
static gboolean
unclip (ClutterActor * actor, GParamSpec * prop, gpointer unused)
{
  /* Make sure not to recurse infinitely. */
  if (clutter_actor_has_clip (actor))
    clutter_actor_remove_clip (actor);
  return TRUE;
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
  clutter_actor_set_size (Navigator, SCREEN_WIDTH, SCREEN_HEIGHT);
  g_signal_connect (Navigator, "show", G_CALLBACK (navigator_shown),  NULL);
  g_signal_connect (Navigator, "hide", G_CALLBACK (navigator_hidden), NULL);
  g_signal_connect (Navigator, "captured-event",
                    G_CALLBACK (navigator_touched), NULL);
  g_signal_connect (Navigator, "button-release-event",
                    G_CALLBACK (navigator_clicked), NULL);

  /* Actor hierarchy */
  /* Turn off visibility detection for @Scroller to it won't be clipped by it. */
  Scroller = tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);
  clutter_actor_set_name (Scroller, "Scroller");
  clutter_actor_set_size (Scroller, SCREEN_WIDTH, SCREEN_HEIGHT);
  clutter_actor_set_visibility_detect(Scroller, FALSE);
  clutter_container_add_actor (CLUTTER_CONTAINER (Navigator), Scroller);
  g_signal_connect (Scroller, "captured-event",
                    G_CALLBACK (scroller_touched), NULL);

  /*
   * When we zoom in we may need to move the @Scroller up or downwards.
   * If we leave clipping on that becomes visible then, by cutting one
   * half of the zooming window.  Circumvent it by removing clipping
   * at the same time it is set.  TODO This can be considered a hack.
   */
  Grid = HD_SCROLLABLE_GROUP (hd_scrollable_group_new ());
  clutter_actor_set_name (CLUTTER_ACTOR (Grid), "Grid");
  clutter_actor_set_reactive (CLUTTER_ACTOR (Grid), TRUE);
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR (Grid), FALSE);
  g_signal_connect (Grid, "notify::has-clip", G_CALLBACK (unclip), NULL);
  g_signal_connect_after (Grid, "captured-event",
                          G_CALLBACK (grid_touched), NULL);
  g_signal_connect (Grid, "button-release-event",
                    G_CALLBACK (grid_clicked), NULL);
  clutter_container_add_actor (CLUTTER_CONTAINER (Scroller),
                               CLUTTER_ACTOR (Grid));

  /* Effect timelines */
  Fly_effect  = new_animation (&Fly_effect_timeline,  FLY_EFFECT_DURATION);
  Zoom_effect = new_animation (&Zoom_effect_timeline, ZOOM_EFFECT_DURATION);

  /* Master pieces */
  LargeSystemFont = hd_gtk_style_resolve_logical_font ("LargeSystemFont");
  SystemFont = hd_gtk_style_resolve_logical_font ("SystemFont");
  SmallSystemFont = hd_gtk_style_resolve_logical_font ("SmallSystemFont");
  hd_gtk_style_resolve_logical_color (&DefaultTextColor,
                                      "DefaultTextColor");
  hd_gtk_style_resolve_logical_color (&NotificationTextColor,
                                      "NotificationTextColor");
  hd_gtk_style_resolve_logical_color (&NotificationSecondaryTextColor,
                                      "NotificationSecondaryTextColor");

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
