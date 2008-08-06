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

#include <stdlib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>

#include <gksu-environment.h>
#include "gksu-process.h"

G_DEFINE_TYPE(GksuProcess, gksu_process, G_TYPE_OBJECT);

struct _GksuProcessPrivate {
  DBusGConnection *dbus;
  DBusGProxy *server;
  gchar *working_directory;
  gchar **arguments;
  GksuEnvironment *environment;
};

#define GKSU_PROCESS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GKSU_TYPE_PROCESS, GksuProcessPrivate))

static void gksu_process_finalize(GObject *object)
{
  GksuProcess *self = GKSU_PROCESS(object);
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);

  g_free(priv->working_directory);
  g_strfreev(priv->arguments);
  g_object_unref(priv->environment);

  G_OBJECT_CLASS(gksu_process_parent_class)->finalize(object);
}

static void gksu_process_class_init(GksuProcessClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = gksu_process_finalize;

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

  priv->environment = gksu_environment_new();
}

GksuProcess*
gksu_process_new(const gchar *working_directory, const gchar **arguments)
{
  GksuProcess *self = g_object_new(GKSU_TYPE_PROCESS, NULL);
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);

  priv->working_directory = g_strdup(working_directory);
  priv->arguments = g_strdupv((gchar**)arguments);

  return self;
}

gboolean
gksu_process_spawn_async(GksuProcess *self, GError **error)
{
  GError *internal_error = NULL;
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);

  GHashTable *environment;
  gint pid;

  environment = gksu_environment_get_variables(priv->environment);
  dbus_g_proxy_call(priv->server, "Spawn", &internal_error,
                    G_TYPE_STRING, priv->working_directory,
                    G_TYPE_STRV, priv->arguments,
                    DBUS_TYPE_G_STRING_STRING_HASHTABLE, environment,
                    G_TYPE_INVALID,
                    G_TYPE_INT, &pid,
                    G_TYPE_INVALID);
  g_hash_table_destroy(environment);

  if(internal_error)
    {
      if(g_str_has_prefix(internal_error->message, "auth_"))
	{
	  DBusError dbus_error;
	  dbus_error_init(&dbus_error);
	  if (polkit_auth_obtain("org.gnome.gksu.spawn",
                                 0, getpid(), &dbus_error))
            return gksu_process_spawn_async(self, error);
	}
      else
        g_warning("%s\n", internal_error->message);
      return FALSE;
    }

  return TRUE;
}
