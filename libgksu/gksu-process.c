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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <polkit-dbus/polkit-dbus.h>

#include <gksu-environment.h>
#include <gksu-write-queue.h>
#include <gksu-marshal.h>

#include "gksu-process.h"

G_DEFINE_TYPE(GksuProcess, gksu_process, G_TYPE_OBJECT);

struct _GksuProcessPrivate {
  DBusGConnection *dbus;
  DBusGProxy *server;
  gchar *working_directory;
  gchar **arguments;
  GksuEnvironment *environment;
  gint pid;
  guint32 cookie;

  /* we keep the pipe to let the application talk to us, and us to it,
   * and we handle our side using GIOChannels; in order to know if the
   * application decides to close an FD we also keep a 'mirror'
   * GIOChannel for its side of the pipe, so that we can monitor the
   * application closing an FD, for instance */
  gint stdin[2];
  GIOChannel *stdin_channel;
  guint stdin_source_id;
  GIOChannel *stdin_mirror;
  guint stdin_mirror_id;

  /* We need the write queue because the channel to which we need to
   * write is not always available for writing, and that may be
   * because the buffer is filled, and application needs to read some
   * of it; we need to give it a chance to read the buffer, thus, but
   * still need to remember to write what is left */
  gint stdout[2];
  GIOChannel *stdout_channel;
  GIOChannel *stdout_mirror;
  guint stdout_mirror_id;
  GksuWriteQueue *stdout_write_queue;

  gint stderr[2];
  GIOChannel *stderr_channel;
  GIOChannel *stderr_mirror;
  guint stderr_mirror_id;
  GksuWriteQueue *stderr_write_queue;
};

#define GKSU_PROCESS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GKSU_TYPE_PROCESS, GksuProcessPrivate))

enum {
  EXITED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

static void process_died_cb(DBusGProxy *server, gint pid, GksuProcess *self)
{
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);
  GError *error = NULL;
  gint status;

  /* we only care about the process we are managing */
  if(pid != priv->pid)
    return;

  dbus_g_proxy_call(server, "Wait", &error,
                    G_TYPE_UINT, priv->cookie,
                    G_TYPE_INVALID,
                    G_TYPE_INT, &status,
                    G_TYPE_INVALID);

  if(error)
    {
      g_warning("Error on wait message reply: %s\n", error->message);
      g_error_free(error);
      status = -1;
    }
  g_signal_emit(self, signals[EXITED], 0, status);
}

static void output_available_cb(DBusGProxy *server, gint pid, gint fd, GksuProcess *self)
{
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);
  GError *error = NULL;
  gchar *data = NULL;
  gsize length;

  if(pid != priv->pid)
    return;

  dbus_g_proxy_call(server, "ReadOutput", &error,
                    G_TYPE_UINT, priv->cookie,
                    G_TYPE_INT, fd,
                    G_TYPE_INVALID,
                    G_TYPE_STRING, &data,
                    G_TYPE_UINT, &length,
                    G_TYPE_INVALID);

  if(error)
    {
      g_warning("%s", error->message);
      g_error_free(error);
      return;
    }

  switch(fd)
    {
    case 1:
      if(priv->stdout_channel)
        {
          gksu_write_queue_add(priv->stdout_write_queue, data, length);
        }
      break;
    case 2:
      if(priv->stderr_channel)
        {
          gksu_write_queue_add(priv->stderr_write_queue, data, length);
        }
      break;
    }

  g_free(data);
}

static void gksu_process_finalize(GObject *object)
{
  GksuProcess *self = GKSU_PROCESS(object);
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);

  if(priv->stdin_channel)
    {
      g_source_remove(priv->stdin_source_id);
      g_io_channel_unref(priv->stdin_channel);
    }
  if(priv->stdin_mirror)
    {
      g_source_remove(priv->stdin_mirror_id);
      g_io_channel_unref(priv->stdin_mirror);
    }

  if(priv->stdout_channel)
    {
      g_object_unref(priv->stdout_write_queue);
      g_io_channel_unref(priv->stdout_channel);
    }
  if(priv->stdout_mirror)
    {
      g_source_remove(priv->stdout_mirror_id);
      g_io_channel_unref(priv->stdout_mirror);
    }

  if(priv->stderr_channel)
    {
      g_object_unref(priv->stderr_write_queue);
      g_io_channel_unref(priv->stderr_channel);
    }
  if(priv->stderr_mirror)
    {
      g_source_remove(priv->stderr_mirror_id);
      g_io_channel_unref(priv->stderr_mirror);
    }

  g_free(priv->working_directory);
  g_strfreev(priv->arguments);
  g_object_unref(priv->environment);

  G_OBJECT_CLASS(gksu_process_parent_class)->finalize(object);
}

