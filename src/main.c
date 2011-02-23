/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008-2009 Nokia Corporation.
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

#include <time.h>
#include <signal.h>
#include <sys/time.h>

#include <matchbox/mb-wm-config.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <hildon/hildon-main.h>
#include <clutter/clutter-main.h>
#include <clutter/clutter-stage.h>
#include <clutter/x11/clutter-x11.h>
#include <clutter/clutter-container.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>

#include "hildon-desktop.h"
#include "hd-wm.h"
#include "hd-theme.h"
#include "hd-util.h"
#include "hd-dbus.h"
#include "hd-volume-profile.h"
#include "launcher/hd-app-mgr.h"
#include "home/hd-render-manager.h"
#include "hd-transition.h"

#ifndef DISABLE_A11Y
#include "hildon-desktop-a11y.h"
#endif

enum {
  KEY_ACTION_TOGGLE_SWITCHER = 1,
  KEY_ACTION_TOGGLE_NON_COMP_MODE,
  KEY_ACTION_TAKE_SCREENSHOT,
  KEY_ACTION_XTERMINAL,
  KEY_ACTION_TOGGLE_PORTRAITABLE,
  KEY_ACTION_ROTATE,
  KEY_ACTION_SEND_DBUS,
};

#ifdef MBWM_DEB_VERSION
asm(".section .rodata");
asm(".string \"built with libmatchbox2 "MBWM_DEB_VERSION"\"");
asm(".previous");
#endif

gboolean hd_debug_mode_set = FALSE;
MBWindowManager *hd_mb_wm = NULL;
static int hd_clutter_mutex_enabled = FALSE;
static int hd_clutter_mutex_do_unlock_after_disabling = FALSE;
static GStaticMutex hd_clutter_mutex = G_STATIC_MUTEX_INIT;

void hd_mutex_enable (int setting)
{
  /*g_printerr ("%s, setting %d\n", __func__, setting);*/
  if (hd_clutter_mutex_enabled && !setting)
    /* deactivating mutex, make sure the mutex is unlocked if
     * hd_mutex_unlock is called later */
    hd_clutter_mutex_do_unlock_after_disabling = TRUE;

  hd_clutter_mutex_enabled = setting;
}

static void
hd_mutex_lock (void)
{
  /*g_printerr ("%s, enabled %d\n", __func__, hd_clutter_mutex_enabled);*/
  if (hd_clutter_mutex_enabled)
    g_static_mutex_lock (&hd_clutter_mutex);
}

static void
hd_mutex_unlock (void)
{
  /*g_printerr ("%s, enabled %d\n", __func__, hd_clutter_mutex_enabled);*/
  if (hd_clutter_mutex_enabled)
    g_static_mutex_unlock (&hd_clutter_mutex);
  else if (hd_clutter_mutex_do_unlock_after_disabling)
    {
      g_static_mutex_unlock (&hd_clutter_mutex);
      hd_clutter_mutex_do_unlock_after_disabling = FALSE;
    }
}

static void
hd_mutex_init (void)
{
  /* g_printerr ("%s%d\n", __func__); */
  if (!g_thread_supported ())
    g_error ("g_thread_init() must be called before %s", __func__);

  clutter_threads_set_lock_functions (hd_mutex_lock, hd_mutex_unlock);
}

/* Take screenshot */
static void
take_screenshot (void)
{
  char *path, *filename;
  static gchar datestamp[255];
  static time_t secs = 0;
  struct tm *tm = NULL;
  GdkDrawable *window;
  int width, height;
  GdkPixbuf *image;
  GError *error = NULL;
  gboolean ret;

  if (!getenv("MYDOCSDIR")) {
    g_warning ("Screenshot failed, environment variable MYDOCSDIR missing.");
    return;
  }

  /* limit the rate of screenshots to avoid jamming HD when the key
   * is pressed all the time */
  if (time (NULL) - secs < 5)
    return;

  path = g_strdup_printf ("%s/.images/Screenshots", getenv("MYDOCSDIR"));
  g_mkdir_with_parents (path, 0770);

  secs = time(NULL);
  tm = localtime(&secs);
  strftime (datestamp, 255, "%Y%m%d-%H%M%S", tm);

  filename = g_strdup_printf ("%s/Screenshot-%s.png",
			      path,
			      datestamp);
  g_free (path);

  window = gdk_get_default_root_window();
  gdk_drawable_get_size(window, &width, &height);
  image = gdk_pixbuf_get_from_drawable(NULL,
				       window,
				       gdk_drawable_get_colormap(window),
				       0, 0,
				       0, 0,
				       width, height);
  ret = gdk_pixbuf_save (image, filename, "png", &error, NULL);
  g_object_unref(image);

  if (ret) {
    g_debug ("Screenshot '%s' saved.", filename);
  } else if (error) {
    g_warning ("%s: Image saving failed: %s", __func__, error->message);
    g_error_free (error);
  }
  g_free (filename);
}

