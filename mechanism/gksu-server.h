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

#ifndef __GKSU_SERVER_H__
#define __GKSU_SERVER_H__ 1

#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <polkit-dbus/polkit-dbus.h>

typedef struct _GksuServerPrivate GksuServerPrivate;

typedef struct {
  GObject parent;
  GksuServerPrivate *priv;
} GksuServer;

typedef struct {
  GObjectClass parent;
} GksuServerClass;

GType gksu_server_get_type(void);

#define GKSU_TYPE_SERVER (gksu_server_get_type())
#define GKSU_SERVER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GKSU_TYPE_SERVER, GksuServer))
#define GKSU_SERVER_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS ((object), GKSU_TYPE_SERVER, GksuServerClass))

gboolean gksu_server_spawn(GksuServer *self, gchar *cwd, gchar **args, GHashTable *environment, gint *pid, GError **error);

#endif
