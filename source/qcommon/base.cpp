#include <stdarg.h>
#include <errno.h>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"

bool break1 = false;
bool break2 = false;
bool break3 = false;
bool break4 = false;

void Fatal( const char * format, ... ) {
	va_list argptr;
	char msg[ 1024 ];

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Sys_ShowErrorMessage( msg );

	abort();
}

void FatalErrno( const char * msg ) {
	Fatal( "%s: %s (%d)", msg, strerror( errno ), errno );
}

void format( FormatBuffer * fb, Span< const char > span, const FormatOpts & opts ) {
	if( fb->capacity > 0 && fb->len < fb->capacity - 1 ) {
		size_t len = Min2( span.n, fb->capacity - fb->len - 1 );
		memcpy( fb->buf + fb->len, span.ptr, len );
		fb->buf[ fb->len + len ] = '\0';
	}
	fb->len += span.n;
}

char * CopyString( Allocator * a, const char * str ) {
	return ( *a )( "{}", str );
}

Span< const char > MakeSpan( const char * str ) {
	return Span< const char >( str, strlen( str ) );
}
