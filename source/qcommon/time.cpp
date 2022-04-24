#include "qcommon/types.h"
#include "qcommon/time.h"

#include "gg/ggtime.h"

static u64 zero_time;

void InitTime() {
	zero_time = ggtime();
}

static void AssertSmallEnoughToCastToFloat( Time t ) {
	assert( t.flicks < GGTIME_FLICKS_PER_SECOND * 1000 );
}

Time Now() {
	return { ggtime() - zero_time };
}

float ToSeconds( Time t ) {
	AssertSmallEnoughToCastToFloat( t );
	return t.flicks / float( GGTIME_FLICKS_PER_SECOND );
}

s64 Sys_Milliseconds() {
	return Now().flicks / ( GGTIME_FLICKS_PER_SECOND / 1000 );
}

Time Hz( u64 hz ) {
	assert( GGTIME_FLICKS_PER_SECOND % hz == 0 );
	return { GGTIME_FLICKS_PER_SECOND / hz };
}

bool operator==( Time lhs, Time rhs ) { return lhs.flicks == rhs.flicks; }
bool operator!=( Time lhs, Time rhs ) { return !( lhs == rhs ); }
bool operator<( Time lhs, Time rhs ) { return lhs.flicks < rhs.flicks; }
bool operator<=( Time lhs, Time rhs ) { return lhs < rhs || lhs == rhs; }
bool operator>( Time lhs, Time rhs ) { return lhs.flicks > rhs.flicks; }
bool operator>=( Time lhs, Time rhs ) { return lhs > rhs || lhs == rhs; }
Time operator+( Time lhs, Time rhs ) { return { lhs.flicks + rhs.flicks }; }
Time operator-( Time lhs, Time rhs ) { return { lhs.flicks - rhs.flicks }; }

Time operator*( Time t, float scale ) {
	AssertSmallEnoughToCastToFloat( t );
	return { u64( float( t.flicks ) * scale ) };
}

Time operator*( float scale, Time t ) { return t * scale; }
Time operator/( Time t, float inv_scale ) { return t * ( 1.0f / inv_scale ); }
Time operator%( Time lhs, Time rhs ) { return { lhs.flicks % rhs.flicks }; }

void operator+=( Time & lhs, Time rhs ) { lhs = lhs + rhs; }
void operator-=( Time & lhs, Time rhs ) { lhs = lhs - rhs; }