static void gksu_process_class_init(GksuProcessClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = gksu_process_finalize;

  signals[EXITED] = g_signal_new("exited",
                                 GKSU_TYPE_PROCESS,
                                 G_SIGNAL_RUN_LAST,
                                 0,
                                 NULL,
                                 NULL,
                                 g_cclosure_marshal_VOID__INT,
                                 G_TYPE_NONE, 1,
                                 G_TYPE_INT);

  g_type_class_add_private(klass, sizeof(GksuProcessPrivate));
}

static void gksu_process_init(GksuProcess *self)
{
  GError *error = NULL;
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);
  self->priv = priv;

  priv->dbus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  if(error)
    {
      g_error(error->message);
      exit(1);
    }

  priv->server = dbus_g_proxy_new_for_name(priv->dbus,
                                           "org.gnome.Gksu",
                                           "/org/gnome/Gksu",
                                           "org.gnome.Gksu");

  dbus_g_object_register_marshaller(gksu_marshal_VOID__INT_INT,
                                    G_TYPE_NONE, G_TYPE_INT, G_TYPE_INT,
                                    G_TYPE_INVALID);

  dbus_g_proxy_add_signal(priv->server, "ProcessExited",
                          G_TYPE_INT, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(priv->server, "ProcessExited",
                              G_CALLBACK(process_died_cb),
                              (gpointer)self, NULL);

  dbus_g_proxy_add_signal(priv->server, "OutputAvailable",
                          G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(priv->server, "OutputAvailable",
                              G_CALLBACK(output_available_cb),
                              (gpointer)self, NULL);

  priv->environment = gksu_environment_new();

  priv->stdin_channel = NULL;
  priv->stdout_channel = NULL;
  priv->stderr_channel = NULL;
}

/* copied from libgksu */
static gchar*
get_xauth_token(const gchar *explicit_display)
{
  gchar *display;
  gchar *xauth_bin = NULL;
  FILE *xauth_output;
  gchar *tmp = NULL;
  gchar *xauth = g_new0(gchar, 256);

  if(explicit_display == NULL)
    display = (gchar*)g_getenv("DISPLAY");
  else
    display = (gchar*)explicit_display;

  /* find out where the xauth binary is located */
  if (g_file_test ("/usr/bin/xauth", G_FILE_TEST_IS_EXECUTABLE))
    xauth_bin = "/usr/bin/xauth";
  else if (g_file_test ("/usr/X11R6/bin/xauth", G_FILE_TEST_IS_EXECUTABLE))
    xauth_bin = "/usr/X11R6/bin/xauth";
  else
    {
      g_warning("Failed to obtain xauth key: xauth binary not found "
                "at usual locations.");

      return NULL;
    }

  /* get the authorization token */
  tmp = g_strdup_printf ("%s -i list %s | "
			 "head -1 | awk '{ print $3 }'",
			 xauth_bin,
			 display);
  if ((xauth_output = popen (tmp, "r")) == NULL)
    {
      g_warning("Failed to obtain xauth key: %s", g_strerror(errno));
      return NULL;
    }

  fread (xauth, sizeof(char), 255, xauth_output);
  pclose (xauth_output);
  g_free (tmp);

  /* If xauth is the empty string, then try striping the
   * hostname part of the DISPLAY string for getting the
   * auth token; this is needed for ssh-forwarded usage
   */
  if((!strcmp("", xauth)) && (explicit_display == NULL))
    {
      gchar *cut_display = NULL;

      g_free (xauth);
      cut_display = g_strdup(g_strrstr (display, ":"));
      xauth = get_xauth_token(cut_display);
      g_free(cut_display);
    }

  return xauth;
}

static void gksu_process_prepare_pipe(GIOChannel **channel, GIOChannel **mirror,
                                      gint stdpipe[2], gint *fd, gboolean is_input)
{
  pipe(stdpipe);
  fcntl(stdpipe[0], F_SETFL, O_NONBLOCK);
  fcntl(stdpipe[1], F_SETFL, O_NONBLOCK);

  /*
   * is_input defines whether the pipe fds are for output (FALSE) or
   * input (TRUE)
   */
  if(is_input)
    {
      *channel = g_io_channel_unix_new(stdpipe[0]);
      *fd = stdpipe[1];

      *mirror = g_io_channel_unix_new(stdpipe[1]);
    }
  else
    {
      *channel = g_io_channel_unix_new(stdpipe[1]);
      *fd = stdpipe[0];

      *mirror = g_io_channel_unix_new(stdpipe[0]);
    }

  /* if the channel is buffered, we may end up not seeing some output
   * for a long time */
  g_io_channel_set_encoding(*channel, NULL, NULL);
  g_io_channel_set_buffered(*channel, FALSE);
}

static gchar* read_all_from_channel(GIOChannel *channel, gsize *length)
{
  GError *error = NULL;
  GString *retstring;
  gchar *retdata;
  gchar buffer[1024];
  gsize buffer_length = -1;

  retstring = g_string_new("");

  while(buffer_length != 0)
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

  return retdata;
}

static void
gksu_process_close_server_fd(GksuProcess *self, guint fd)
{
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);
  GError *error = NULL;
  
  dbus_g_proxy_call(priv->server, "CloseFD", &error,
                    G_TYPE_UINT, priv->cookie,
                    G_TYPE_INT, fd,
                    G_TYPE_INVALID,
                    G_TYPE_INVALID);

  if(error)
    {
      g_warning("%s", error->message);
      g_error_free(error);
    }
}

