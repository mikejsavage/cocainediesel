#include "qcommon/base.h"
#include "qcommon/time.h"

#include "gg/ggtime.h"

static u64 zero_time;

void InitTime() {
	if( is_public_build ) {
		zero_time = ggtime();
	}
	else {
		// start at 1 year to catch float precision bugs
		// we still have such bugs so only do it in debug for now
		zero_time = ggtime() - Days( 365 ).flicks;
	}
}

static void AssertSmallEnoughToCastToFloat( Time t ) {
	assert( t < Hours( 2 ) );
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
bool operator!=( Time lhs, Time rhs ) { return lhs.flicks != rhs.flicks; }
bool operator<( Time lhs, Time rhs ) { return lhs.flicks < rhs.flicks; }
bool operator<=( Time lhs, Time rhs ) { return lhs.flicks <= rhs.flicks; }
bool operator>( Time lhs, Time rhs ) { return lhs.flicks > rhs.flicks; }
bool operator>=( Time lhs, Time rhs ) { return lhs.flicks >= rhs.flicks; }
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

float Sin( Time t, Time period ) {
	AssertSmallEnoughToCastToFloat( period );
	t = t % period;
	return sinf( float( t.flicks ) / float( period.flicks ) * PI * 2.0f );
}