static unsigned int
theme_type_func (const char *theme_name, void *data)
{
  if (theme_name && !strcmp (theme_name, "hildon"))
    return HD_TYPE_THEME;

  return MB_WM_TYPE_THEME;
}

static unsigned int
theme_client_type_func (const char *type_name, void *user_data)
{
  if (!type_name)
    return 0;

  if (!strcmp (type_name, "application-menu"))
    return HdWmClientTypeAppMenu;
  else if (!strcmp (type_name, "home-applet"))
    return HdWmClientTypeHomeApplet;
  else if (!strcmp (type_name, "status-menu"))
      return HdWmClientTypeStatusMenu;

  return 0;
}

static unsigned int
theme_button_type_func (const char *type_name,
			void       *user_data)
{
  if (type_name && !strcmp (type_name, "back"))
    return HdHomeThemeButtonBack;

  return 0;
}

/* Toggle the portrait-capable flag on and off on the topmost window */
static void
toggle_portraitable(MBWindowManager   *wm)
{
  MBWindowManagerClient *c;
  for (c=wm->stack_top;c;c=c->stacked_below)
    {
      MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (c);
      if (c_type == MBWMClientTypeDesktop)
        break;
      if (c_type == MBWMClientTypeApp)
        {
          /* actually set the portrait property to the opposite now */
          gboolean new_supports = !hd_comp_mgr_client_supports_portrait(c);
          guint value = new_supports ? 1 : 0;
          mb_wm_util_async_trap_x_errors (wm->xdpy);
          XChangeProperty(wm->xdpy, c->window->xwindow,
                          wm->atoms[MBWM_ATOM_HILDON_PORTRAIT_MODE_SUPPORT],
                          XA_CARDINAL, 32, PropModeReplace,
                          (unsigned char *)&value, 1);
          mb_wm_util_async_untrap_x_errors ();
        }
    }
}

static void
key_binding_func (MBWindowManager   *wm,
		  MBWMKeyBinding    *binding,
		  void              *userdata)
{
  int action;

  action = (int)(userdata);

  switch (action)
    {
    case KEY_ACTION_TOGGLE_SWITCHER:
      /* don't go to the switcher if we are showing a system-modal */
      if (hd_render_manager_get_state () == HDRM_STATE_TASK_NAV ) {
          switch(conf_ctrl_backspace_in_tasknav) {
              case 1:
                  hd_render_manager_set_state (HDRM_STATE_HOME);
                  break;
              case 2:
                  hd_render_manager_set_state (HDRM_STATE_LAUNCHER);
                  break;
              case 3:
                  hd_task_navigator_activate(-2, -2, 0);
                  break;
              case 4:
                  hd_task_navigator_activate(-1, -2, 0);
                  break;
              case 5:
                  in_alt_tab = TRUE;
                  hd_task_navigator_rotate_thumbs();
                  break;
  
              case 0:
              default:
                  break;
          }
      } else { 
      if (!hd_wm_has_modal_blockers (hd_mb_wm))
        hd_render_manager_set_state (HDRM_STATE_TASK_NAV);
            if(conf_ctrl_backspace_in_tasknav==5) { 
                in_alt_tab = TRUE;
                    hd_task_navigator_sort_thumbs();
                hd_task_navigator_rotate_thumbs();
            }
      }
      break;
    case KEY_ACTION_TOGGLE_NON_COMP_MODE:
      /* printf(" ### KEY_ACTION_TOGGLE_NON_COMP_MODE ###\n"); */
      if (hd_render_manager_get_state () == HDRM_STATE_NON_COMPOSITED)
        hd_render_manager_set_state (HDRM_STATE_APP);
      else if (hd_render_manager_get_state () == HDRM_STATE_NON_COMP_PORT)
        hd_render_manager_set_state (HDRM_STATE_APP_PORTRAIT);
      else if (hd_render_manager_get_state () == HDRM_STATE_APP)
        {
          hd_render_manager_set_state (HDRM_STATE_NON_COMPOSITED);
          /* render manager does not unredirect non-fullscreen apps,
           * so do it here */
          hd_comp_mgr_unredirect_topmost_client (hd_mb_wm, TRUE);
        }
      else if (hd_render_manager_get_state () == HDRM_STATE_APP_PORTRAIT)
        {
          hd_render_manager_set_state (HDRM_STATE_NON_COMP_PORT);
          hd_comp_mgr_unredirect_topmost_client (hd_mb_wm, TRUE);
        }
      break;
    case KEY_ACTION_TAKE_SCREENSHOT:
        take_screenshot();
	break;
    case KEY_ACTION_XTERMINAL:
      {
        GPid pid;
        if (hd_app_mgr_execute ("/usr/bin/osso-xterm", &pid, TRUE))
          g_spawn_close_pid (pid);
        break;
      }
    case KEY_ACTION_TOGGLE_PORTRAITABLE:
        toggle_portraitable(wm);
        break;
    case KEY_ACTION_ROTATE:
        hd_transition_rotate_screen (wm, !hd_comp_mgr_is_portrait());
        break;
    }
}
static void
key_binding_func_key (MBWindowManager   *wm,
		  MBWMKeyBinding    *binding,
		  void              *userdata)
{
  int action;
  char s[32];

  action = (int)(userdata);
  sprintf(s,"%i",action);

  hd_dbus_send_event (s);
}

