#include <time.h>
#include <unistd.h>

#include "qcommon/types.h"

static bool time_set = false;
static u64 base_usec;

u64 Sys_Microseconds() {
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );

	u64 usec = u64( ts.tv_sec ) * 1000000 + u64( ts.tv_nsec ) / 1000;

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
	usleep( millis * 1000 );
}

bool Sys_FormatTimestamp( char * buf, size_t buf_size, const char * fmt, s64 timestamp ) {
	time_t time_t_timestamp = checked_cast< time_t >( timestamp );
	struct tm tm;
	localtime_r( &time_t_timestamp, &tm );
	return strftime( buf, buf_size, fmt, &tm ) != 0;
}

bool Sys_FormatCurrentTime( char * buf, size_t buf_size, const char * fmt ) {
	return Sys_FormatTimestamp( buf, buf_size, fmt, time( NULL ) );
}
