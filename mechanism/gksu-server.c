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

#include <stdio.h>
#include <stdlib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <polkit-dbus/polkit-dbus.h>

#include <gksu-error.h>
#include <gksu-marshal.h>

#include "gksu-controller.h"
#include "gksu-server.h"
#include "gksu-server-service-glue.h"

static void gksu_server_init(GksuServer *self);
static void gksu_server_class_init(GksuServerClass *klass);
DBusHandlerResult gksu_server_handle_dbus_message(DBusConnection *conn,
                                                  DBusMessage *message,
                                                  void *user_data);

G_DEFINE_TYPE(GksuServer, gksu_server, G_TYPE_OBJECT);

struct _GksuServerPrivate {
  DBusGConnection *dbus;
  PolKitContext *pk_context;
  PolKitTracker *pk_tracker;
  GHashTable *controllers;
  GHashTable *zombies;

  guint shutdown_source_id;
};

#define GKSU_SERVER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GKSU_TYPE_SERVER, GksuServerPrivate))

enum {
  PROCESS_EXITED,
  OUTPUT_AVAILABLE,
  SHUTDOWN,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

#define ZOMBIE_CHECK_DELAY 300
#define ZOMBIE_AGE_THRESHOLD 10

typedef struct {
  gint status;
  gint age;

  gchar *pending_stdout;
  gsize pending_stdout_length;

  gchar *pending_stderr;
  gsize pending_stderr_length;
} GksuZombie;

static void gksu_server_finalize(GObject *object)
{
  GksuServer *self = GKSU_SERVER(object);
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);

  g_hash_table_destroy(priv->controllers);
  g_hash_table_destroy(priv->zombies);

  G_OBJECT_CLASS(gksu_server_parent_class)->finalize(object);
}

static void gksu_server_class_init(GksuServerClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = gksu_server_finalize;

  signals[PROCESS_EXITED] =
    g_signal_new("process-exited",
                 GKSU_TYPE_SERVER,
                 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                 0,
                 NULL,
                 NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1,
                 G_TYPE_INT);

  signals[OUTPUT_AVAILABLE] =
    g_signal_new("output-available",
                 GKSU_TYPE_SERVER,
                 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                 0,
                 NULL,
                 NULL,
                 gksu_marshal_VOID__INT_INT,
                 G_TYPE_NONE, 2,
                 G_TYPE_INT,
                 G_TYPE_INT);

  /* not a dbus signal; just to signal that we want to quit */
  signals[SHUTDOWN] =
    g_signal_new("shutdown",
                 GKSU_TYPE_SERVER,
                 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                 0,
                 NULL,
                 NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  g_type_class_add_private(klass, sizeof(GksuServerPrivate));
}

static void gksu_server_process_exited_cb(GksuController *controller, gint status,
                                          GksuServer *self)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);
  GksuZombie *zombie = g_new(GksuZombie, 1);
  gint pid;
  guint32 cookie;
  gsize length;

  pid = gksu_controller_get_pid(controller);
  cookie = gksu_controller_get_cookie(controller);
  g_hash_table_remove(priv->controllers, GINT_TO_POINTER(cookie));

  zombie->status = status;
  zombie->age = 0;

  /* we might get a message for this, still, so we keep it */
  if(gksu_controller_is_using_stdout(controller))
    {
      zombie->pending_stdout = gksu_controller_read_output(controller, 1, &length, TRUE);
      zombie->pending_stdout_length = length;
    }
  else
    zombie->pending_stdout = NULL;

  if(gksu_controller_is_using_stderr(controller))
    {
      zombie->pending_stderr = gksu_controller_read_output(controller, 2, &length, TRUE);
      zombie->pending_stderr_length = length;
    }
    zombie->pending_stderr = NULL;

  g_hash_table_replace(priv->zombies, GINT_TO_POINTER(cookie), zombie);

  g_signal_emit(self, signals[PROCESS_EXITED], 0, pid);
}

