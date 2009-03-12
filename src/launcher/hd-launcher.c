/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
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
#include "config.h"
#endif

#include <math.h>
#include <unistd.h>

#include "hd-launcher.h"
#include "hd-launcher-app.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <clutter/clutter.h>
#include <tidy/tidy-finger-scroll.h>
#include <tidy/tidy-blur-group.h>

#include "hildon-desktop.h"
#include "hd-launcher-page.h"
#include "hd-gtk-utils.h"
#include "hd-render-manager.h"
#include "hd-app-mgr.h"
#include "hd-gtk-style.h"
#include "hd-theme.h"
#include "hd-clutter-cache.h"

#define I_(str) (g_intern_static_string ((str)))

struct _HdLauncherPrivate
{
  ClutterActor *group;

  GData *pages;
  ClutterActor *active_page;
  ClutterActor *top_blur; /* blurring applied to top page when in sub page */

  /* Actor and timeline required for zoom in on application screenshot
   * for app start. */
  ClutterActor *launch_image;
  ClutterTimeline *launch_transition;
  ClutterVertex launch_position; /* where were we clicked? */

  HdLauncherTree *tree;
};

#define HD_LAUNCHER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                HD_TYPE_LAUNCHER, HdLauncherPrivate))

/* Signals */
enum
{
  APP_LAUNCHED,
  APP_RELAUNCHED,
  CAT_LAUNCHED,
  CAT_HIDDEN,
  HIDDEN,

  LAST_SIGNAL
};

static guint launcher_signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE (HdLauncher, hd_launcher, G_TYPE_OBJECT);

/* Forward declarations */
static void hd_launcher_constructed (GObject *gobject);
static void hd_launcher_dispose (GObject *gobject);
static void hd_launcher_category_tile_clicked (HdLauncherTile *tile,
                                               gpointer data);
static void hd_launcher_application_tile_clicked (HdLauncherTile *tile,
                                                  gpointer data);
static void hd_launcher_populate_tree_finished (HdLauncherTree *tree,
                                                gpointer data);
static gboolean hd_launcher_transition_app_start (HdLauncherTile *tile,
                                                  HdLauncherApp *item);
static void hd_launcher_transition_new_frame(ClutterTimeline *timeline,
                                             gint frame_num, gpointer data);

/* We cannot #include "hd-transition.h" because it #include:s mb-wm.h,
 * which wants to #define _GNU_SOURCE unconditionally, but we already
 * have it in -D and they clash.  XXX */
extern void hd_transition_play_sound(const gchar *fname);

/* The HdLauncher singleton */
static HdLauncher *the_launcher = NULL;

HdLauncher *
hd_launcher_get (void)
{
  if (G_UNLIKELY (!the_launcher))
    the_launcher = g_object_new (HD_TYPE_LAUNCHER, NULL);
  return the_launcher;
}

static void
hd_launcher_class_init (HdLauncherClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherPrivate));

  gobject_class->dispose     = hd_launcher_dispose;
  gobject_class->constructed = hd_launcher_constructed;

  launcher_signals[APP_LAUNCHED] =
    g_signal_new (I_("application-launched"),
                  HD_TYPE_LAUNCHER,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, HD_TYPE_LAUNCHER_APP);
  launcher_signals[APP_RELAUNCHED] =
    g_signal_new (I_("application-relaunched"),
                  HD_TYPE_LAUNCHER,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, HD_TYPE_LAUNCHER_APP);
  launcher_signals[CAT_LAUNCHED] =
    g_signal_new (I_("category-launched"),
                  HD_TYPE_LAUNCHER,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0, NULL);
  launcher_signals[CAT_HIDDEN] =
    g_signal_new (I_("category-hidden"),
                  HD_TYPE_LAUNCHER,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0, NULL);
  launcher_signals[HIDDEN] =
    g_signal_new (I_("launcher-hidden"),
                  HD_TYPE_LAUNCHER,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0, NULL);
}

