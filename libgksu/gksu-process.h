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

#ifndef __GKSU_PROCESS_H__
#define __GKSU_PROCESS_H__ 1

#include <glib-object.h>

typedef struct _GksuProcessPrivate GksuProcessPrivate;

typedef struct {
  GObject parent;
  GksuProcessPrivate *priv;
} GksuProcess;

typedef struct {
  GObjectClass parent;
} GksuProcessClass;

GType gksu_process_get_type(void);

#define GKSU_TYPE_PROCESS (gksu_process_get_type())
#define GKSU_PROCESS(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GKSU_TYPE_PROCESS, GksuProcess))
#define GKSU_PROCESS_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS ((object), GKSU_TYPE_PROCESS, GksuProcessClass))

GksuProcess* gksu_process_new(const gchar *working_directory, const gchar **arguments);

gboolean gksu_process_spawn_async(GksuProcess *process, GError **error);

#endif