static void gksu_server_output_available_cb(GksuController *controller, gint fd, GksuServer *self)
{
  gint pid;
  pid = gksu_controller_get_pid(controller);
  g_signal_emit(self, signals[OUTPUT_AVAILABLE], 0, pid, fd);
}

static void pk_config_changed_cb(PolKitContext *pk_context, gpointer user_data)
{
  polkit_context_force_reload(pk_context);
}

static gboolean gksu_server_handle_zombies(GksuServer *self)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);

  GList *zombies = g_hash_table_get_keys(priv->zombies);
  GksuZombie *zombie;
  guint32 cookie;

  while(zombies != NULL)
    {
      cookie = GPOINTER_TO_INT(zombies->data);
      zombie = (GksuZombie*)g_hash_table_lookup(priv->zombies, zombies->data);

      if(zombie->age > ZOMBIE_AGE_THRESHOLD)
        gksu_server_wait(self, cookie, NULL, NULL);
      else
        zombie->age++;

      zombies = zombies->next;
    }

  return TRUE;
}

static void gksu_server_init(GksuServer *self)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);

  PolKitContext *pk_context = NULL;
  PolKitTracker *pk_tracker = NULL;
  PolKitError *pk_error = NULL;
  DBusConnection *connection;
  DBusError dbus_error;
  GError *error = NULL;

  self->priv = priv;

  /* Basic DBus setup */
  priv->dbus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  if(error)
    {
      g_error(error->message);
      exit(1);
    }
  connection = dbus_g_connection_get_connection(priv->dbus);

  /* object exposure to DBus */
  dbus_g_object_type_install_info(GKSU_TYPE_SERVER,
				  &dbus_glib_gksu_server_object_info);

  dbus_g_connection_register_g_object(priv->dbus,
				      "/org/gnome/Gksu",
				      G_OBJECT(self));

  dbus_error_init(&dbus_error);
  if(dbus_bus_request_name(connection,
			   "org.gnome.Gksu",
			   0, &dbus_error) == -1)
    {
      g_error("Failed to get well-known name:\n %s: %s\n",
	      dbus_error.name, dbus_error.message);
      exit(1);
    }

  /* Monitoring signals that are important for ourselves */
  dbus_bus_add_match (connection,
		      "type='signal'"
		      ",interface='"DBUS_INTERFACE_DBUS"'"
		      ",sender='"DBUS_SERVICE_DBUS"'"
		      ",member='NameOwnerChanged'",
		      &dbus_error);

  dbus_bus_add_match (connection,
		      "type='signal',sender='org.freedesktop.ConsoleKit'",
		      &dbus_error);

  /* PolicyKit setup */
  pk_context = polkit_context_new();
  polkit_context_set_config_changed(pk_context, pk_config_changed_cb, NULL);
  polkit_context_init(pk_context, &pk_error);
  if(polkit_error_is_set(pk_error))
    {
      g_error("polkit_context_init failed: %s: %s\n",
	      polkit_error_get_error_name(pk_error),
	      polkit_error_get_error_message(pk_error));
    }
  priv->pk_context = pk_context;

  pk_tracker = polkit_tracker_new();
  polkit_tracker_set_system_bus_connection(pk_tracker, connection);
  polkit_tracker_init(pk_tracker);
  priv->pk_tracker = pk_tracker;

  /* Setup our main filter to handle the DBus messages */
  dbus_connection_add_filter(connection, gksu_server_handle_dbus_message,
			     (void*)self, NULL);

  /* "properties" */
  priv->controllers = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_object_unref);
  priv->zombies = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

  /* monitor zombies */
  g_timeout_add_seconds(ZOMBIE_CHECK_DELAY,
                        (GSourceFunc)gksu_server_handle_zombies,
                        (gpointer)self);
}

