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

#ifndef __GKSU_CONTROLLER_H__
#define __GKSU_CONTROLLER_H__ 1

#include <glib-object.h>

typedef struct _GksuControllerPrivate GksuControllerPrivate;

typedef struct {
  GObject parent;
  GksuControllerPrivate *priv;
} GksuController;

typedef struct {
  GObjectClass parent;
} GksuControllerClass;

GType gksu_controller_get_type(void);

#define GKSU_TYPE_CONTROLLER (gksu_controller_get_type())
#define GKSU_CONTROLLER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GKSU_TYPE_CONTROLLER, GksuController))
#define GKSU_CONTROLLER_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS ((object), GKSU_TYPE_CONTROLLER, GksuControllerClass))

GksuController* gksu_controller_new(gchar *working_directory, gchar **arguments,
                                    GHashTable *environment, DBusGConnection *dbus,
                                    gint *pid, GError **error);

#endif