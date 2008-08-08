#include "hd-launcher-utils.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n.h>

#include <clutter/clutter.h>
#include <tidy/tidy-finger-scroll.h>

#include "hd-task-launcher.h"
#include "hd-dummy-launcher.h"
#include "hd-launcher-utils.h"
#include "hd-launcher-tree.h"

#define DEFAULT_APPS_DIR        "/usr/share/applications/"

#define ICON_FAVOURITES         "qgn_list_gene_favor"
#define ICON_FOLDER             "qgn_list_filesys_common_fldr"
/*#define ICON_FOLDER           "qgn_list_gene_fldr_cls"*/
#define ICON_DEFAULT_APP        "qgn_list_gene_default_app"
#define ICON_SIZE               26
/* Apparently 64 is what we get for the "scalable" size. Should really be -1.*/
#define ICON_THUMB_SIZE         64

#if 0
GList *
hd_get_launcher_items (const gchar *directory)
{
  GList *retval = NULL;
  GDir *dir;
  GError *error;

  if (!directory || *directory == '\0')
    directory = DEFAULT_APPS_DIR;

  error = NULL;
  dir = g_dir_open (directory, 0, &error);
  if (!dir)
    {
      g_warning ("Unable to enumerate `%s'", directory);
      return NULL;
    }

  file = g_dir_read_name (dir);
  while (file != NULL)
    {
      gchar *full_path;
      struct stat stat_buf;

      full_path = g_build_filename (directory, file, NULL);

      if (g_stat (full_path, &stat_buf) < 0)
        {
          g_warning ("Unable to check `%s': %s",
                     full_path,
                     g_strerror (errno));
          goto next;
        }

      if (S_ISDIR (stat_buf.st_mode))
        {
          GList *res;

          /* recurse into subdirectories */
          res = hd_build_launcher_items (full_path);
          if (res)
            retval = g_list_concat (retval, res);
        }
      else if (S_ISREG (stat_buf.st_mode) &&
               g_str_has_suffix (full_path, ".desktop"))
        {
          HdLauncherItem *item;

          item = hd_app_launcher_new_from_file (full_path, &error);
          if (error)
            {
              g_warning ("Unable to load `%s': %s",
                         full_path,
                         error->message);

              g_clear_error (&error);
              if (item)
                g_object_unref (item);

              goto next;
            }

          if (!hd_app_launcher_get_item_type (HD_APP_LAUNCHER (item)))
            {
              g_warning ("Invalid desktop file `%s'", full_path);

              g_object_unref (item);
            }

          retval = g_list_prepend (retval, item);
        }
      else
        g_warning ("Invalid file `%s' found", full_path);

next:
      g_free (full_path);

      file = g_dir_read_name (dir);
    }

  g_dir_close (dir);

  return g_list_reverse (retval);
}

static void
build_node (const gchar *filename,
            GNode       *parent,
            xmlDocPtr    doc,
            xmlNodePtr   root,
            GList       *launchers)
{
  xmlNodePtr cur;
  gboolean first_level = FALSE;

  if (!doc)
    {
      doc = xmlReadFile (filename, NULL, 0);
      if (!doc)
        {
          g_warning ("Unable to read: `%s'", filename);
          return;
        }

      first_level = NULL;
    }

  if (!root)
    {
      root = xmlDocGetRootElement (doc);
      if (!root)
        {
          g_warning ("Unbale to retrieve the root element of `%s'",
                     filename);
          xmlFreeDoc (doc);
          return;
        }
    }

  if (xmlStrcmp (root->name, (const xmlChar *) "Menu") != 0)
    {
      g_warning ("The file `%s' is not a menu definition", filename);
      xmlFreeDoc (doc);
      return;
    }

  if (!parent)
    parent = g_node_new (NULL);

#define IS_NODE(node,name)      (0 == strcmp ((const char *) (node)->name, (name)))

  for (cur = root->xmlChildrenNode;
       cur != NULL;
       cur = cur->next)
    {
      if (IS_NODE (cur, "Menu"))
        build_node (filename, parent, doc, root, launchers);
      else if (IS_NODE (cur, "Name"))
        {
          xmlChar *key;

          key = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);

          xmlFree (key);
        }
      else if (IS_NODE (cur, "All"))
        {
        }
      else if (IS_NODE (cur, "MergeFile"))
        {
        }
      else if (IS_NODE (cur, "Include"))
        {
        }
      else if (IS_NODE (cur, "Separator"))
        {
        }
    }
}

