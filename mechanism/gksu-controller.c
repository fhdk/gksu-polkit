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
#include <fcntl.h>
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

  GIOChannel *stdin;
  guint stdin_source_id;

  GIOChannel *stdout;
  guint stdout_source_id;

  GIOChannel *stderr;
  guint stderr_source_id;
};

#define GKSU_CONTROLLER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GKSU_TYPE_CONTROLLER, GksuControllerPrivate))

enum {
  PROCESS_EXITED,
  OUTPUT_AVAILABLE,

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

  if(priv->stdin)
    {
      g_source_remove(priv->stdin_source_id);
      g_io_channel_unref(priv->stdin);
    }

  if(priv->stdout)
    {
      g_source_remove(priv->stdout_source_id);
      g_io_channel_unref(priv->stdout);
    }

  if(priv->stderr)
    {
      g_source_remove(priv->stderr_source_id);
      g_io_channel_unref(priv->stderr);
    }

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

  signals[OUTPUT_AVAILABLE] = g_signal_new("output-available",
                                           GKSU_TYPE_CONTROLLER,
                                           G_SIGNAL_RUN_LAST,
                                           0,
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

static gboolean gksu_controller_stdin_hangup_cb(GIOChannel *stdin,
                                                GIOCondition condition,
                                                GksuController *self)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);

  if(condition == G_IO_HUP)
    {
      g_source_remove(priv->stdin_source_id);
      return FALSE;
    }

  return FALSE;
}

static gboolean gksu_controller_stdout_ready_to_read_cb(GIOChannel *stdout,
                                                        GIOCondition condition,
                                                        GksuController *self)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);

  if(condition == G_IO_HUP)
    {
      g_source_remove(priv->stdout_source_id);
      return FALSE;
    }

  /* 1 == stdout */
  g_signal_emit(self, signals[OUTPUT_AVAILABLE], 0, 1);
  return FALSE;
}

static gboolean gksu_controller_stderr_ready_to_read_cb(GIOChannel *stderr,
                                                        GIOCondition condition,
                                                        GksuController *self)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);

  if(condition == G_IO_HUP)
    {
      g_source_remove(priv->stderr_source_id);
      return FALSE;
    }

  /* 2 == stderr */
  g_signal_emit(self, signals[OUTPUT_AVAILABLE], 0, 2);
  return FALSE;
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
                                    gboolean using_stdin, gboolean using_stdout,
                                    gboolean using_stderr, gint *pid, GError **error)
{
  GksuController *self;
  GksuControllerPrivate *priv;
  GList *keys;
  GList *iter;
  gchar **environmentv;
  gint size = 0;

  GSpawnFlags spawn_flags = G_SPAWN_SEARCH_PATH|G_SPAWN_DO_NOT_REAP_CHILD;

  /* the pointers are just to allow us to only pass in fds in which
   * our caller is interested */
  gint *stdin = NULL;
  gint *stdout = NULL;
  gint *stderr = NULL;
  gint stdin_real, stdout_real, stderr_real;

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

  /* if we are not using a given FD, it remains set to NULL, and
   * g_spawn_async_with_pipes handles it correctly
   */
  if(using_stdin)
    stdin = &stdin_real;

  if(using_stdout)
    stdout = &stdout_real;
  else
    spawn_flags |= G_SPAWN_STDOUT_TO_DEV_NULL;

  if(using_stderr)
    stderr = &stderr_real;
  else
    spawn_flags |= G_SPAWN_STDERR_TO_DEV_NULL;

  g_spawn_async_with_pipes(working_directory, arguments, environmentv,
                           spawn_flags, NULL, NULL, pid,
                           stdin, stdout, stderr, &internal_error);
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

  /* these conditions are here so that we don't waste resources on fds
   * in which our caller is not interested
   */
  if(stdin)
    {
      priv->stdin = g_io_channel_unix_new(stdin_real);
      g_io_channel_set_encoding(priv->stdin, NULL, NULL);
      g_io_channel_set_buffered(priv->stdin, FALSE);
      priv->stdin_source_id = 
        g_io_add_watch(priv->stdin, G_IO_HUP|G_IO_NVAL,
                       (GIOFunc)gksu_controller_stdin_hangup_cb,
                       (gpointer)self);
    }

  if(stdout)
    {
      priv->stdout = g_io_channel_unix_new(stdout_real);
      g_io_channel_set_flags(priv->stdout, G_IO_FLAG_NONBLOCK, NULL);
      /* the child may output binary data; we don't care */
      g_io_channel_set_encoding(priv->stdout, NULL, NULL);
      priv->stdout_source_id = 
        g_io_add_watch(priv->stdout, G_IO_IN|G_IO_PRI|G_IO_HUP,
                       (GIOFunc)gksu_controller_stdout_ready_to_read_cb,
                       (gpointer)self);
    }

  if(stderr)
    {
      priv->stderr = g_io_channel_unix_new(stderr_real);
      g_io_channel_set_flags(priv->stderr, G_IO_FLAG_NONBLOCK, NULL);
      g_io_channel_set_encoding(priv->stderr, NULL, NULL);
      priv->stderr_source_id = 
        g_io_add_watch(priv->stderr, G_IO_IN|G_IO_PRI|G_IO_HUP,
                       (GIOFunc)gksu_controller_stderr_ready_to_read_cb,
                       (gpointer)self);
    }

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

void gksu_controller_close_fd(GksuController *self, gint fd, GError **error)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);
  GIOChannel *channel;
  GError *internal_error = NULL;

  switch(fd)
    {
    case 0:
      channel = priv->stdin;
      break;
    case 1:
      channel = priv->stdout;
      break;
    case 2:
      channel = priv->stderr;
      break;
    default:
      fprintf(stderr, "Trying to close invalid FD '%d' for PID '%d'\n",
              fd, priv->pid);
      return;
    }

  if(channel == NULL)
    return;

  g_io_channel_shutdown(channel, TRUE, &internal_error);
  if(internal_error)
    {
      g_warning("%s", internal_error->message);
      g_propagate_error(error, internal_error);
    }
}