static void
hd_launcher_init (HdLauncher *self)
{
  HdLauncherPrivate *priv;

  self->priv = priv = HD_LAUNCHER_GET_PRIVATE (self);
  g_datalist_init (&priv->pages);
}

static void hd_launcher_constructed (GObject *gobject)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (gobject);

  priv->tree = hd_launcher_tree_new (NULL);
  g_signal_connect (priv->tree, "finished",
                    G_CALLBACK (hd_launcher_populate_tree_finished),
                    NULL);

  priv->group = clutter_group_new ();
  clutter_actor_hide(priv->group);
  clutter_actor_set_size (priv->group, HD_LAUNCHER_PAGE_WIDTH,
                                       HD_LAUNCHER_PAGE_WIDTH);

  /* Blurring for the top-level launcher */
  priv->top_blur = tidy_blur_group_new();
  clutter_actor_show(priv->top_blur);
  tidy_blur_group_set_use_alpha(priv->top_blur, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->group),
                               priv->top_blur);

  ClutterActor *top_page = hd_launcher_page_new (NULL, NULL);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->top_blur),
                               top_page);
  clutter_actor_hide (top_page);
  priv->active_page = NULL;
  g_datalist_set_data (&priv->pages, HD_LAUNCHER_ITEM_TOP_CATEGORY, top_page);

  /* App launch transition */
  priv->launch_image = 0;
  priv->launch_transition = g_object_ref (
                                clutter_timeline_new_for_duration (400));
  g_signal_connect (priv->launch_transition, "new-frame",
                    G_CALLBACK (hd_launcher_transition_new_frame), gobject);
  priv->launch_position.x = CLUTTER_INT_TO_FIXED(HD_LAUNCHER_PAGE_WIDTH) / 2;
  priv->launch_position.y = CLUTTER_INT_TO_FIXED(HD_LAUNCHER_PAGE_HEIGHT) / 2;
  priv->launch_position.z = 0;

  if (!hd_disable_threads())
    hd_launcher_tree_populate (priv->tree);
}

static void
hd_launcher_dispose (GObject *gobject)
{
  HdLauncher *self = HD_LAUNCHER (gobject);
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (self);

  if (priv->top_blur)
    {
      clutter_actor_destroy (priv->top_blur);
      priv->top_blur = NULL;
    }

  if (priv->group)
    {
      clutter_actor_destroy (priv->group);
      priv->group = NULL;
    }

  g_datalist_clear (&priv->pages);

  G_OBJECT_CLASS (hd_launcher_parent_class)->dispose (gobject);
}

void
hd_launcher_show (void)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());

  ClutterActor *top_page = g_datalist_get_data (&priv->pages,
                                                HD_LAUNCHER_ITEM_TOP_CATEGORY);
  priv->active_page = top_page;
  clutter_actor_show (priv->group);
  hd_launcher_page_transition(HD_LAUNCHER_PAGE(priv->active_page),
        HD_LAUNCHER_PAGE_TRANSITION_IN);
}

void
hd_launcher_hide (void)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());

  if (priv->active_page)
    {
      ClutterActor *top_page = g_datalist_get_data (&priv->pages,
                                    HD_LAUNCHER_ITEM_TOP_CATEGORY);
      /* if we're not at the top page, we must transition that out too */
      if (priv->active_page != top_page)
        {
          hd_launcher_page_transition(HD_LAUNCHER_PAGE(top_page),
              HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK);
        }

      hd_launcher_page_transition(HD_LAUNCHER_PAGE(priv->active_page),
          HD_LAUNCHER_PAGE_TRANSITION_OUT);
      priv->active_page = NULL;
    }
}

/* hide the launcher fully. Called from hd-launcher-page
 * after a transition has finished */
void
hd_launcher_hide_final (void)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());

  clutter_actor_hide (priv->group);
}


