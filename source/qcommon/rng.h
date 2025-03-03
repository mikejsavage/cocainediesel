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
 *     http://www.rng-random.org
 */

#pragma once

#include "qcommon/types.h"

struct RNG {
	// RNG state. All values are possible.
	u64 state;
	// Controls which RNG sequence (stream) is selected. Must *always* be odd.
	u64 inc;
};

RNG NewRNG();
RNG NewRNG( u64 state, u64 seq );

// return a random number in [0, 2^32)
u32 Random32( RNG * rng );

// return a random number in [0, 2^64)
u64 Random64( RNG * rng );

// return a random number in [lo, hi)
int RandomUniform( RNG * rng, int lo, int hi );
int RandomUniformExact( RNG * rng, int lo, int hi );

// return a random float in [0, 1)
float RandomFloat01( RNG * rng );
// return a random float in [-1, 1)
float RandomFloat11( RNG * rng );

float RandomUniformFloat( RNG * rng, float lo, float hi );

// return a random double in [0, 1)
double RandomDouble01( RNG * rng );
// return a random double in [-1, 1)
double RandomDouble11( RNG * rng );

// returns true with probability p
bool Probability( RNG * rng, float p );

template< typename T >
T & RandomElement( RNG * rng, T * arr, size_t n ) {
	Assert( n > 0 );
	return arr[ RandomUniform( rng, 0, n ) ];
}

template< typename T >
T & RandomElement( RNG * rng, Span< T > span ) {
	return RandomElement( rng, span.ptr, span.n );
}

template< typename T, size_t N >
const T & RandomElement( RNG * rng, const T ( &arr )[ N ] ) {
	return RandomElement( rng, arr, N );
}
