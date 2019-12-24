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

// global preprocessor defines
#include "config.h"

// q_shared.h -- included first by ALL program modules
#include <stdarg.h>
#include <ctype.h>
#include <inttypes.h>

//==============================================

#ifdef _WIN32

#define HAVE__STRICMP

#define OSNAME "Windows"

#include <malloc.h>

typedef int socklen_t;

typedef unsigned long ioctl_param_t;

typedef uintptr_t socket_handle_t;

#endif

#define ARCH "x86_64"

//==============================================

#if defined ( __linux__ )

#define OSNAME "Linux"

#include <alloca.h>

typedef int ioctl_param_t;

typedef int socket_handle_t;

#define SOCKET_ERROR ( -1 )
#define INVALID_SOCKET ( -1 )

#endif

//==============================================

#ifdef HAVE__STRICMP
#define Q_stricmp( s1, s2 ) _stricmp( ( s1 ), ( s2 ) )
#define Q_strnicmp( s1, s2, n ) _strnicmp( ( s1 ), ( s2 ), ( n ) )
#else
#define Q_stricmp( s1, s2 ) strcasecmp( ( s1 ), ( s2 ) )
#define Q_strnicmp( s1, s2, n ) strncasecmp( ( s1 ), ( s2 ), ( n ) )
#endif

// The `malloc' attribute is used to tell the compiler that a function
// may be treated as if it were the malloc function.  The compiler
// assumes that calls to malloc result in a pointers that cannot
// alias anything.  This will often improve optimization.
#if defined ( __GNUC__ )
#define ATTRIBUTE_MALLOC __attribute__( ( malloc ) )
#elif defined ( _MSC_VER )
#define ATTRIBUTE_MALLOC __declspec( noalias ) __declspec( restrict )
#else
#define ATTRIBUTE_MALLOC
#endif
