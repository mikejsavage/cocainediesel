/*
 * RNG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@rng-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the RNG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.rng-random.org
 */

#include <string.h>
#include <assert.h>

#include "qcommon/rng.h"
#include "gameshared/q_shared.h"
#include "gameshared/q_math.h"

RNG new_rng() {
	RNG rng;
	rng.state = UINT64_C( 0x853c49e6748fea9b );
	rng.inc = UINT64_C( 0xda3e39cb94b95bdb );
	return rng;
}

RNG new_rng( uint64_t state, uint64_t seq ) {
	RNG rng;
	rng.state = 0;
	rng.inc = ( seq << 1 ) | 1;
	random_u32( &rng );
	rng.state += state;
	random_u32( &rng );
	return rng;
}

uint32_t random_u32( RNG * rng ) {
	uint64_t oldstate = rng->state;
	rng->state = oldstate * UINT64_C( 6364136223846793005 ) + rng->inc;
	uint32_t xorshifted = uint32_t( ( ( oldstate >> 18 ) ^ oldstate ) >> 27 );
	uint32_t rot = uint32_t( oldstate >> 59 );
	return ( xorshifted >> rot ) | ( xorshifted << ( ( -rot ) & 31 ) );
}

uint64_t random_u64( RNG * rng ) {
	uint64_t hi = uint64_t( random_u32( rng ) ) << uint64_t( 32 );
	return hi | random_u32( rng );
}

// http://www.rng-random.org/posts/bounded-rands.html
int random_uniform( RNG * rng, int lo, int hi ) {
	assert( lo <= hi );
	uint32_t range = uint32_t( hi ) - uint32_t( lo );
	uint32_t x = random_u32( rng );

	uint64_t m = uint64_t( x ) * uint64_t( range );
	uint32_t l = uint32_t( m );
	if( l < range ) {
		uint32_t t = -range;
		if( t >= range ) {
			t -= range;
			if( t >= range )
				t %= range;
		}
		while( l < t ) {
			x = random_u32( rng );
			m = uint64_t( x ) * uint64_t( range );
			l = uint32_t( m );
		}
	}
	return lo + ( m >> 32 );
}

float random_float01( RNG * rng ) {
	uint32_t r = random_u32( rng );
	return bit_cast< float >( UINT32_C( 0x3F800000 ) | ( r >> 9 ) ) - 1.0f;
}

float random_float11( RNG * rng ) {
	union {
		uint32_t u;
		float f;
	} x;
	uint32_t r = random_u32( rng );
	uint32_t sign = ( r & 1 ) << 31;
	x.u = UINT32_C( 0x3F800000 ) | ( r >> 9 );
	x.f -= 1.0f;
	x.u |= sign;
	return x.f;
}

float random_uniform_float( RNG * rng, float lo, float hi ) {
	float t = random_float01( rng );
	return Lerp( lo, t, hi );
}

double random_double01( RNG * rng ) {
	uint64_t r = random_u64( rng );
	return bit_cast< double >( UINT64_C( 0x3FF0000000000000 ) | ( r >> 12 ) ) - 1.0;
}

double random_double11( RNG * rng ) {
	union {
		uint64_t u;
		double d;
	} x;
	uint64_t r = random_u64( rng );
	uint64_t sign = ( r & 1 ) << 63;
	x.u = UINT64_C( 0x3FF0000000000000 ) | ( r >> 12 );
	x.d -= 1.0;
	x.u |= sign;
	return x.d;
}

bool random_p( RNG * rng, float p ) {
	return random_float01( rng ) < p;
}
