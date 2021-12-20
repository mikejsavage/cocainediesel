#include "qcommon/types.h"
#include "qcommon/time.h"

#include "gg/ggtime.h"

static void AssertSmallEnoughToCastToFloat( Time t ) {
	assert( t.flicks < GGTIME_FLICKS_PER_SECOND * 1000 );
}

Time Now() {
	return { ggtime() };
}

float ToSeconds( Time t ) {
	AssertSmallEnoughToCastToFloat( t );
	return t.flicks / float( GGTIME_FLICKS_PER_SECOND );
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
	return { float( t.flicks * scale ) };
}

Time operator*( float scale, Time t ) {
	return t * scale;
}

Time operator/( Time t, float inv_scale ) {
	return t * ( 1.0f / inv_scale );
}
