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
#include <gksu-process.h>

void process_exited_cb(GksuProcess *self, gint status, GMainLoop *loop)
{
  g_warning("Status: %d\n", status);
  while(g_main_context_pending(NULL))
    g_main_context_iteration(NULL, FALSE);
  g_main_loop_quit(loop);
}

void output_received (GIOChannel *channel,
                      GIOCondition condition,
                      gpointer dara)
{
  GError *error = NULL;
  GString *retstring;
  gchar buffer[1024];
  gsize length = -1;

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

  printf("%s", retstring->str);
  g_string_free(retstring, TRUE);

}

int main(int argc, char **argv)
{
  GMainLoop *loop;
  GksuProcess *process;
  gchar *args[] = { "/home/kov/bin/gksupktest" , "/home/kov/bin/gksupktest", NULL };
  GError *error = NULL;
  gint stdout;
  GIOChannel *stdout_channel;
  gint stderr;
  GIOChannel *stderr_channel;

  g_type_init();

  process = gksu_process_new("/home/kov", (const gchar**)args);
  loop = g_main_loop_new(NULL, TRUE);
  g_signal_connect(process, "exited", G_CALLBACK(process_exited_cb), (gpointer)loop);
  gksu_process_spawn_async_with_pipes(process, NULL, &stdout, &stderr, &error);
  if(error)
    {
      g_warning("Error: %s\n", error->message);
      return 1;
    }

  stdout_channel = g_io_channel_unix_new(stdout);
  g_io_channel_set_encoding(stdout_channel, NULL, NULL);
  g_io_add_watch(stdout_channel, G_IO_IN|G_IO_PRI,
                 (GIOFunc)output_received,
                 NULL);

  stderr_channel = g_io_channel_unix_new(stderr);
  g_io_channel_set_encoding(stderr_channel, NULL, NULL);
  g_io_add_watch(stderr_channel, G_IO_IN|G_IO_PRI,
                 (GIOFunc)output_received,
                 NULL);

  g_main_loop_run(loop);

  return 0;
}
