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

int main(int argc, char **argv)
{
  GksuProcess *process;
  gchar *args[] = { "/usr/bin/xterm" , "/usr/bin/xterm", NULL };
  GError *error = NULL;

  g_type_init();

  process = gksu_process_new("/home/kov", (const gchar**)args);
  gksu_process_spawn_async(process, &error);

  return 0;
}
