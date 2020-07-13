#include "qcommon/types.h"

#include "gg/ggtime.h"

u64 Milliseconds( u64 ms ) {
	return ms * GGTIME_FLICKS_PER_SECOND / 1000;
}

u64 Seconds( u64 secs ) {
	return secs * GGTIME_FLICKS_PER_SECOND;
}

float ToSeconds( u64 t ) {
	assert( t < GGTIME_FLICKS_PER_SECOND * 1000 );
	return t / float( GGTIME_FLICKS_PER_SECOND );
}