gboolean
hd_launcher_back_button_clicked (ClutterActor *actor,
                                 ClutterEvent *event,
                                 gpointer data)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());
  ClutterActor *top_page = g_datalist_get_data (&priv->pages,
                                                HD_LAUNCHER_ITEM_TOP_CATEGORY);

  if (hd_render_manager_get_state() != HDRM_STATE_LAUNCHER)
    return FALSE;

  if (priv->active_page == top_page)
    g_signal_emit (data, launcher_signals[HIDDEN], 0);
  else
    {
      hd_launcher_page_transition(HD_LAUNCHER_PAGE(priv->active_page),
        HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB);
      hd_launcher_page_transition(HD_LAUNCHER_PAGE(top_page),
        HD_LAUNCHER_PAGE_TRANSITION_FORWARD);
      priv->active_page = top_page;
      g_signal_emit (hd_launcher_get (), launcher_signals[CAT_HIDDEN],
                     0, NULL);
    }

  return FALSE;
}

ClutterActor *hd_launcher_get_group (void)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());

  return priv->group;
}

/* sets blur amount for transitions involving blurring out the top view */
void
hd_launcher_set_top_blur (float amount, float opacity)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());

  if (amount<0) amount=0;
  if (amount>1) amount=1;

  tidy_blur_group_set_blur(priv->top_blur, amount*5.0f);
  tidy_blur_group_set_saturation(priv->top_blur, 1.0f - amount*0.7f);
  tidy_blur_group_set_brightness(priv->top_blur, 1.0f - amount*0.7f);
  tidy_blur_group_set_zoom(priv->top_blur,
                        (15.0f + cos(amount*3.141592f)) / 16);

  clutter_actor_set_opacity(priv->top_blur, (int)(255*opacity));
}

static void
hd_launcher_category_tile_clicked (HdLauncherTile *tile, gpointer data)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());
  ClutterActor *page = CLUTTER_ACTOR (data);

  hd_launcher_page_transition(HD_LAUNCHER_PAGE(priv->active_page),
        HD_LAUNCHER_PAGE_TRANSITION_BACK);
  hd_launcher_page_transition(HD_LAUNCHER_PAGE(page),
        HD_LAUNCHER_PAGE_TRANSITION_IN_SUB);
  priv->active_page = page;
  g_signal_emit (hd_launcher_get (), launcher_signals[CAT_LAUNCHED],
                 0, NULL);
}

static void
hd_launcher_application_tile_clicked (HdLauncherTile *tile,
                                      gpointer data)
{
  HdLauncher *launcher = hd_launcher_get();
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (launcher);
  HdLauncherApp *app = HD_LAUNCHER_APP (data);
  ClutterActor *top_page;

  /* If the app has been already launched, send the relaunched signal
   * and don't animate anything.
   */
  if (hd_launcher_app_get_state (app) == HD_APP_STATE_SHOWN)
    {
      g_signal_emit (hd_launcher_get (), launcher_signals[APP_RELAUNCHED],
                     0, data, NULL);
    }
  /* If there's no launch transition, do the 'fall away' transition.
   */
  else if (!hd_launcher_transition_app_start (tile, app))
    {
      hd_launcher_page_transition(HD_LAUNCHER_PAGE(priv->active_page),
            HD_LAUNCHER_PAGE_TRANSITION_LAUNCH);
      /* also do animation for the topmost pane if we had it... */
      top_page = g_datalist_get_data (&priv->pages,
                                       HD_LAUNCHER_ITEM_TOP_CATEGORY);
      /* if we're not at the top page, we must transition that out too */
      if (priv->active_page != top_page)
        {
          hd_launcher_page_transition(HD_LAUNCHER_PAGE(top_page),
                    HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK);
        }
    }

  /* then launch */
  g_signal_emit (hd_launcher_get (), launcher_signals[APP_LAUNCHED],
                 0, data, NULL);
  hd_app_mgr_launch (app);
}

