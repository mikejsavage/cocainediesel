#pragma once

#include "qcommon/types.h"
#include "gg/ggtime.h"

void InitTime();
Time Now();

bool FormatTimestamp( char * buf, size_t buf_size, const char * fmt, s64 time );
bool FormatCurrentTime( char * buf, size_t buf_size, const char * fmt );

constexpr Time Milliseconds( u64 ms ) { return { ms * GGTIME_FLICKS_PER_SECOND / 1000 }; }
constexpr Time Milliseconds( int ms ) { return Milliseconds( u64( ms ) ); }
constexpr Time Milliseconds( double ms ) { return { u64( ms * GGTIME_FLICKS_PER_SECOND / 1000.0 ) }; }
constexpr Time Seconds( u64 secs ) { return { secs * GGTIME_FLICKS_PER_SECOND }; }
constexpr Time Seconds( int secs ) { return Seconds( u64( secs ) ); }
constexpr Time Seconds( double secs ) { return { u64( secs * GGTIME_FLICKS_PER_SECOND ) }; }

float ToSeconds( Time t );
Time Hz( u64 hz );

bool operator==( Time lhs, Time rhs );
bool operator!=( Time lhs, Time rhs );
bool operator<( Time lhs, Time rhs );
bool operator<=( Time lhs, Time rhs );
bool operator>( Time lhs, Time rhs );
bool operator>=( Time lhs, Time rhs );

Time operator+( Time lhs, Time rhs );
Time operator-( Time lhs, Time rhs );
Time operator*( Time t, float scale );
Time operator*( float scale, Time t );
Time operator/( Time t, float inv_scale );
Time operator%( Time lhs, Time rhs );

void operator+=( Time & lhs, Time rhs );
void operator-=( Time & lhs, Time rhs );

// FYI: old style sinf( t / x ) is equivalent to Sin( t, Seconds( x * 2pi / 1000 ) )
float Sin( Time t, Time period );
