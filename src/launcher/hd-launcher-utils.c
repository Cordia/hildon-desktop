#include "hd-launcher-utils.h"
#include "hd-app-launcher.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n.h>

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#define DEFAULT_APPS_DIR        "/usr/share/applications/"

#define ICON_FAVOURITES         "qgn_list_gene_favor"
#define ICON_FOLDER             "qgn_list_filesys_common_fldr"
/*#define ICON_FOLDER           "qgn_list_gene_fldr_cls"*/
#define ICON_DEFAULT_APP        "qgn_list_gene_default_app"
#define ICON_SIZE               26
/* Apparently 64 is what we get for the "scalable" size. Should really be -1.*/
#define ICON_THUMB_SIZE         64

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

GNode *
hd_build_launcher_items (void)
{
  return NULL;
}

GList *
hd_get_top_level_items (GNode *root)
{
  return NULL;
}