#endif

static HdLauncherTree *
hd_build_launcher_items (void)
{
  HdLauncherTree *retval;

  retval = hd_launcher_tree_new (NULL); /* default */

  return retval;
}

typedef struct
{
  ClutterTimeline *timeline;

  ClutterEffectTemplate *tmpl;

  ClutterBehaviour *fade_behaviour;
  ClutterBehaviour *zoom_behaviour;
  
  ClutterActor *group;

  ClutterActor *top_scroll;
  ClutterActor *sub_scroll;

  ClutterActor *top_level;
  ClutterActor *sub_level;

  HdLauncherTree *tree;
} HdLauncher;

static HdLauncher *hd_launcher = NULL;

typedef struct
{
  HdTaskLauncher *launcher;
  GList *items;
  guint n_items;
  guint current_pos;
} PopulateClosure;

static gboolean
populate_top_launcher (gpointer data)
{
  PopulateClosure *closure = data;
  HdLauncherItem *item;

  item = g_list_nth_data (closure->items, closure->current_pos);
  hd_task_launcher_add_item (closure->launcher, item);

  closure->current_pos += 1;

  if (closure->current_pos == closure->n_items)
    return FALSE;

  return TRUE;
}

static gboolean
populate_sub_launcher (gpointer data)
{
  PopulateClosure *closure = data;
  HdLauncherItem *item;

  item = hd_dummy_launcher_new (HD_APPLICATION_LAUNCHER);
  hd_task_launcher_add_item (closure->launcher, item);

  closure->current_pos += 1;

  if (closure->current_pos == closure->n_items)
    return FALSE;

  return TRUE;
}

static void
populate_launcher_cleanup (gpointer data)
{
  PopulateClosure *closure = data;

  g_object_unref (closure->launcher);
  g_free (closure);
}

static void
lazily_populate_top_launcher (HdTaskLauncher *launcher,
                              GList          *items)
{
  PopulateClosure *closure;

  closure              = g_new0 (PopulateClosure, 1);
  closure->launcher    = g_object_ref (launcher);
  closure->items       = items;
  closure->n_items     = g_list_length (items);
  closure->current_pos = 0;

  clutter_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE + 50,
                                 populate_top_launcher,
                                 closure,
                                 populate_launcher_cleanup);
}

static void
lazily_populate_sub_launcher (HdTaskLauncher *launcher,
                              gint            n_items)
{
  PopulateClosure *closure;

  closure = g_new0 (PopulateClosure, 1);
  closure->launcher = g_object_ref (launcher);
  closure->n_items = n_items > 0
    ? n_items
    : g_random_int_range (1, (n_items * -1));
  closure->current_pos = 0;

  clutter_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE + 50,
                                 populate_sub_launcher,
                                 closure,
                                 populate_launcher_cleanup);
}

static void
top_level_item_clicked (HdTaskLauncher *launcher,
                        HdLauncherItem *item,
                        HdLauncher     *data)
{
  g_print ("%s: item `%s' (type: %s) clicked\n",
           G_STRLOC,
           hd_launcher_item_get_text (item),
           hd_launcher_item_get_item_type (item) == HD_APPLICATION_LAUNCHER
             ? "application"
             : "group");

  if (hd_launcher_item_get_item_type (item) == HD_SECTION_LAUNCHER)
    {
      lazily_populate_sub_launcher (HD_TASK_LAUNCHER (data->sub_level), -12);

      clutter_timeline_set_direction (data->timeline, CLUTTER_TIMELINE_FORWARD);
      clutter_timeline_rewind (data->timeline);
      clutter_timeline_start (data->timeline);

      clutter_actor_set_reactive (data->sub_scroll, TRUE);
      clutter_actor_show (data->sub_scroll);

      clutter_actor_set_reactive (data->top_scroll, FALSE);
      clutter_effect_fade (data->tmpl, data->top_scroll, 0, NULL, NULL);
      clutter_effect_depth (data->tmpl, data->top_scroll, -100, NULL, NULL);
    }
  else
    {
      /* no-op */
    }
}