static PolKitCaller* gksu_server_get_caller_from_message(GksuServer *self,
                                                         DBusMessage *message)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);

  PolKitTracker *pk_tracker = priv->pk_tracker;
  PolKitCaller *pk_caller = NULL;
  DBusError dbus_error;
  const gchar *sender = NULL;

  dbus_error_init(&dbus_error);

  sender = dbus_message_get_sender(message);
  pk_caller = polkit_tracker_get_caller_from_dbus_name(pk_tracker,
						       sender,
						       &dbus_error);
  if(pk_caller == NULL)
    {
      if(dbus_error_is_set(&dbus_error))
	{
	  g_error("Failed to get caller from dbus: %s: %s\n",
		  dbus_error.name, dbus_error.message);
	  return NULL;
	}
    }

  return pk_caller;
}

static gboolean gksu_server_is_message_spawn_related(DBusMessage *message)
{
  return (dbus_message_is_method_call(message, "org.gnome.Gksu", "Spawn"));
}

DBusHandlerResult gksu_server_handle_dbus_message(DBusConnection *conn,
                                                  DBusMessage *message,
                                                  void *user_data)
{
  GksuServer *self = GKSU_SERVER(user_data);
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);

  DBusGConnection *dbus = priv->dbus;
  DBusConnection *connection = dbus_g_connection_get_connection(dbus);
  PolKitContext *pk_context = priv->pk_context;
  PolKitCaller *pk_caller = NULL;
  PolKitAction *pk_action = NULL;
  PolKitResult pk_result;
  PolKitError *pk_error = NULL;

  if(gksu_server_is_message_spawn_related(message))
    {
      /*
       * hmmm... my current authorization model needs this to work
       * correctly; we only use the cache if it works, so that other
       * calls requesting aditional information are cached
       */
      polkit_context_force_reload(pk_context);

      pk_caller = gksu_server_get_caller_from_message(self, message);
      if(pk_caller != NULL)
        {
	  pid_t caller_pid;
	  uid_t caller_uid;

	  polkit_caller_get_pid(pk_caller, &caller_pid);
	  polkit_caller_get_uid(pk_caller, &caller_uid);
        }

      pk_action = polkit_action_new();
      polkit_action_set_action_id(pk_action, "org.gnome.gksu.spawn");

      pk_result = polkit_context_is_caller_authorized(pk_context,
						      pk_action,
						      pk_caller,
						      TRUE,
						      &pk_error);

      if(pk_result != POLKIT_RESULT_YES)
	{
	  DBusMessage *reply;
	  const gchar *pk_result_str = 
	    polkit_result_to_string_representation(pk_result);

	  reply = dbus_message_new_error(message,
					 DBUS_ERROR_FAILED,
					 pk_result_str);
	  dbus_connection_send(connection, reply, NULL);
	  dbus_message_unref(reply);

	  return DBUS_HANDLER_RESULT_HANDLED;
	}
    }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

gboolean gksu_server_spawn(GksuServer *self, gchar *cwd, gchar *xauth, gchar **args,
                           GHashTable *environment, gboolean using_stdin, gboolean using_stdout,
                           gboolean using_stderr, gint *pid, guint32 *cookie, GError **error)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);

  GksuController *controller;
  GError *internal_error = NULL;
  guint32 random_number;

  controller = gksu_controller_new(cwd, xauth, args, environment, priv->dbus,
                                   using_stdin, using_stdout, using_stderr,
                                   pid, &internal_error);
  if(internal_error)
    {
      g_propagate_error(error, internal_error);
      return FALSE;
    }

  g_signal_connect(controller, "process-exited",
                   G_CALLBACK(gksu_server_process_exited_cb),
                   self);

  g_signal_connect(controller, "output-available",
                   G_CALLBACK(gksu_server_output_available_cb),
                   self);

  do
    {
      random_number = g_random_int();
    } while (g_hash_table_lookup(priv->controllers, GINT_TO_POINTER(random_number)));

  g_object_ref(controller);

  /* set the cookie for the controller to know, and also send it back
   * to the caller */
  gksu_controller_set_cookie(controller, random_number);
  *cookie = random_number;

  g_hash_table_replace(priv->controllers, GINT_TO_POINTER(random_number), controller);

  return TRUE;
}