static gboolean
gksu_process_stdin_mirror_hangup_cb(GIOChannel *channel, GIOCondition condition,
                                    GksuProcess *self)
{
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);
  GError *error = NULL;

  if((condition == G_IO_HUP) || (condition == G_IO_NVAL))
    {
      gksu_process_close_server_fd(self, 0);
      g_source_remove(priv->stdin_source_id);
      g_io_channel_shutdown(priv->stdin_channel, TRUE, &error);
      if(error)
        {
          g_warning("%s", error->message);
          g_error_free(error);
        }
    }

  return FALSE;
}

static gboolean
gksu_process_stdout_mirror_hangup_cb(GIOChannel *channel, GIOCondition condition,
                                     GksuProcess *self)
{
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);
  GError *error = NULL;

  if((condition == G_IO_HUP) || (condition == G_IO_NVAL))
    {
      gksu_process_close_server_fd(self, 1);
      g_io_channel_shutdown(priv->stdout_channel, TRUE, &error);
      if(error)
        {
          g_warning("%s", error->message);
          g_error_free(error);
        }
    }

  return FALSE;
}

static gboolean
gksu_process_stderr_mirror_hangup_cb(GIOChannel *channel, GIOCondition condition,
                                     GksuProcess *self)
{
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);
  GError *error = NULL;

  if((condition == G_IO_HUP) || (condition == G_IO_NVAL))
    {
      gksu_process_close_server_fd(self, 2);
      g_io_channel_shutdown(priv->stderr_channel, TRUE, &error);
      if(error)
        {
          g_warning("%s", error->message);
          g_error_free(error);
        }
    }

  return FALSE;
}

static gboolean
gksu_process_stdin_ready_to_send_cb(GIOChannel *channel, GIOCondition condition,
                                    GksuProcess *self)
{
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);
  gsize length;
  gchar *data;
  GError *error = NULL;

  data = read_all_from_channel(channel, &length);
  dbus_g_proxy_call(priv->server, "WriteInput", &error,
                    G_TYPE_UINT, priv->cookie,
                    G_TYPE_STRING, data,
                    G_TYPE_UINT, length,
                    G_TYPE_INVALID,
                    G_TYPE_INVALID);
  g_free(data);
  return TRUE;
}