static ClutterX11FilterReturn
clutter_x11_event_filter (XEvent *xev, ClutterEvent *cev, gpointer data)
{
  MBWindowManager * wm = data;

  if (xev->type == ButtonPress)
    {
      hd_render_manager_press_effect ();
    }

  mb_wm_main_context_handle_x_event (xev, wm->main_ctx);

  if (wm->sync_type)
    mb_wm_sync (wm);
  return CLUTTER_X11_FILTER_CONTINUE;
}

/* Debugging aids */
static gboolean
dump_debug_info_when_idle (gpointer unused)
{
  extern void hd_comp_mgr_dump_debug_info (const gchar *tag);
  hd_comp_mgr_dump_debug_info ("SIGUSR1");
  return FALSE;
}

static void
dump_debug_info_sighand (int unused)
{
  if (hd_debug_mode_set)
    hd_debug_mode_set = FALSE;
  else
    hd_debug_mode_set = TRUE;

  g_idle_add (dump_debug_info_when_idle, GINT_TO_POINTER (0));
  signal(SIGUSR1, dump_debug_info_sighand);
}

/* Returns the pid of the currently running hildon-desktop process or -1. */
static pid_t
hd_already_running (Display *dpy)
{
  Window root;
  char *wmname;
  pid_t *wmpid, pid;
  Window *wmwin, *wmwin2;
  Atom xpcheck, xpname, xppid;

  pid = -1;
  root = DefaultRootWindow(dpy);

  /* Read _NET_WM_PID of the _NET_SUPPORTING_WM_CHECK window. */
  xpcheck = XInternAtom (dpy, "_NET_SUPPORTING_WM_CHECK", False);
  wmwin = hd_util_get_win_prop_data_and_validate (dpy, root, xpcheck,
                                                  XA_WINDOW, 32, 1, NULL);
  if (!wmwin)
    goto out0;

  wmwin2 = hd_util_get_win_prop_data_and_validate (dpy, *wmwin, xpcheck,
                                                   XA_WINDOW, 32, 1, NULL);
  if (!wmwin2)
    {
      g_warning ("dangling _NET_SUPPORTING_WM_CHECK property on the root window");
      goto out1;
    }
  else if (*wmwin != *wmwin2)
    {
      g_warning ("inconsistent _NET_SUPPORTING_WM_CHECK values");
      goto out2;
    }

  xpname = XInternAtom (dpy, "_NET_WM_NAME", False);
  wmname = hd_util_get_win_prop_data_and_validate (dpy, *wmwin, xpname,
                                 XInternAtom (dpy, "UTF8_STRING", False),
                                 8, 0, NULL);
  if (!wmname)
    {
      g_warning ("_NET_SUPPORTING_WM_CHECK window has no _NET_WM_NAME");
      goto out2;
    }
  if (strcmp(wmname, PACKAGE))
    /* Not us. */
    goto out3;

  xppid = XInternAtom (dpy, "_NET_WM_PID", False);
  wmpid = hd_util_get_win_prop_data_and_validate (dpy, *wmwin, xppid,
                                                  XA_CARDINAL, 32, 1, NULL);
  if (!wmpid)
    {
      g_critical ("our previous instance didn't set _NET_WM_PID");
      goto out3;
    }

  pid = *wmpid;
  XFree (wmpid);
out3:
  XFree (wmname);
out2:
  XFree (wmwin2);
out1:
  XFree (wmwin);
out0:
  return pid;
}

