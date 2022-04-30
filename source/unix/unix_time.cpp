#include <time.h>
#include <unistd.h>

#include "qcommon/types.h"

void Sys_Sleep( unsigned int millis ) {
	usleep( millis * 1000 );
}

bool FormatTimestamp( char * buf, size_t buf_size, const char * fmt, s64 timestamp ) {
	time_t time_t_timestamp = checked_cast< time_t >( timestamp );
	struct tm tm;
	localtime_r( &time_t_timestamp, &tm );
	return strftime( buf, buf_size, fmt, &tm ) != 0;
}

bool FormatCurrentTime( char * buf, size_t buf_size, const char * fmt ) {
	return FormatTimestamp( buf, buf_size, fmt, time( NULL ) );
}
