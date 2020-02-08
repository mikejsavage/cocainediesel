#include <windows.h>
#include <time.h>

#include "qcommon/types.h"

static LARGE_INTEGER hwtimer_freq;

void Sys_InitTime() {
	QueryPerformanceFrequency( &hwtimer_freq );
}

static bool time_set = false;
static u64 base_usec;

u64 Sys_Microseconds() {
	static bool first = true;
	static LARGE_INTEGER p_start;

	LARGE_INTEGER now;
	QueryPerformanceCounter( &now );

	u64 usec = ( ( now.QuadPart - p_start.QuadPart ) * 1000000 ) / hwtimer_freq.QuadPart;

	if( !time_set ) {
		base_usec = usec;
		time_set = true;
	}

	return usec - base_usec;
}

s64 Sys_Milliseconds() {
	return Sys_Microseconds() / 1000;
}

void Sys_Sleep( unsigned int millis ) {
	Sleep( millis );
}

bool Sys_FormatTime( char * buf, size_t buf_size, const char * fmt ) {
	time_t now = time( NULL );
	struct tm tm;
	localtime_s( &tm, &now );
	return strftime( buf, buf_size, fmt, &tm ) != 0;
}
