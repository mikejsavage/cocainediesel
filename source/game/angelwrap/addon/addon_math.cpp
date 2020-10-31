/*
Copyright (C) 2008 German Garcia

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

#include "qcommon/base.h"
#include "../qas_local.h"
#include "addon_math.h"
#include "qcommon/rng.h"
#include "server/server.h"

/*************************************
* MATHS ADDON
**************************************/

static int asFunc_abs( int x ) {
	return Abs( x );
}

static float asFunc_fabs( float x ) {
	return Abs( x );
}

static float asFunc_pow( float x, float y ) {
	return powf( x, y );
}

static float asFunc_cos( float x ) {
	return cosf( x );
}

static float asFunc_sin( float x ) {
	return sinf( x );
}

static float asFunc_tan( float x ) {
	return tanf( x );
}

static float asFunc_sqrt( float x ) {
	return sqrtf( x );
}

static float asFunc_ceil( float x ) {
	return ceilf( x );
}

static float asFunc_floor( float x ) {
	return floorf( x );
}

static uint32_t asFunc_random_uint() {
	return random_u32( &svs.rng );
}

static int asFunc_random_uniform( int lo, int hi ) {
	return random_uniform( &svs.rng, lo, hi );
}

static float asFunc_random_float01() {
	return random_float01( &svs.rng );
}

void PreRegisterMathAddon( asIScriptEngine *engine ) {
}

void RegisterMathAddon( asIScriptEngine *engine ) {
	const struct
	{
		const char *declaration;
		asSFuncPtr ptr;
	}
	math_asGlobFuncs[] =
	{
		{ "int abs( int x )", asFUNCTION( asFunc_abs ) },
		{ "float abs( float x )", asFUNCTION( asFunc_fabs ) },
		{ "float pow( float x, float y )", asFUNCTION( asFunc_pow ) },
		{ "float cos( float x )", asFUNCTION( asFunc_cos ) },
		{ "float sin( float x )", asFUNCTION( asFunc_sin ) },
		{ "float tan( float x )", asFUNCTION( asFunc_tan ) },
		{ "float sqrt( float x )", asFUNCTION( asFunc_sqrt ) },
		{ "float ceil( float x )", asFUNCTION( asFunc_ceil ) },
		{ "float floor( float x )", asFUNCTION( asFunc_floor ) },
		{ "uint random_uint()", asFUNCTION( asFunc_random_uint ) },
		{ "int random_uniform( int lo, int hi )", asFUNCTION( asFunc_random_uniform ) },
		{ "float random_float01()", asFUNCTION( asFunc_random_float01 ) },

		{ NULL, asFUNCTION( 0 ) }
	}, *func;
	int r = 0;

	for( func = math_asGlobFuncs; func->declaration; func++ ) {
		r = engine->RegisterGlobalFunction( func->declaration, func->ptr, asCALL_CDECL ); assert( r >= 0 );
	}

	(void)sizeof( r ); // hush the compiler
}