static gboolean
get_program_file (const gchar *link, gchar *path, size_t spath)
{
  gint n;
  gchar *p;

  if ((n = readlink (link, path, spath-1)) < 0)
    {
      g_warning ("%s: %m", link);
      return FALSE;
    }

  path[n] = '\0';
  if ((p = strstr(path, " (deleted)")) != NULL)
    *p = '\0';

  return TRUE;
}

static gboolean
same_file (const gchar *link1, const gchar *link2)
{
  char path1[128], path2[128];

  if (!get_program_file (link1, path1, sizeof (path1)))
    return FALSE;
  if (!get_program_file (link2, path2, sizeof (path2)))
    return FALSE;

  return !strcmp(path1, path2);
}

static void
relaunch (int unused)
{
  char me[128];
  MBWMRootWindow *root;

  g_warning ("Relaunching myself...");
  if (!get_program_file ("/proc/self/exe", me, sizeof (me)))
    return;

  root = mb_wm_root_window_get (NULL);
  g_return_if_fail (root && root->wm);
  execv (me, root->wm->argv);
  g_warning ("%s: %m", me);
}

static void
try_to_relaunch (Display *dpy)
{
  pid_t mate;
  gchar *matepath;

  if ((mate = hd_already_running (dpy)) < 0)
    return;

  /* If our executable has overritten the other's ask it
   * to relaunch itself rather than killing it.  It has
   * the advantage of preserving file descriptor redirections. */
  matepath = g_strdup_printf ("/proc/%d/exe", mate);
  if (same_file (matepath, "/proc/self/exe"))
    {
      g_warning ("%d: reborn", mate);
      kill (mate, SIGHUP);
      exit (0);
    }
  else
    {
      g_warning("killing fellow hd %d", mate);
      kill (mate, SIGTERM);
      usleep (500000);
    }
  g_free (matepath);
}

static gboolean
get_debug_info (Display *dpy)
{
  pid_t mate;

  if ((mate = hd_already_running (dpy)) < 0)
    {
      g_warning ("Mate ain't not found");
      return FALSE;
    }
  else if (kill (mate, SIGUSR1) < 0)
    {
      g_warning ("kill(%d): %m", mate);
      return FALSE;
    }
  else
    return TRUE;
}

static void
terminating (int unused)
{
  gtk_main_quit ();
}

typedef enum
{
  OSSO_FPU_IEEE,    /* Usual processor mode, slow and accurate */
  OSSO_FPU_FAST     /* Fast but a bit non-accurate mode        */
} OSSO_FPU_MODE;

static void hd_fpu_set_mode(OSSO_FPU_MODE mode)
{
#ifdef __arm__
  if (OSSO_FPU_FAST == mode)
  {
    int tmp;
    __asm__ volatile(
        "fmrx       %[tmp], fpscr\n"
        "orr        %[tmp], %[tmp], #(1 << 24)\n" /* flush-to-zero */
        "orr        %[tmp], %[tmp], #(1 << 25)\n" /* default NaN */
        "bic        %[tmp], %[tmp], #((1 << 15) | (1 << 12) | (1 << 11) | (1 << 10) | (1 << 9) | (1 << 8))\n" /* clear exception bits */
        "fmxr       fpscr, %[tmp]\n"
        : [tmp] "=r" (tmp)
      );
  }
  else
  {
    int tmp;
    __asm__ volatile(
        "fmrx       %[tmp], fpscr\n"
        "bic        %[tmp], %[tmp], #(1 << 24)\n" /* flush-to-zero */
        "bic        %[tmp], %[tmp], #(1 << 25)\n" /* default NaN */
        "fmxr       fpscr, %[tmp]\n"
        : [tmp] "=r" (tmp)
      );
  }
#endif /* if __arm__ */
}

