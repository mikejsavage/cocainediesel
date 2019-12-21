#include "qcommon/base.h"

bool break1 = false;
bool break2 = false;
bool break3 = false;
bool break4 = false;

void format( FormatBuffer * fb, Span< const char > span, const FormatOpts & opts ) {
	if( fb->capacity > 0 && fb->len < fb->capacity - 1 ) {
		size_t len = Min2( span.n, fb->capacity - fb->len - 1 );
		memcpy( fb->buf + fb->len, span.ptr, len );
		fb->buf[ fb->len + len ] = '\0';
	}
	fb->len += span.n;
}
