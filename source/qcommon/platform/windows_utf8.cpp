#include "qcommon/platform.h"

#if PLATFORM_WINDOWS

#include "qcommon/platform/windows_mini_windows_h.h"
#include <wchar.h>

#include "qcommon/base.h"

wchar_t * UTF8ToWide( Allocator * a, const char * utf8 ) {
	int len = MultiByteToWideChar( CP_UTF8, 0, utf8, -1, NULL, 0 );
	Assert( len != 0 );

	wchar_t * wide = AllocMany< wchar_t >( a, len );
	MultiByteToWideChar( CP_UTF8, 0, utf8, -1, wide, len );

	return wide;
}

char * WideToUTF8( Allocator * a, Span< const wchar_t > wide ) {
	int len = WideCharToMultiByte( CP_UTF8, 0, wide.ptr, wide.n, NULL, 0, NULL, NULL );
	Assert( len != 0 );

	char * utf8 = AllocMany< char >( a, len + 1 );
	WideCharToMultiByte( CP_UTF8, 0, wide.ptr, wide.n, utf8, len, NULL, NULL );
	utf8[ len ] = '\0';

	return utf8;
}

char * WideToUTF8( Allocator * a, const wchar_t * wide ) {
	return WideToUTF8( a, Span< const wchar_t >( wide, wcslen( wide ) ) );
}

#endif
