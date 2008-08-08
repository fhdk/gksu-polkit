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
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <polkit-dbus/polkit-dbus.h>

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

enum {
  EXITED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

static void process_died_cb(DBusGProxy *server, gint pid, GksuProcess *self)
{
  GError *error = NULL;
  gint status;

  dbus_g_proxy_call(server, "Wait", &error,
                    G_TYPE_INT, pid,
                    G_TYPE_INVALID,
                    G_TYPE_INT, &status,
                    G_TYPE_INVALID);

  if(error)
    {
      g_warning("Error on wait message reply: %s\n", error->message);
      g_error_free(error);
      status = -1;
    }
  g_signal_emit(self, signals[EXITED], 0, status);
}

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

  signals[EXITED] = g_signal_new("exited",
                                 GKSU_TYPE_PROCESS,
                                 G_SIGNAL_RUN_LAST,
                                 0,
                                 NULL,
                                 NULL,
                                 g_cclosure_marshal_VOID__INT,
                                 G_TYPE_NONE, 1,
                                 G_TYPE_INT);

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

  dbus_g_proxy_add_signal(priv->server, "ProcessExited",
                          G_TYPE_INT, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(priv->server, "ProcessExited",
                              G_CALLBACK(process_died_cb),
                              (gpointer)self, NULL);

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

/* copied from libgksu */
static gchar*
get_xauth_token(const gchar *explicit_display)
{
  gchar *display;
  gchar *xauth_bin = NULL;
  FILE *xauth_output;
  gchar *tmp = NULL;
  gchar *xauth = g_new0(gchar, 256);

  if(explicit_display == NULL)
    display = (gchar*)g_getenv("DISPLAY");
  else
    display = (gchar*)explicit_display;

  /* find out where the xauth binary is located */
  if (g_file_test ("/usr/bin/xauth", G_FILE_TEST_IS_EXECUTABLE))
    xauth_bin = "/usr/bin/xauth";
  else if (g_file_test ("/usr/X11R6/bin/xauth", G_FILE_TEST_IS_EXECUTABLE))
    xauth_bin = "/usr/X11R6/bin/xauth";
  else
    {
      g_warning("Failed to obtain xauth key: xauth binary not found "
                "at usual locations.");

      return NULL;
    }

  /* get the authorization token */
  tmp = g_strdup_printf ("%s -i list %s | "
			 "head -1 | awk '{ print $3 }'",
			 xauth_bin,
			 display);
  if ((xauth_output = popen (tmp, "r")) == NULL)
    {
      g_warning("Failed to obtain xauth key: %s", g_strerror(errno));
      return NULL;
    }

  fread (xauth, sizeof(char), 255, xauth_output);
  pclose (xauth_output);
  g_free (tmp);

  /* If xauth is the empty string, then try striping the
   * hostname part of the DISPLAY string for getting the
   * auth token; this is needed for ssh-forwarded usage
   */
  if((!strcmp("", xauth)) && (explicit_display == NULL))
    {
      gchar *cut_display = NULL;

      g_free (xauth);
      cut_display = g_strdup(g_strrstr (display, ":"));
      xauth = get_xauth_token(cut_display);
      g_free(cut_display);
    }

  return xauth;
}

gboolean
gksu_process_spawn_async(GksuProcess *self, GError **error)
{
  GError *internal_error = NULL;
  GksuProcessPrivate *priv = GKSU_PROCESS_GET_PRIVATE(self);

  GHashTable *environment;
  gchar *xauth = get_xauth_token(NULL);
  gint pid;

  environment = gksu_environment_get_variables(priv->environment);
  dbus_g_proxy_call(priv->server, "Spawn", &internal_error,
                    G_TYPE_STRING, priv->working_directory,
                    G_TYPE_STRING, xauth,
                    G_TYPE_STRV, priv->arguments,
                    DBUS_TYPE_G_STRING_STRING_HASHTABLE, environment,
                    G_TYPE_INVALID,
                    G_TYPE_INT, &pid,
                    G_TYPE_INVALID);
  g_hash_table_destroy(environment);
  g_free(xauth);

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
        g_propagate_error(error, internal_error);

      return FALSE;
    }

  return TRUE;
}
