/*
 * Copyright (C) 2008 Gustavo Noronha Silva
 *
 * This file is part of the Gksu PolicyKit library.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.  You should have received
 * a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GKSU_WRITE_QUEUE_H__
#define __GKSU_WRITE_QUEUE_H__ 1

#include <glib-object.h>

typedef struct _GksuWriteQueuePrivate GksuWriteQueuePrivate;

typedef struct {
  GObject parent;
  GksuWriteQueuePrivate *priv;
} GksuWriteQueue;

typedef struct {
  GObjectClass parent;
} GksuWriteQueueClass;

GType gksu_write_queue_get_type(void);

#define GKSU_TYPE_WRITE_QUEUE (gksu_write_queue_get_type())
#define GKSU_WRITE_QUEUE(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GKSU_TYPE_WRITE_QUEUE, GksuWriteQueue))
#define GKSU_WRITE_QUEUE_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS ((object), GKSU_TYPE_WRITE_QUEUE, GksuWriteQueueClass))

void gksu_write_queue_add(GksuWriteQueue *self, gchar *data, gsize length);
void gksu_write_queue_shutdown(GksuWriteQueue *self, gboolean flush);
GksuWriteQueue* gksu_write_queue_new(GIOChannel *channel);

#endif
