#pragma once

#include "qcommon/types.h"
#include "gg/ggtime.h"

void InitTime();
Time Now();

bool FormatTimestamp( char * buf, size_t buf_size, const char * fmt, s64 time );
bool FormatCurrentTime( char * buf, size_t buf_size, const char * fmt );

template< typename T > constexpr Time Milliseconds( T ms ) { return { u64( ms    * GGTIME_FLICKS_PER_SECOND / 1000 ) }; }
template< typename T > constexpr Time Seconds( T secs )    { return { u64( secs  * GGTIME_FLICKS_PER_SECOND ) }; }
template< typename T > constexpr Time Minutes( T mins )    { return { u64( mins  * GGTIME_FLICKS_PER_SECOND * 60 ) }; }
template< typename T > constexpr Time Hours( T hours )     { return { u64( hours * GGTIME_FLICKS_PER_SECOND * 60 * 60 ) }; }
template< typename T > constexpr Time Days( T days )       { return { u64( days  * GGTIME_FLICKS_PER_SECOND * 60 * 60 * 24 ) }; }

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

inline float Unlerp01( Time lo, Time x, Time hi ) {
	x = Clamp( lo, x, hi );
	return ToSeconds( x - lo ) / ToSeconds( hi - lo );
}