/**
 * gksu_process_new
 * @working_directory: directory path
 * @arguments: %NULL-terminated array of strings
 *
 * This function creates a new #GksuProcess object, which can be used
 * to launch a process as the root user (uid 0). The process is
 * started with the given directory path as its working directory. The
 * @arguments array must have the command to be executed at its first
 * position, followed by the command's arguments; it must also contain
 * a %NULL at its last position.
 * 
 * Returns: a new instance of #GksuProcess
 */
GksuProcess*
gksu_process_new(const gchar *working_directory, const gchar **arguments)
{
  GksuProcess *self = g_object_new(GKSU_TYPE_PROCESS, NULL);
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);

  priv->working_directory = g_strdup(working_directory);
  priv->arguments = g_strdupv((gchar**)arguments);

  return self;
}

/**
 * gksu_process_spawn_async_with_pipes
 * @self: a #GksuProcess instance
 * @standard_input: return location for file descriptor to write to child's
 * stdin, or %NULL
 * @standard_output: return location for file descriptor to write to child's
 * stdout, or %NULL
 * @standard_error: return location for file descriptor to write to child's
 * stderr, or %NULL
 * @error: return location for a #GError
 *
 * Creates the process with the information stored in the
 * #GksuProcess. If you pass the pointers to integers to the
 * @standard_input, @standard_output and @standard_error parameters
 * they will be set to the corresponding file descriptors of the
 * child; the child standard I/O channels will be essentially disabled
 * for the ones to which %NULL is given.
 *
 * This function return immediately after the process has been
 * created. You need to connect to the GksuProcess::exited signal to
 * know that the process has ended and get its exit status.
 *
 * Notice that some caveats exist in how the input and output are
 * handled. Gksu PolicyKit uses a D-Bus service to do the actual
 * running of the program, and all the input must be sent to and all
 * the output must be received from this service, through D-Bus. The
 * library handles this, but it needs a glib main loop for that. This
 * means that if you keep the mainloop from running by using a loop to
 * read the standard output, for example, you may end up not having
 * anything to read.
 *
 * Returns: %FALSE if @error is set, %TRUE if all went well
 */
gboolean
gksu_process_spawn_async_with_pipes(GksuProcess *self, gint *standard_input,
                                    gint *standard_output, gint *standard_error,
                                    GError **error)
{
  GError *internal_error = NULL;
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);

  GHashTable *environment;
  gchar *xauth = get_xauth_token(NULL);
  gint pid;
  guint32 cookie;

  environment = gksu_environment_get_variables(priv->environment);
  dbus_g_proxy_call(priv->server, "Spawn", &internal_error,
                    G_TYPE_STRING, priv->working_directory,
                    G_TYPE_STRING, xauth,
                    G_TYPE_STRV, priv->arguments,
                    DBUS_TYPE_G_STRING_STRING_HASHTABLE, environment,
                    G_TYPE_BOOLEAN, standard_input != NULL,
                    G_TYPE_BOOLEAN, standard_output != NULL,
                    G_TYPE_BOOLEAN, standard_error != NULL,
                    G_TYPE_INVALID,
                    G_TYPE_INT, &pid,
                    G_TYPE_UINT, &cookie,
                    G_TYPE_INVALID);
  g_hash_table_destroy(environment);
  g_free(xauth);

  if(internal_error)
    {
      if(g_str_has_prefix(internal_error->message, "auth_"))
	{
	  DBusError dbus_error;
	  dbus_error_init(&dbus_error);
	  if (polkit_auth_obtain("org.gnome.gksu.spawn",
                                 0, getpid(), &dbus_error))
            return gksu_process_spawn_async_with_pipes(self, standard_input,
                                                       standard_output, standard_error,
                                                       error);
	}
      else
        g_propagate_error(error, internal_error);

      return FALSE;
    }

  priv->pid = pid;
  priv->cookie = cookie;

  if(standard_input)
    {
      gksu_process_prepare_pipe(&(priv->stdin_channel),
                                &(priv->stdin_mirror),
                                priv->stdin,
                                standard_input,
                                TRUE);

      priv->stdin_source_id = 
        g_io_add_watch(priv->stdin_channel, G_IO_IN|G_IO_PRI,
                       (GIOFunc)gksu_process_stdin_ready_to_send_cb,
                       (gpointer)self);

      priv->stdin_mirror_id = 
        g_io_add_watch(priv->stdin_mirror, G_IO_HUP|G_IO_NVAL,
                       (GIOFunc)gksu_process_stdin_mirror_hangup_cb,
                       (gpointer)self);

    }

  if(standard_output)
    {
      gksu_process_prepare_pipe(&(priv->stdout_channel),
                                &(priv->stdout_mirror),
                                priv->stdout,
                                standard_output,
                                FALSE);

      priv->stdout_mirror_id =
        g_io_add_watch(priv->stdout_mirror, G_IO_HUP|G_IO_NVAL,
                       (GIOFunc)gksu_process_stdout_mirror_hangup_cb,
                       (gpointer)self);

      priv->stdout_write_queue =
        gksu_write_queue_new(priv->stdout_channel);
    }

  if(standard_error)
    {
      gksu_process_prepare_pipe(&(priv->stderr_channel),
                                &(priv->stderr_mirror),
                                priv->stderr,
                                standard_error,
                                FALSE);

      priv->stderr_mirror_id =
        g_io_add_watch(priv->stderr_mirror, G_IO_HUP|G_IO_NVAL,
                       (GIOFunc)gksu_process_stderr_mirror_hangup_cb,
                       (gpointer)self);

      priv->stderr_write_queue =
        gksu_write_queue_new(priv->stderr_channel);
    }

  return TRUE;
}