/*
 * Creating the pages and tiles
 */

typedef struct
{
  GList *items;
} HdLauncherTraverseData;

static void
hd_launcher_create_page (HdLauncherItem *item, gpointer data)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());
  ClutterActor *newpage;

  if (hd_launcher_item_get_item_type (item) != HD_CATEGORY_LAUNCHER)
    return;

  newpage = hd_launcher_page_new(
                   hd_launcher_item_get_icon_name (item),
                   hd_launcher_item_get_local_name (item));

  clutter_actor_hide (newpage);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->group), newpage);

  g_datalist_set_data (&priv->pages, hd_launcher_item_get_id (item), newpage);
}

static gboolean
hd_launcher_lazy_traverse_tree (gpointer data)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());
  HdLauncherTraverseData *tdata = data;
  HdLauncherItem *item;
  HdLauncherTile *tile;
  HdLauncherPage *page;

  if (!tdata->items)
    return FALSE;
  item = tdata->items->data;

  tile = hd_launcher_tile_new (
      hd_launcher_item_get_icon_name (item),
      hd_launcher_item_get_local_name (item));

  /* Find in which page it goes */
  page = g_datalist_get_data (&priv->pages,
                              hd_launcher_item_get_category (item));
  if (!page)
    /* Put it in the default level. */
    page = g_datalist_get_data (&priv->pages, HD_LAUNCHER_ITEM_DEFAULT_CATEGORY);

  hd_launcher_page_add_tile (page, tile);

  if (hd_launcher_item_get_item_type(item) == HD_CATEGORY_LAUNCHER)
    {
      g_signal_connect (tile, "clicked",
                        G_CALLBACK (hd_launcher_category_tile_clicked),
                        g_datalist_get_data (&priv->pages,
                          hd_launcher_item_get_id (item)));
    }
  else if (hd_launcher_item_get_item_type(item) == HD_APPLICATION_LAUNCHER)
    {
      g_signal_connect (tile, "clicked",
                        G_CALLBACK (hd_launcher_application_tile_clicked),
                        item);
    }

  if (tdata->items->next)
    {
      tdata->items = tdata->items->next;
      return TRUE;
    }

  return FALSE;
}

static void
hd_launcher_lazy_traverse_cleanup (gpointer data)
{
  g_free (data);
}

static void
hd_launcher_populate_tree_finished (HdLauncherTree *tree, gpointer data)
{
  HdLauncherTraverseData *tdata = g_new0 (HdLauncherTraverseData, 1);
  tdata->items = hd_launcher_tree_get_items(tree, NULL);

  /* First we traverse the list and create all the categories,
   * so that apps can be correctly put into them.
   */
  g_list_foreach (tdata->items, (GFunc) hd_launcher_create_page, NULL);

  /* Then we add the tiles to them in a idle callback. */
  clutter_threads_add_idle_full (CLUTTER_PRIORITY_REDRAW + 20,
                                 hd_launcher_lazy_traverse_tree,
                                 tdata,
                                 hd_launcher_lazy_traverse_cleanup);
}

/* handle clicks to the fake launch image. If we've been up this long the
   app may have died and we just want to remove ourselves. */
static gboolean
_hd_launcher_transition_clicked(ClutterActor *actor,
                                ClutterEvent *event,
                                gpointer user_data)
{
  hd_launcher_window_created();
  /* check to see if we had any apps, because we may want to change state... */
  if (hd_render_manager_has_apps())
    hd_render_manager_set_state(HDRM_STATE_TASK_NAV);
  else
    hd_render_manager_set_state(HDRM_STATE_HOME);
  /* we don't want any animation this time as we want it to
   * be instant. */
  hd_render_manager_stop_transition();
  /* redraw the stage so it is immediately removed */
  clutter_actor_queue_redraw( clutter_stage_get_default() );
  return TRUE;
}

