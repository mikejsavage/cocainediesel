#include "windows/miniwindows.h"
#include <time.h>

#include "qcommon/types.h"

static LARGE_INTEGER hwtimer_freq;

void Sys_InitTime() {
	QueryPerformanceFrequency( &hwtimer_freq );
}

static bool time_set = false;
static u64 base_usec;

static u64 Sys_Microseconds() {
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

bool Sys_FormatTimestamp( char * buf, size_t buf_size, const char * fmt, s64 timestamp ) {
	time_t time_t_timestamp = checked_cast< time_t >( timestamp );
	struct tm tm;
	localtime_s( &tm, &time_t_timestamp );
	return strftime( buf, buf_size, fmt, &tm ) != 0;
}

bool Sys_FormatCurrentTime( char * buf, size_t buf_size, const char * fmt ) {
	return Sys_FormatTimestamp( buf, buf_size, fmt, time( NULL ) );
}
