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
#include "gksu-controller.h"

G_DEFINE_TYPE(GksuController, gksu_controller, G_TYPE_OBJECT);

struct _GksuControllerPrivate {
  DBusGConnection *dbus;
  gchar *working_directory;
  gchar **arguments;
};

#define GKSU_CONTROLLER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GKSU_TYPE_CONTROLLER, GksuControllerPrivate))

static void gksu_controller_finalize(GObject *object)
{
  GksuController *self = GKSU_CONTROLLER(object);
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);

  g_free(priv->working_directory);
  g_strfreev(priv->arguments);

  G_OBJECT_CLASS(gksu_controller_parent_class)->finalize(object);
}

static void gksu_controller_class_init(GksuControllerClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = gksu_controller_finalize;

  g_type_class_add_private(klass, sizeof(GksuControllerPrivate));
}

static void gksu_controller_init(GksuController *self)
{
}

GksuController* gksu_controller_new(gchar *working_directory, gchar **arguments,
                                    GHashTable *environment, DBusGConnection *dbus,
                                    gint *pid, GError **error)
{
  GksuController *self = g_object_new(GKSU_TYPE_CONTROLLER, NULL);
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);
  GList *keys = g_hash_table_get_keys(environment);
  GList *iter = keys;
  gchar **environmentv = g_malloc(sizeof(gchar**));
  gint size = 0;
  GError *internal_error = NULL;

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

  g_spawn_async(working_directory, arguments, environmentv, G_SPAWN_FILE_AND_ARGV_ZERO,
                NULL, NULL, pid, &internal_error);

  g_strfreev(environmentv);

  if(internal_error)
    {
      g_warning("%s\n", internal_error->message);
      g_error_free(internal_error);
      return NULL;
    }

  priv->working_directory = g_strdup(working_directory);
  priv->arguments = g_strdupv(arguments);
  priv->dbus = dbus;

  return self;
}
