#pragma once

#include "qcommon/types.h"
#include "gg/ggtime.h"

struct Time {
	u64 flicks;
};

Time Now();

constexpr Time Milliseconds( u64 ms ) { return { ms * GGTIME_FLICKS_PER_SECOND / 1000 }; }
constexpr Time Seconds( u64 secs ) { return { secs * GGTIME_FLICKS_PER_SECOND }; }

float ToSeconds( Time t );

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