static void
sub_level_item_clicked (HdTaskLauncher *launcher,
                        HdLauncherItem *item,
                        HdLauncher     *data)
{
  hd_task_launcher_clear (HD_TASK_LAUNCHER (data->sub_level));

  clutter_timeline_set_direction (data->timeline, CLUTTER_TIMELINE_BACKWARD);
  clutter_timeline_rewind (data->timeline);
  clutter_timeline_start (data->timeline);

  clutter_actor_set_reactive (data->sub_scroll, FALSE);
  clutter_actor_hide (data->sub_scroll);

  clutter_actor_set_reactive (data->top_scroll, TRUE);
  clutter_effect_fade (data->tmpl, data->top_scroll, 255, NULL, NULL);
  clutter_effect_depth (data->tmpl, data->top_scroll, 0, NULL, NULL);
}

static void
launcher_tree_finished (HdLauncherTree *tree,
                        gpointer        user_data)
{
  lazily_populate_top_launcher (HD_TASK_LAUNCHER (hd_launcher->top_level),
                                hd_launcher_tree_get_items (tree, NULL));
}

static gboolean
populate_tree (gpointer data)
{
  hd_launcher_tree_populate (hd_launcher->tree);

  return FALSE;
}

ClutterActor *
hd_get_application_launcher (void)
{
  if (G_UNLIKELY (hd_launcher == NULL))
    {
      hd_launcher = g_new0 (HdLauncher, 1);

      hd_launcher->tree = hd_build_launcher_items ();
      g_signal_connect (hd_launcher->tree, "finished",
                        G_CALLBACK (launcher_tree_finished),
                        NULL);

      hd_launcher->timeline = clutter_timeline_new_for_duration (250);
      hd_launcher->tmpl =
        clutter_effect_template_new_for_duration (250, clutter_sine_inc_func);

      hd_launcher->fade_behaviour =
        clutter_behaviour_opacity_new (clutter_alpha_new_full (hd_launcher->timeline,
                                                               clutter_sine_inc_func,
                                                               NULL, NULL),
                                       0, 255);
      hd_launcher->zoom_behaviour =
        clutter_behaviour_depth_new (clutter_alpha_new_full (hd_launcher->timeline,
                                                             clutter_ramp_inc_func,
                                                             NULL, NULL),
                                     200, 0);

      hd_launcher->group = clutter_group_new ();

      hd_launcher->top_scroll = tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);
      clutter_actor_set_position (hd_launcher->top_scroll, 0, 64);
      clutter_actor_set_size (hd_launcher->top_scroll, 760, 400);
      clutter_container_add_actor (CLUTTER_CONTAINER (hd_launcher->group),
                                   hd_launcher->top_scroll);

      hd_launcher->sub_scroll = tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);
      clutter_actor_set_position (hd_launcher->sub_scroll, 0, 64);
      clutter_actor_set_size (hd_launcher->sub_scroll, 760, 400);
      clutter_actor_hide (hd_launcher->sub_scroll);
      clutter_container_add_actor (CLUTTER_CONTAINER (hd_launcher->group),
                                   hd_launcher->sub_scroll);

      /* top level launcher */
      hd_launcher->top_level = hd_task_launcher_new ();
      clutter_actor_set_width (hd_launcher->top_level, 760);
      clutter_container_add_actor (CLUTTER_CONTAINER (hd_launcher->top_scroll),
                                   hd_launcher->top_level);
      g_signal_connect (hd_launcher->top_level,
                        "item-clicked", G_CALLBACK (top_level_item_clicked),
                        hd_launcher);

      /* secondary level launcher */
      hd_launcher->sub_level = hd_task_launcher_new ();
      clutter_actor_set_width (hd_launcher->sub_level, 760);
      clutter_container_add_actor (CLUTTER_CONTAINER (hd_launcher->sub_scroll),
                                   hd_launcher->sub_level);
      g_signal_connect (hd_launcher->sub_level,
                        "item-clicked", G_CALLBACK (sub_level_item_clicked),
                        hd_launcher);

      clutter_behaviour_apply (hd_launcher->zoom_behaviour,
                               hd_launcher->sub_level);
      clutter_behaviour_apply (hd_launcher->fade_behaviour,
                               hd_launcher->sub_level);

      clutter_threads_add_idle (populate_tree, NULL);

      return hd_launcher->group;
    }
  else
    return hd_launcher->group;
}