/**
 * gksu_process_spawn_async
 * @self: a #GksuProcess instance
 * @error: return location for a #GError
 *
 * Creates the process with the information stored in the
 * #GksuProcess. This function return immediately after the process
 * has been created. You need to connect to the GksuProcess::exited
 * signal to know that the process has ended and get its exit status.
 *
 * Calling this method has the same effect of calling
 * gksu_process_spawn_async_with_pipes() with all the file descriptor
 * pointers as %NULL.
 *
 * Returns: %FALSE if @error is set, %TRUE if all went well
 */
gboolean
gksu_process_spawn_async(GksuProcess *self, GError **error)
{
  return gksu_process_spawn_async_with_pipes(self, NULL, NULL, NULL, error);
}

typedef struct
{
  GMainLoop *loop;
  gint status;
} SyncRunInfo;

static void
sync_handle_exited(GksuProcess *self, gint status, SyncRunInfo *sri)
{
  sri->status = status;
  g_main_loop_quit(sri->loop);
}

/**
 * gksu_process_spawn_async
 * @self: a #GksuProcess instance
 * @status: return location for the child's exit status, as returned
 * by waitpid(2)
 * @error: return location for a #GError
 *
 * Creates the process with the information stored in the
 * #GksuProcess. This function will only return after the child
 * process has finished.
 *
 * Notice that, internally, this function runs the main loop, so even
 * though this function will not return, idles, IO watches, timeouts,
 * and event handlers may be called while the child is not yet
 * finished.
 *
 * Returns: %FALSE if @error is set, %TRUE if all went well
 */
gboolean
gksu_process_spawn_sync(GksuProcess *self, gint *status, GError **error)
{
  SyncRunInfo sri;
  GError *internal_error = NULL;
  gboolean retval;
  gulong signal_id;

  retval = gksu_process_spawn_async(self, &internal_error);
  if(internal_error)
    {
      g_propagate_error(error, internal_error);
      return FALSE;
    }

  sri.loop = g_main_loop_new(NULL, FALSE);
  signal_id = g_signal_connect(G_OBJECT(self), "exited",
                               G_CALLBACK(sync_handle_exited),
                               (gpointer)&sri);
  g_main_loop_run(sri.loop);

  g_main_loop_unref(sri.loop);

  g_signal_handler_disconnect(self, signal_id);

  if(status != NULL)
    *status = sri.status;

  return retval;
}
