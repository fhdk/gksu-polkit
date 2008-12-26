/*
 * Copyright (C) 2008 Gustavo Noronha Silva
 *
 * This file is part of the Gksu PolicyKit Mechanism.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.  You should have received
 * a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307, USA.
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

  void (* process_exited) (GksuController *controller, gint status);
} GksuControllerClass;

GType gksu_controller_get_type(void);

#define GKSU_TYPE_CONTROLLER (gksu_controller_get_type())
#define GKSU_CONTROLLER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GKSU_TYPE_CONTROLLER, GksuController))
#define GKSU_IS_CONTROLLER(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GKSU_TYPE_CONTROLLER))
#define GKSU_CONTROLLER_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS ((object), GKSU_TYPE_CONTROLLER, GksuControllerClass))

GksuController* gksu_controller_new(gchar *working_directory, gchar *xauth, gchar **arguments,
                                    GHashTable *environment, DBusGConnection *dbus,
                                    gboolean using_stdin, gboolean using_stdout,
                                    gboolean using_stderr, gint *pid, GError **error);

void gksu_controller_finish(GksuController *controller);

gint gksu_controller_get_pid(GksuController *controller);

void gksu_controller_set_cookie(GksuController *controller, guint32 cookie);

gint gksu_controller_get_cookie(GksuController *controller);

void gksu_controller_close_fd(GksuController *self, gint fd, GError **error);

gchar* gksu_controller_read_output(GksuController *self, gint fd,
                                   gsize *length, gboolean read_to_end);

gboolean gksu_controller_write_input(GksuController *self, const gchar *data,
                                     const gsize length, GError **error);

gboolean gksu_controller_is_using_stdout(GksuController *self);

gboolean gksu_controller_is_using_stderr(GksuController *self);

#endif
