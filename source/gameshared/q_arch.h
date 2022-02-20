/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

#include <stdarg.h>
#include <ctype.h>
#include <inttypes.h>

#ifdef _WIN32

typedef int socklen_t;

typedef unsigned long ioctl_param_t;

typedef uintptr_t socket_handle_t;

#endif

#if defined ( __linux__ )

typedef int ioctl_param_t;

typedef int socket_handle_t;

#define SOCKET_ERROR ( -1 )
#define INVALID_SOCKET ( -1 )

#endif
