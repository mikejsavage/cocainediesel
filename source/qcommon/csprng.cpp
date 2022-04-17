#include "qcommon/base.h"

#include "gg/ggentropy.h"

#include "monocypher/monocypher.h"

static u8 entropy[ 40 ];
static u64 ctr;

void InitCSPRNG() {
	if( !ggentropy( entropy, sizeof( entropy ) ) ) {
		Fatal( "ggentropy" );
	}
	ctr = 0;
}

void ShutdownCSPRNG() {
	crypto_wipe( &entropy, sizeof( entropy ) );
	crypto_wipe( &ctr, sizeof( ctr ) );
}

void CSPRNG( void * buf, size_t n ) {
	ctr = crypto_chacha20_ctr( ( u8 * ) buf, NULL, n, entropy, entropy + 32, ctr );
}