int
main (int argc, char **argv)
{
  Display * dpy = NULL;
  MBWindowManager *wm;
  HdAppMgr *app_mgr;
  char keys1[32], c; 

  signal (SIGUSR1, dump_debug_info_sighand);
  signal (SIGHUP,  relaunch);
  signal (SIGTERM, terminating);

  /* fast float calculations */
  hd_fpu_set_mode (OSSO_FPU_FAST);

  /*
  g_log_set_always_fatal (G_LOG_LEVEL_ERROR    |
			  G_LOG_LEVEL_CRITICAL |
			  G_LOG_LEVEL_WARNING);
                          */

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, "/usr/share/locale");
  textdomain (GETTEXT_PACKAGE);

  g_thread_init (NULL);

  gnome_vfs_init ();

  mb_wm_object_init();

  mb_wm_theme_set_custom_theme_type_func (theme_type_func, NULL);
  mb_wm_theme_set_custom_theme_alloc_func (hd_theme_alloc_func);
  mb_wm_theme_set_custom_client_type_func (theme_client_type_func, NULL);
  mb_wm_theme_set_custom_button_type_func (theme_button_type_func, NULL);

  hildon_gtk_init (&argc, &argv);
  /* Initialise the async error handler. Do it after gtk is inited, or gtk
   * will grab the handler for itself */
  mb_wm_util_async_x_error_init();

  hd_mutex_init ();

  /* NB: We _dont_ pass the X display from gtk into clutter because
   * it makes life far too complicated to have gtkish things like
   * creating dialogs going on using the same X display as the window
   * manager because it breaks all kinds of assumptions about
   * the relationship with client windows and the window manager.
   *
   * For example it breaks the SubStructureRedirect mechanism, and
   * assumptions about the number of benign UnmapNotifies the window
   * manager will see when reparenting "client" windows. */
  clutter_init (&argc, &argv);
  /* Disable mipmapping of text, as it is seldom scaled down and this
   * saves us memory/bandwidth/update speed */
  clutter_set_use_mipmapped_text(FALSE);
  /* Use software-based selection, which is much faster on SGX than rendering
   * with 'GL and reading back */
  clutter_set_software_selection(TRUE);

#ifndef DISABLE_A11Y
  hildon_desktop_a11y_init ();