gchar* gksu_controller_read_output(GksuController *self, gint fd,
                                   gsize *length, gboolean read_to_end)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);
  GIOChannel *channel;
  guint *source_id;
  GIOFunc handler_func;
  GError *error = NULL;
  GString *retstring;
  gchar *retdata;
  gchar buffer[1024];
  gsize buffer_length = -1;
  gint count;

  switch(fd)
    {
    case 1:
      channel = priv->stdout;
      source_id = &(priv->stdout_source_id);
      handler_func = (GIOFunc)gksu_controller_stdout_ready_to_read_cb;
      break;
    case 2:
      channel = priv->stderr;
      source_id = &(priv->stderr_source_id);
      handler_func = (GIOFunc)gksu_controller_stderr_ready_to_read_cb;
      break;
    default:
      return FALSE;
    }

  retstring = g_string_new("");

  /*
   * Unless we're told to read every possible character, we read only
   * 5 x 1024 chars, in order to not let the rest of the server suffer
   * from starvation when the child outputs loads of text. Reading
   * everything in one go is important for when the child is gone,
   * though, so that we can quickly store the pending output in the
   * GksuZombie, in GksuServer.
   */
  for(count = 0; (((buffer_length != 0) && (read_to_end)) || ((buffer_length != 0) && (count < 5))); count++)
    {
      GIOStatus status;
      status = g_io_channel_read_chars(channel, buffer, 1024, &buffer_length, &error);
      if(error)
        {
          fprintf(stderr, "%s\n", error->message);
          g_error_free(error);
        }
      g_string_append_len(retstring, buffer, buffer_length);
    }

  retdata = retstring->str;
  *length = retstring->len;
  g_string_free(retstring, FALSE);

  if((count == 5) && (buffer_length != 0))
    g_signal_emit(self, signals[OUTPUT_AVAILABLE], 0, fd);
  else
    {
     *source_id = 
        g_io_add_watch(channel, G_IO_IN|G_IO_PRI|G_IO_HUP,
                       handler_func,
                       (gpointer)self);
    }

  return retdata;
}

gboolean gksu_controller_write_input(GksuController *self, const gchar *data,
                                     const gsize length, GError **error)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);
  GIOChannel *channel = priv->stdin;
  GError *internal_error = NULL;
  gsize bytes_written =0;
  gsize bytes_left = length;
  gchar *writing_from = (gchar*)data;

  while(bytes_left > 0)
    {
      g_io_channel_write_chars(channel, writing_from, bytes_left,
                               &bytes_written, &internal_error);
      if(internal_error)
        {
          fprintf(stderr, "%s\n", internal_error->message);
          g_propagate_error(error, internal_error);
          return FALSE;
        }

      bytes_left = bytes_left - bytes_written;
      writing_from = writing_from + bytes_written;
    }

  return TRUE;
}

gboolean gksu_controller_is_using_stdout(GksuController *self)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);

  if(priv->stdout)
    return TRUE;

  return FALSE;
}

gboolean gksu_controller_is_using_stderr(GksuController *self)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);

  if(priv->stderr)
    return TRUE;

  return FALSE;
}
