#include <stdarg.h>
#include <errno.h>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"

void FatalImpl( const char * file, int line, const char * format, ... ) {
	va_list argptr;
	char msg[ 1024 ];

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	ShowErrorMessage( msg, file, line );
	abort();
}

void FatalErrno( const char * msg, SourceLocation src_loc ) {
	FatalImpl( src_loc.file, src_loc.line, "%s: %s (%d)", msg, strerror( errno ), errno );
}

void AssertFail( const char * str, const char * file, int line ) {
	FatalImpl( file, line, "Assertion failed: %s", str );
}

void format( FormatBuffer * fb, Span< const char > span, const FormatOpts & opts ) {
	if( fb->capacity > 0 && fb->len < fb->capacity - 1 ) {
		size_t len = Min2( span.n, fb->capacity - fb->len - 1 );
		memcpy( fb->buf + fb->len, span.ptr, len );
		fb->buf[ fb->len + len ] = '\0';
	}
	fb->len += span.n;
}

char * CopyString( Allocator * a, const char * str, SourceLocation src ) {
	return ( *a )( src, "{}", str );
}

Span< char > MakeSpan( char * str ) {
	return Span< char >( str, strlen( str ) );
}

Span< const char > MakeSpan( const char * str ) {
	return Span< const char >( str, strlen( str ) );
}
