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

#include "qcommon/base.h"
#include "qcommon/rng.h"

RNG NewRNG() {
	return RNG {
		.state = 0x853c49e6748fea9b_u64,
		.inc = 0xda3e39cb94b95bdb_u64,
	};
}

RNG NewRNG( uint64_t state, uint64_t seq ) {
	RNG rng;
	rng.state = 0;
	rng.inc = ( seq << 1 ) | 1;
	Random32( &rng );
	rng.state += state;
	Random32( &rng );
	return rng;
}

uint32_t Random32( RNG * rng ) {
	uint64_t oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005_u64 + rng->inc;
	uint32_t xorshifted = uint32_t( ( ( oldstate >> 18 ) ^ oldstate ) >> 27 );
	uint32_t rot = uint32_t( oldstate >> 59 );
	return ( xorshifted >> rot ) | ( xorshifted << ( ( -rot ) & 31 ) );
}

uint64_t Random64( RNG * rng ) {
	uint64_t hi = uint64_t( Random32( rng ) ) << uint64_t( 32 );
	return hi | Random32( rng );
}

int RandomUniform( RNG * rng, int lo, int hi ) {
	uint32_t range = uint32_t( hi ) - uint32_t( lo );
	uint32_t x = Random32( rng );
	return lo + int( ( uint64_t( x ) * range ) >> 32 );
}

// http://www.rng-random.org/posts/bounded-rands.html
int RandomUniformExact( RNG * rng, int lo, int hi ) {
	Assert( lo <= hi );
	uint32_t range = uint32_t( hi ) - uint32_t( lo );
	uint32_t x = Random32( rng );

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
			x = Random32( rng );
			m = uint64_t( x ) * uint64_t( range );
			l = uint32_t( m );
		}
	}
	return lo + ( m >> 32 );
}

float RandomFloat01( RNG * rng ) {
	uint32_t r = Random32( rng );
	return bit_cast< float >( 0x3F800000_u32 | ( r >> 9 ) ) - 1.0f;
}

float RandomFloat11( RNG * rng ) {
	union {
		uint32_t u;
		float f;
	} x;
	uint32_t r = Random32( rng );
	uint32_t sign = ( r & 1 ) << 31;
	x.u = 0x3F800000_u32 | ( r >> 9 );
	x.f -= 1.0f;
	x.u |= sign;
	return x.f;
}

float RandomUniformFloat( RNG * rng, float lo, float hi ) {
	float t = RandomFloat01( rng );
	return Lerp( lo, t, hi );
}

double RandomDouble01( RNG * rng ) {
	uint64_t r = Random64( rng );
	return bit_cast< double >( 0x3FF0000000000000_u64 | ( r >> 12 ) ) - 1.0;
}

double RandomDouble11( RNG * rng ) {
	union {
		uint64_t u;
		double d;
	} x;
	uint64_t r = Random64( rng );
	uint64_t sign = ( r & 1 ) << 63;
	x.u = 0x3FF0000000000000_u64 | ( r >> 12 );
	x.d -= 1.0;
	x.u |= sign;
	return x.d;
}

bool Probability( RNG * rng, float p ) {
	return RandomFloat01( rng ) < p;
}
