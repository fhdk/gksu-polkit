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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib-object.h>

#include "gksu-write-queue.h"

G_DEFINE_TYPE(GksuWriteQueue, gksu_write_queue, G_TYPE_OBJECT);

struct _GksuWriteQueuePrivate {
  GIOChannel *channel;
  guint source_id;
  GSList *queue;
  gsize queue_len;
};

#define GKSU_WRITE_QUEUE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GKSU_TYPE_WRITE_QUEUE, GksuWriteQueuePrivate))

static void gksu_write_queue_finalize(GObject *object)
{
  GksuWriteQueue *self = GKSU_WRITE_QUEUE(object);
  GksuWriteQueuePrivate *priv = GKSU_WRITE_QUEUE_GET_PRIVATE(self);

  if(priv->channel)
    {
      g_source_remove(priv->source_id);
      g_io_channel_unref(priv->channel);
    }

  g_slist_free(priv->queue);

  G_OBJECT_CLASS(gksu_write_queue_parent_class)->finalize(object);
}

static void gksu_write_queue_class_init(GksuWriteQueueClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = gksu_write_queue_finalize;

  g_type_class_add_private(klass, sizeof(GksuWriteQueuePrivate));
}

static void gksu_write_queue_init(GksuWriteQueue *self)
{
  GksuWriteQueuePrivate *priv = GKSU_WRITE_QUEUE_GET_PRIVATE(self);
  self->priv = priv;
}

gboolean
gksu_write_queue_disable(GIOChannel *channel, GIOCondition condition,
                         GksuWriteQueue *self)
{
  GksuWriteQueuePrivate *priv = GKSU_WRITE_QUEUE_GET_PRIVATE(self);
  g_source_remove(priv->source_id);
  return FALSE;
}

gboolean
gksu_write_queue_perform(GIOChannel *channel, GIOCondition condition,
                         GksuWriteQueue *self)
{
  GksuWriteQueuePrivate *priv = GKSU_WRITE_QUEUE_GET_PRIVATE(self);
  GIOStatus status = G_IO_STATUS_NORMAL;
  GError *error = NULL;
  GSList *iter = priv->queue;

  if(priv->queue_len == 0)
    {
      g_source_remove(priv->source_id);
      return FALSE;
    }

  while((iter != NULL) && (status != G_IO_STATUS_AGAIN))
    {
      GString *curstring = iter->data;
      gsize written;

      status = g_io_channel_write_chars(channel, curstring->str,
                                        curstring->len, &written, &error);
      if(error)
        {
          fprintf(stderr, "%s\n", error->message);
          g_clear_error(&error);
        }

      if(error)
        {
          fprintf(stderr, "%s\n", error->message);
          g_clear_error(&error);
        }

      if((status == G_IO_STATUS_AGAIN) && (written < curstring->len))
        {
          g_string_erase(curstring, 0, written);
          break;
        }

      g_string_free(curstring, TRUE);
      priv->queue = g_slist_delete_link(priv->queue, iter);
      iter = priv->queue;
      priv->queue_len--;

      /* we should never go bellow 0! */
      g_assert(priv->queue_len >= 0);
    }

  return TRUE;
}

void
gksu_write_queue_add(GksuWriteQueue *self, gchar *data, gsize length)
{
  GksuWriteQueuePrivate *priv = GKSU_WRITE_QUEUE_GET_PRIVATE(self);
  GString *string = g_string_new_len(data, length);

  if(priv->queue_len == 0)
    {
      priv->source_id = g_io_add_watch(priv->channel, G_IO_OUT,
                                       (GIOFunc)gksu_write_queue_perform,
                                       (gpointer)self);
    }

  priv->queue = g_slist_append(priv->queue, string);
  priv->queue_len++;
}

void
gksu_write_queue_shutdown(GksuWriteQueue *self, gboolean flush)
{
  GksuWriteQueuePrivate *priv = GKSU_WRITE_QUEUE_GET_PRIVATE(self);

  while(priv->queue_len > 0)
    gksu_write_queue_perform(priv->channel, G_IO_OUT, self);
  g_io_channel_shutdown(priv->channel, TRUE, NULL);
}

GksuWriteQueue*
gksu_write_queue_new(GIOChannel *channel)
{
  GksuWriteQueue *self = g_object_new(GKSU_TYPE_WRITE_QUEUE, NULL);
  GksuWriteQueuePrivate *priv = GKSU_WRITE_QUEUE_GET_PRIVATE(self);

  priv->channel = g_io_channel_ref(channel);
  g_io_add_watch(channel, G_IO_HUP|G_IO_NVAL,
                 (GIOFunc)gksu_write_queue_disable,
                 (gpointer)self);

  return self;
}