/* Does the transition for the application launch */
static gboolean
hd_launcher_transition_app_start (HdLauncherTile *tile, HdLauncherApp *item)
{
  const gchar *loading_image = NULL;
  HdLauncher *launcher = hd_launcher_get();
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (launcher);
  gboolean launch_anim = FALSE;
  const gchar *service_name;
  gchar *cached_image = NULL;
  ClutterActor *app_image = 0;
  ClutterActor *tb_image = 0;
  gint title_height;

  /* Is there a cached image? */
  service_name = hd_launcher_app_get_service (item);
  if (service_name &&
      index(service_name, '/')==NULL &&
      service_name[0]!='.')
    {
      cached_image = g_strdup_printf("%s/.cache/launch/%s.png",
				     getenv("HOME"),
				     service_name);

      if (access (cached_image, R_OK)==0)
	loading_image = cached_image;
    }

  /* If not, does the .desktop file specify an image? */
  if (!loading_image)
    loading_image = hd_launcher_app_get_loading_image( item );

  if (loading_image && !strlen(loading_image))
    loading_image = 0;

  if (priv->launch_image)
    clutter_actor_destroy(priv->launch_image);

  /* Load the launch image and add to the stage, along with the title bar
   * from the theme (in their own group) */
  priv->launch_image = clutter_group_new();
  clutter_actor_set_name(priv->launch_image,
      "hd_launcher_transition_app_start");
  clutter_actor_set_size(priv->launch_image,
                         HD_COMP_MGR_SCREEN_WIDTH,
                         HD_COMP_MGR_SCREEN_HEIGHT);
  /* Title bar */
  tb_image = hd_clutter_cache_get_texture(
      HD_THEME_IMG_TITLE_BAR, TRUE);
  clutter_actor_set_width(tb_image, HD_COMP_MGR_SCREEN_WIDTH);
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->launch_image), tb_image);
  /* App image - if we had one */
  if (loading_image)
    {
      app_image = clutter_texture_new_from_file(loading_image, 0);
      if (!app_image)
        g_warning("%s: Preload image file '%s' specified for '%s'"
                    " couldn't be loaded",
                  __FUNCTION__, loading_image, hd_launcher_app_get_exec(item));
    }
  /* if not, create a rectangle with the background colour from the theme */
  if (!app_image)
    {
      ClutterColor col;
      hd_gtk_style_get_bg_color(HD_GTK_BUTTON_SINGLETON,
                                GTK_STATE_NORMAL,
                                &col);
      app_image = clutter_rectangle_new_with_color(&col);
    }

  title_height = clutter_actor_get_height(tb_image);
  clutter_actor_set_size(app_image,
                         HD_COMP_MGR_SCREEN_WIDTH,
                         HD_COMP_MGR_SCREEN_HEIGHT-title_height);
  clutter_actor_set_position(app_image,
                             0, title_height);
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->launch_image), app_image);


  ClutterActor *icon;
  ClutterContainer *parent = hd_render_manager_get_front_group();

  /* default pos to centre of the screen */
  priv->launch_position.x = CLUTTER_INT_TO_FIXED(HD_LAUNCHER_PAGE_WIDTH) / 2;
  priv->launch_position.y = CLUTTER_INT_TO_FIXED(HD_LAUNCHER_PAGE_HEIGHT) / 2;
  /* work out where to expand the image from from the centre of the icon of
   * the tile that was clicked on */
  clutter_actor_get_positionu(CLUTTER_ACTOR(tile),
            &priv->launch_position.x,
            &priv->launch_position.y);
  icon = hd_launcher_tile_get_icon(tile);
  if (icon)
    {
      ClutterVertex offs, size;
      clutter_actor_get_positionu(icon,
            &offs.x,
            &offs.y);
      clutter_actor_get_sizeu(icon, &size.x, &size.y);
      priv->launch_position.x += offs.x + size.x/2;
      priv->launch_position.y += offs.y + size.y/2;
    }
  /* append scroller movement */
  priv->launch_position.y += CLUTTER_INT_TO_FIXED(HD_LAUNCHER_PAGE_YMARGIN);
  if (priv->active_page)
    priv->launch_position.y -=
        hd_launcher_page_get_scroll_y(HD_LAUNCHER_PAGE(priv->active_page));
  /* all because the tidy- stuff breaks clutter's nice 'get absolute position'
   * code... */

  clutter_actor_set_name(priv->launch_image,
                         "HdLauncher:launch_image");
  clutter_container_add_actor(parent,
                              priv->launch_image);
  clutter_actor_set_reactive ( priv->launch_image, TRUE );
  g_signal_connect (priv->launch_image, "button-release-event",
                    G_CALLBACK(_hd_launcher_transition_clicked), 0);

  /* Run the first step of the transition so we don't get flicker before
   * the timeline is called */
  hd_launcher_transition_new_frame(priv->launch_transition,
                                   0, launcher);
  clutter_actor_show(priv->launch_image);

  clutter_timeline_rewind(priv->launch_transition);
  clutter_timeline_start(priv->launch_transition);

  launch_anim = TRUE;

  hd_transition_play_sound ("/usr/share/sounds/ui-window_open.wav");
  g_free (cached_image);

  return launch_anim;
}

