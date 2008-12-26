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

#ifndef __GKSU_ERROR_H__
#define __GKSU_ERROR_H__ 1

#include <glib/gerror.h>

#define GKSU_ERROR g_quark_from_static_string("gksu-error")

typedef enum
  {
    GKSU_ERROR_INVALID_VARIABLE,
    GKSU_ERROR_PREPARE_XAUTH_FAILED,
    GKSU_ERROR_PROCESS_NOT_FOUND,
  } GksuErrorEnum;

#endif
