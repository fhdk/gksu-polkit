/*
 *  Copyright (C) 2008 Gustavo Noronha Silva
 *
 *  This file is part of gksu-policykit.
 *
 *  gksu-policykit is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  gksu-policykit is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with gksu-policykit.  If not, see
 *  <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <wait.h>

#include <gksu-process.h>
#include <gksu-write-queue.h>

gint retval;

void process_exited_cb(GksuProcess *self, gint status, GMainLoop *loop)
{
  //  g_warning("Status: %d\n", status);
  while(g_main_context_pending(NULL))
    g_main_context_iteration(NULL, FALSE);
  g_main_loop_quit(loop);
  retval = WEXITSTATUS(status);
}

gboolean output_received (GIOChannel *channel,
                          GIOCondition condition,
                          gpointer dara)
{
  GError *error = NULL;
  GString *retstring;
  gchar buffer[1024];
  gsize length = -1;

  if((condition == G_IO_NVAL) || (condition == G_IO_HUP))
    {
      return FALSE;
    }

  retstring = g_string_new("");

  while(length != 0)
    {
      GIOStatus status;
      status = g_io_channel_read_chars(channel, buffer, 1024, &length, &error);
      if(error)
        {
          fprintf(stderr, "%s\n", error->message);
          g_error_free(error);
        }
      g_string_append_len(retstring, buffer, length);
    }

  write(1, retstring->str, retstring->len);
  g_string_free(retstring, TRUE);

  return TRUE;
}

gboolean input_received (GIOChannel *channel,
                         GIOCondition condition,
                         GksuWriteQueue *queue)
{
  GError *error = NULL;
  GString *retstring;
  gchar buffer[1024];
  gsize length = -1;

  if((condition == G_IO_HUP) || (condition == G_IO_NVAL))
    {
      gksu_write_queue_shutdown(queue, TRUE);
      return FALSE;
    }

  retstring = g_string_new("");

  while(length != 0)
    {
      GIOStatus status;
      status = g_io_channel_read_chars(channel, buffer, 1024, &length, &error);
      if(error)
        {
          fprintf(stderr, "%s\n", error->message);
          g_error_free(error);
        }
      g_string_append_len(retstring, buffer, length);
    }

  gksu_write_queue_add(queue, retstring->str, retstring->len);
  g_string_free(retstring, TRUE);

  return TRUE;
}

int main(int argc, char **argv)
{
  GMainLoop *loop;
  GksuProcess *process;
  gchar **args;
  gint count;
  GError *error = NULL;
  gint stdin_fd;
  GIOChannel *stdin_channel;
  GIOChannel *process_stdin_channel;
  GksuWriteQueue *stdin_write_queue;
  gint stdout_fd;
  GIOChannel *stdout_channel;
  gint stderr_fd;
  GIOChannel *stderr_channel;
  gchar *cwd;

  retval = 0;

  g_type_init();

  args = (gchar**)g_malloc(sizeof(gchar*));
  for(count = 0; count < argc; count++)
    {
      args = (gchar**)g_realloc(args, sizeof(gchar*)*(count+2));
      args[count] = g_strdup(argv[count+1]);
    }
  args[count] = NULL;

  cwd = g_get_current_dir();
  process = gksu_process_new(cwd, (const gchar**)args);
  g_free(cwd);

  loop = g_main_loop_new(NULL, TRUE);
  g_signal_connect(process, "exited", G_CALLBACK(process_exited_cb), (gpointer)loop);
  gksu_process_spawn_async_with_pipes(process, &stdin_fd, &stdout_fd, &stderr_fd, &error);
  if(error)
    {
      g_warning("Error: %s\n", error->message);
      return 1;
    }

  process_stdin_channel = g_io_channel_unix_new(stdin_fd);
  g_io_channel_set_encoding(process_stdin_channel, NULL, NULL);
  g_io_channel_set_buffered(process_stdin_channel, FALSE);
  stdin_write_queue = gksu_write_queue_new(process_stdin_channel);

  stdin_channel = g_io_channel_unix_new(0);
  g_io_channel_set_encoding(stdin_channel, NULL, NULL);
  g_io_channel_set_buffered(stdin_channel, FALSE);
  g_io_add_watch(stdin_channel, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_NVAL,
                 (GIOFunc)input_received,
                 stdin_write_queue);

  stdout_channel = g_io_channel_unix_new(stdout_fd);
  g_io_channel_set_encoding(stdout_channel, NULL, NULL);
  g_io_add_watch(stdout_channel, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_NVAL,
                 (GIOFunc)output_received,
                 NULL);

  stderr_channel = g_io_channel_unix_new(stderr_fd);
  g_io_channel_set_encoding(stderr_channel, NULL, NULL);
  g_io_add_watch(stderr_channel, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_NVAL,
                 (GIOFunc)output_received,
                 NULL);

  g_main_loop_run(loop);

  return retval;
}
