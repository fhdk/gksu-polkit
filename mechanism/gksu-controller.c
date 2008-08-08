/*
 * Copyright (C) 2008 Gustavo Noronha Silva
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.  You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>

#include <gksu-environment.h>
#include <gksu-error.h>

#include "gksu-controller.h"

G_DEFINE_TYPE(GksuController, gksu_controller, G_TYPE_OBJECT);

struct _GksuControllerPrivate {
  DBusGConnection *dbus;
  gchar *working_directory;
  gchar **arguments;
  gchar *xauth_file;
  gint pid;
};

#define GKSU_CONTROLLER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GKSU_TYPE_CONTROLLER, GksuControllerPrivate))

enum {
  PROCESS_EXITED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

void gksu_controller_cleanup(GksuController *self)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);
  gchar *xauth_dir = g_path_get_dirname(priv->xauth_file);
  
  unlink(priv->xauth_file);

  if(rmdir(xauth_dir))
    g_warning("Unable to remove directory %s: %s", xauth_dir,
              g_strerror(errno));

  g_free(xauth_dir);
}

static void gksu_controller_real_process_exited(GksuController *self, gint status)
{
  gksu_controller_cleanup(self);

  /* take out our own reference */
  g_object_unref(self);
}

static void gksu_controller_finalize(GObject *object)
{
  GksuController *self = GKSU_CONTROLLER(object);
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);

  g_free(priv->working_directory);
  g_free(priv->xauth_file);
  g_strfreev(priv->arguments);

  G_OBJECT_CLASS(gksu_controller_parent_class)->finalize(object);
}

static void gksu_controller_class_init(GksuControllerClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = gksu_controller_finalize;

  signals[PROCESS_EXITED] = g_signal_new("process-exited",
                                         GKSU_TYPE_CONTROLLER,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET(GksuControllerClass, process_exited),
                                         NULL,
                                         NULL,
                                         g_cclosure_marshal_VOID__INT,
                                         G_TYPE_NONE, 1,
                                         G_TYPE_INT);

  klass->process_exited = gksu_controller_real_process_exited;

  g_type_class_add_private(klass, sizeof(GksuControllerPrivate));
}

static void gksu_controller_init(GksuController *self)
{
}

static void gksu_controller_process_exited_cb(GPid pid, gint status, GksuController *self)
{
  g_signal_emit(self, signals[PROCESS_EXITED], 0, status);
}

static gboolean gksu_controller_prepare_xauth(GksuController *self, GHashTable *environment,
                                              gchar *xauth_token)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);
  gchar *xauth_dirtemplate = g_strdup ("/tmp/" PACKAGE_NAME "-XXXXXX");
  gchar *xauth_bin = NULL;
  gchar *xauth_dir = NULL;
  gchar *xauth_file = NULL;
  gchar *xauth_display = NULL;
  gchar *xauth_cmd = NULL;

  FILE *file;
  gchar *command;
  gchar *tmpfilename;
  gint return_code;
  GError *error = NULL;

  xauth_dir = mkdtemp (xauth_dirtemplate);
  if (!xauth_dir)
    {
      g_warning("Failed creating xauth_dir.\n");
      return FALSE;
    }

  xauth_file = g_strdup_printf("%s/.Xauthority", xauth_dir);
  g_free(xauth_dir);

  xauth_display = g_hash_table_lookup(environment, "DISPLAY");
  tmpfilename = g_strdup_printf ("%s.tmp", xauth_file);

  /* write a temporary file with a command to add the cookie we have */
  file = fopen(tmpfilename, "w");
  if(!file)
    {
      g_warning("Error writing temporary auth file: %s\n", tmpfilename);
      g_free(tmpfilename);
      return FALSE;
    }

  xauth_cmd = g_strdup_printf("add %s . %s\n", xauth_display, xauth_token);
  fwrite(xauth_cmd, sizeof(gchar), strlen(xauth_cmd), file);
  g_free(xauth_cmd);
  fclose(file);
  chmod(tmpfilename, S_IRUSR|S_IWUSR);
    
  /* actually create the real file */
  if (g_file_test("/usr/bin/xauth", G_FILE_TEST_IS_EXECUTABLE))
    xauth_bin = "/usr/bin/xauth";
  else if (g_file_test("/usr/X11R6/bin/xauth", G_FILE_TEST_IS_EXECUTABLE))
    xauth_bin = "/usr/X11R6/bin/xauth";
  else
    {
      unlink(tmpfilename);
      g_free(tmpfilename);
      g_warning("Failed to obtain xauth key: xauth binary not found "
                "at usual locations");

      return FALSE;
    }

  command = g_strdup_printf("%s -q -f %s source %s", xauth_bin, xauth_file, tmpfilename);

  g_spawn_command_line_sync(command, NULL, NULL, &return_code, &error);

  unlink(tmpfilename);
  g_free(tmpfilename);
  g_free(command);

  if(error)
    {
      g_warning("Failure running xauth: %s\n", error->message);
      g_error_free(error);
      return FALSE;
    }

  g_hash_table_replace(environment, g_strdup("XAUTHORITY"), g_strdup(xauth_file));
  priv->xauth_file = xauth_file;

  return TRUE;
}

GksuController* gksu_controller_new(gchar *working_directory, gchar *xauth, gchar **arguments,
                                    GHashTable *environment, DBusGConnection *dbus,
                                    gint *pid, GError **error)
{
  GksuController *self;
  GksuControllerPrivate *priv;
  GList *keys;
  GList *iter;
  gchar **environmentv;
  gint size = 0;
  GError *internal_error = NULL;

  self = g_object_new(GKSU_TYPE_CONTROLLER, NULL);

  /* First we handle xauth, and add the XAUTHORITY variable to the
   * environment, so that X-based applications will be able to open
   * their windows */
  if(!gksu_controller_prepare_xauth(self, environment, xauth))
    {
      g_object_unref(self);
      g_set_error(error, GKSU_ERROR, GKSU_ERROR_PREPARE_XAUTH_FAILED,
                  "Unable to prepare the X authorization environment.");
      return NULL;
    }

  priv = GKSU_CONTROLLER_GET_PRIVATE(self);
  environmentv = g_malloc(sizeof(gchar**));

  keys = g_hash_table_get_keys(environment);
  iter = keys;

  for(; iter != NULL; iter = iter->next)
  {
    gchar *key = (gchar*)iter->data;
    gchar *value = (gchar*)g_hash_table_lookup(environment, (gpointer)key);

    environmentv = g_realloc(environmentv, sizeof(gchar*) * (size + 1));
    environmentv[size] = g_strdup_printf("%s=%s", key, value);
    size++;
  }
  environmentv = g_realloc(environmentv, sizeof(gchar*) * (size + 1));
  environmentv[size] = NULL;

  g_spawn_async(working_directory, arguments, environmentv,
                G_SPAWN_FILE_AND_ARGV_ZERO|G_SPAWN_DO_NOT_REAP_CHILD,
                NULL, NULL, pid, &internal_error);

  g_strfreev(environmentv);

  if(internal_error)
    {
      g_warning("%s\n", internal_error->message);
      g_propagate_error(error, internal_error);
      return NULL;
    }

  priv->working_directory = g_strdup(working_directory);
  priv->arguments = g_strdupv(arguments);
  priv->dbus = dbus;
  priv->pid = *pid;

  g_child_watch_add(priv->pid,
                    (GChildWatchFunc)gksu_controller_process_exited_cb,
                    (gpointer)self);

  return self;
}

void gksu_controller_finish(GksuController *controller)
{
}

gint gksu_controller_get_pid(GksuController *self)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);
  return priv->pid;
}
