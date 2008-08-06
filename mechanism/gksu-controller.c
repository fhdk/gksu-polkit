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
#include <sys/types.h>
#include <sys/wait.h>

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
  gint pid;
};

#define GKSU_CONTROLLER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GKSU_TYPE_CONTROLLER, GksuControllerPrivate))

enum {
  PROCESS_EXITED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

static void gksu_controller_real_process_exited(GksuController *controller, gint status)
{
  /* take out our own reference */
  g_object_unref(controller);
}

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

  signals[PROCESS_EXITED] = g_signal_new("process-exited",
                                         GKSU_TYPE_CONTROLLER,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET(GksuControllerClass, process_exited),
                                         NULL,
                                         NULL,
                                         g_cclosure_marshal_VOID__INT,
                                         G_TYPE_NONE, 1,
                                         G_TYPE_INT);

  klass->process_exited = gksu_controller_real_process_exited;

  g_type_class_add_private(klass, sizeof(GksuControllerPrivate));
}

static void gksu_controller_init(GksuController *self)
{
}

static void gksu_controller_process_exited_cb(GPid pid, gint status, GksuController *self)
{
  g_signal_emit(self, signals[PROCESS_EXITED], 0, status);
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

  g_spawn_async(working_directory, arguments, environmentv,
                G_SPAWN_FILE_AND_ARGV_ZERO|G_SPAWN_DO_NOT_REAP_CHILD,
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
  priv->pid = *pid;

  g_child_watch_add(priv->pid,
                    (GChildWatchFunc)gksu_controller_process_exited_cb,
                    (gpointer)self);

  return self;
}

void gksu_controller_finish(GksuController *controller)
{
}

gint gksu_controller_get_pid(GksuController *self)
{
  GksuControllerPrivate *priv = GKSU_CONTROLLER_GET_PRIVATE(self);
  return priv->pid;
}