#endif

  dpy = clutter_x11_get_default_display ();

  gdk_pixbuf_xlib_init (dpy, clutter_x11_get_default_screen ());

  /* Just before mb discovers there's a WM already and aborts
   * see if it's hildon-desktop and take it over. */
  if (argv[1] && !strcmp (argv[1], "-r"))
    try_to_relaunch (dpy);
  else if (argv[1] && !strcmp (argv[1], "-d"))
    return get_debug_info (dpy) != TRUE;

  wm = MB_WINDOW_MANAGER (mb_wm_object_new (HD_TYPE_WM,
					    MBWMObjectPropArgc, argc,
					    MBWMObjectPropArgv, argv,
					    MBWMObjectPropDpy,  dpy,
					    NULL));
  if (wm == NULL)
    mb_wm_util_fatal_error("OOM?");

  mb_wm_rename_window (wm, wm->root_win->hidden_window, PACKAGE);
  mb_wm_init (wm);
  g_assert (mb_wm_comp_mgr_enabled (wm->comp_mgr));

  if(conf_enable_ctrl_backspace) {
  mb_wm_keys_binding_add_with_spec (wm,
				    "<ctrl>BackSpace",
				    key_binding_func,
				    NULL,
				    (void*)KEY_ACTION_TOGGLE_SWITCHER);
  }
  if(conf_enable_preset_shift_ctrl) {
  mb_wm_keys_binding_add_with_spec (wm,
				    "<shift><ctrl>x",
				    key_binding_func,
				    NULL,
				    (void*)KEY_ACTION_XTERMINAL);
  mb_wm_keys_binding_add_with_spec (wm,
				    "<shift><ctrl>n",
				    key_binding_func,
				    NULL,
				    (void*)KEY_ACTION_TOGGLE_NON_COMP_MODE);
  mb_wm_keys_binding_add_with_spec (wm,
				    "<shift><ctrl>p",
				    key_binding_func,
				    NULL,
				    (void*)KEY_ACTION_TAKE_SCREENSHOT);
  mb_wm_keys_binding_add_with_spec (wm,
                                      "<shift><ctrl>r",
                                      key_binding_func,
                                      NULL,
                                      (void*)KEY_ACTION_TOGGLE_PORTRAITABLE);
  mb_wm_keys_binding_add_with_spec (wm, /* mod5 == Fn */
                                      "<shift><ctrl><mod5>l",
                                      key_binding_func,
                                      NULL,
                                      (void*)KEY_ACTION_ROTATE);
  }

  if(conf_enable_dbus_shift_ctrl) {
      if(conf_dbus_shortcuts_use_fn) {
          mb_wm_keys_binding_add_with_spec (wm,
                           "<ctrl><mod5>Space",
                           key_binding_func_key,
                           NULL,
                           (void*)(192+32));
          mb_wm_keys_binding_add_with_spec (wm,
                           "<ctrl><mod5>comma",
                           key_binding_func_key,
                           NULL,
                           (void*)(192+33));
          mb_wm_keys_binding_add_with_spec (wm,
                           "<ctrl><mod5>period",
                           key_binding_func_key,
                           NULL,
                           (void*)(192+34));
      } else {
          mb_wm_keys_binding_add_with_spec (wm,
                           "<shift><ctrl>Space",
                           key_binding_func_key,
                           NULL,
                           (void*)(192+32));
          mb_wm_keys_binding_add_with_spec (wm,
                           "<shift><ctrl>comma",
                           key_binding_func_key,
                           NULL,
                           (void*)(192+33));
          mb_wm_keys_binding_add_with_spec (wm,
                           "<shift><ctrl>period",
                           key_binding_func_key,
                           NULL,
                           (void*)(192+34));
      }
	  if(conf_dbus_ctrl_shortcuts) {
		  mb_wm_keys_binding_add_with_spec (wm,
							"<ctrl>F7",
							key_binding_func_key,
							NULL,
							(void*)247);
		  mb_wm_keys_binding_add_with_spec (wm,
							"<ctrl>F8",
							key_binding_func_key,
							NULL,
							(void*)248);
		  mb_wm_keys_binding_add_with_spec (wm,
						   "<ctrl>Space",
						   key_binding_func_key,
						   NULL,
						   (void*)(192+36));
		  mb_wm_keys_binding_add_with_spec (wm,
						   "<ctrl>comma",
						   key_binding_func_key,
						   NULL,
						   (void*)(192+37));
		  mb_wm_keys_binding_add_with_spec (wm,
						   "<ctrl>period",
						   key_binding_func_key,
						   NULL,
						   (void*)(192+38));
	  }

	  if(conf_dbus_shortcuts_use_fn) {
		  strcpy(keys1,"<ctrl><mod5>a");
	  } else {
		  strcpy(keys1,"<shift><ctrl>a");
	  }
	  for(c='a';c<='z';c++) if(!conf_enable_preset_shift_ctrl || conf_dbus_shortcuts_use_fn || ((c!='n') && (c!='p') && (c!='x') && (c!='h'))){
		  keys1[strlen(keys1)-1]=c;
		  mb_wm_keys_binding_add_with_spec (wm,
						keys1,
						key_binding_func_key,
						NULL,
						(void*)(192+c-'a'+1));
	  }
  }


  clutter_x11_add_filter (clutter_x11_event_filter, wm);

  app_mgr = hd_app_mgr_get ();

  hd_volume_profile_init ();

  /* Move to landscape for safety. */
  if (hd_util_change_screen_orientation (wm, FALSE));
    hd_util_root_window_configured (wm);

  /* NB: we call gtk_main as opposed to clutter_main or mb_wm_main_loop
   * because it does the most extra magic, such as supporting quit functions
   * that the others don't. Except for adding the clutter_x11_add_filter
   * (manually done above) it appears be a super set of the other two
   * so everything *should* be covered this way. */
  gtk_main ();

  mb_wm_object_unref (MB_WM_OBJECT (wm));

  hd_app_mgr_stop ();
  g_object_unref (app_mgr);
  signal (SIGTERM, SIG_DFL);

#if MBWM_WANT_DEBUG
  mb_wm_object_dump ();
#endif

  return 0;
}