/* When a window has been created we want to be sure we've removed our
 * screenshot. Either that or we smoothly fade it out... maybe? :) */
void hd_launcher_window_created(void)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());

  if (priv->launch_image)
  {
    clutter_timeline_stop(priv->launch_transition);
    clutter_actor_destroy(priv->launch_image);
    priv->launch_image = 0;
  }
}

static void
hd_launcher_transition_new_frame(ClutterTimeline *timeline,
                          gint frame_num, gpointer data)
{
  HdLauncher *page = HD_LAUNCHER(data);
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (page);
  gint frames;
  float amt;
  ClutterFixed mx,my,zx,zy;
  guint width, height;

  if (!HD_IS_LAUNCHER(data))
    return;

  frames = clutter_timeline_get_n_frames(timeline);
  amt = frame_num / (float)frames;

  amt = (1 - cos(amt * 3.141592)) * 0.5f;

  if (!priv->launch_image)
    return;

  clutter_actor_get_size(priv->launch_image, &width, &height);
  /* mid-position of actor */
  mx = CLUTTER_FLOAT_TO_FIXED(
                width*0.5f*amt +
                CLUTTER_FIXED_TO_FLOAT(priv->launch_position.x)*(1-amt));
  my = CLUTTER_FLOAT_TO_FIXED(
                height*0.5f*amt +
                CLUTTER_FIXED_TO_FLOAT(priv->launch_position.y)*(1-amt));
  /* size of actor */
  zx = CLUTTER_FLOAT_TO_FIXED(HD_LAUNCHER_PAGE_WIDTH*amt*0.5f);
  zy = CLUTTER_FLOAT_TO_FIXED(HD_LAUNCHER_PAGE_HEIGHT*amt*0.5f);

  clutter_actor_set_positionu(priv->launch_image, mx-zx, my-zy);
  clutter_actor_set_scale(priv->launch_image, amt, amt);
}

HdLauncherTree *
hd_launcher_get_tree (void)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());
  return priv->tree;
}

static void
_hd_launcher_transition_stop_foreach(GQuark         key_id,
                                     gpointer       data,
                                     gpointer       user_data)
{
  hd_launcher_page_transition_stop(HD_LAUNCHER_PAGE(data));
}

/* Stop any currently active transitions */
void
hd_launcher_transition_stop(void)
{
  HdLauncherPrivate *priv = HD_LAUNCHER_GET_PRIVATE (hd_launcher_get ());

  g_datalist_foreach(&priv->pages,
                     _hd_launcher_transition_stop_foreach,
                     (gpointer)0);
}
