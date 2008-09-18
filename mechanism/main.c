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
#include <stdlib.h>
#include <string.h>

#include "gksu-server.h"

void server_shutdown_cb(GksuServer *server, GMainLoop *loop)
{
  g_main_loop_quit(loop);
}

int main (int argc, char ** argv)
{
  GMainLoop *loop;
  GksuServer *server;

  g_type_init ();
  
  loop = g_main_loop_new(NULL, TRUE);
  server = g_object_new(GKSU_TYPE_SERVER, NULL);
  g_signal_connect(G_OBJECT(server), "shutdown",
                   G_CALLBACK(server_shutdown_cb), (gpointer)loop);
  g_main_loop_run(loop);

  return 0;
}