gboolean gksu_server_close_fd(GksuServer *self, guint32 cookie, gint fd, GError **error)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);
  GksuController *controller;
  GError *internal_error = NULL;

  controller = g_hash_table_lookup(priv->controllers, GINT_TO_POINTER(cookie));
  if(controller == NULL)
    return FALSE;

  gksu_controller_close_fd(controller, fd, &internal_error);
  if(internal_error)
    {
      g_propagate_error(error, internal_error);
      return FALSE;
    }

  return TRUE;
}

static gboolean gksu_server_maybe_shutdown(GksuServer *self)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);

  /* if the hash tables are still empty we can safely shutdown */
  if((g_hash_table_size(priv->controllers)) == 0 &&
     (g_hash_table_size(priv->zombies) == 0))
    g_signal_emit(self, signals[SHUTDOWN], 0);
  
  return FALSE;
}

static void gksu_server_check_shutdown(GksuServer *self)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);

  /* we already have a maybe shutdown scheduled */
  if(priv->shutdown_source_id)
    {
      g_source_remove(priv->shutdown_source_id);
      priv->shutdown_source_id = 0;
    }

  if((g_hash_table_size(priv->controllers)) == 0 &&
     (g_hash_table_size(priv->zombies) == 0))
    {
      priv->shutdown_source_id =
        g_timeout_add_seconds(10, (GSourceFunc)gksu_server_maybe_shutdown, (gpointer)self);
    }
}

gboolean gksu_server_wait(GksuServer *self, guint32 cookie, gint *status, GError **error)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);
  GksuZombie *zombie = g_hash_table_lookup(priv->zombies, GINT_TO_POINTER(cookie));

  if(zombie != NULL)
    {
      /* this allows for being able to call this method locally and
         not caring about the return value */
      if(status != NULL)
        *status = zombie->status;
      g_free(zombie->pending_stdout);
      zombie->pending_stdout_length = 0;
      g_free(zombie->pending_stderr);
      zombie->pending_stderr_length = 0;
      g_hash_table_remove(priv->zombies, GINT_TO_POINTER(cookie));
    }
  else
    {
      g_set_error(error, GKSU_ERROR, GKSU_ERROR_PROCESS_NOT_FOUND,
                  "Process not found. Process has not been started by this server or "
                  "has already been waited for.");
      *status = 0;
      return FALSE;
    }

  gksu_server_check_shutdown(self);

  return TRUE;
}

gboolean gksu_server_read_output(GksuServer *self, guint32 cookie, gint fd,
                                 gchar **data, gsize *length, GError **error)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);
  GksuController *controller;
  GksuZombie *zombie;

  controller = g_hash_table_lookup(priv->controllers, GINT_TO_POINTER(cookie));
  if(controller == NULL)
    zombie = g_hash_table_lookup(priv->zombies, GINT_TO_POINTER(cookie));

  if((controller == NULL) && (zombie == NULL))
    return FALSE;

  if(controller)
    {
      *data = gksu_controller_read_output(controller, fd, length, FALSE);
    }
  else
    {
      switch(fd)
        {
        case 1:
          *data = g_strdup(zombie->pending_stdout);
          *length = zombie->pending_stdout_length;
          break;
        case 2:
          *data = g_strdup(zombie->pending_stderr);
          *length = zombie->pending_stderr_length;
          break;
        }
    }

  return TRUE;
}

gboolean gksu_server_write_input(GksuServer *self, guint32 cookie, gchar *data,
                                 gsize length, GError **error)
{
  GksuServerPrivate *priv = GKSU_SERVER_GET_PRIVATE(self);
  GksuController *controller;
  GError *internal_error = NULL;

  controller = g_hash_table_lookup(priv->controllers, GINT_TO_POINTER(cookie));
  if(controller == NULL)
    return FALSE;

  gksu_controller_write_input(controller, (const gchar*)data,
                              (const gsize)length, &internal_error);
  if(internal_error)
    {
      g_propagate_error(error, internal_error);
      return FALSE;
    }

  return TRUE;
}
