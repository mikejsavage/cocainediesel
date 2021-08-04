#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/threads.h"

#include "gg/ggentropy.h"

#include "monocypher/monocypher.h"

static Mutex * mtx;

static u8 entropy[ 40 ];
static u64 ctr;

static s64 time_of_last_stir;
static size_t bytes_since_stir;

static bool InitChacha() {
	if( !ggentropy( entropy, sizeof( entropy ) ) )
		return false;
	ctr = 0;
	return true;
}

void InitCSPRNG() {
	mtx = NewMutex();

	time_of_last_stir = Sys_Milliseconds();
	bytes_since_stir = 0;

	if( !InitChacha() ) {
		Fatal( "InitChacha" );
	}
}

void ShutdownCSPRNG() {
	crypto_wipe( &entropy, sizeof( entropy ) );
	ctr = 0;
	DeleteMutex( mtx );
}

static void Stir() {
	// stir every 64k or 5 minutes
	if( bytes_since_stir < 64000 )
		return;

	s64 now = Sys_Milliseconds();
	if( now - time_of_last_stir < 300 * 1000 )
		return;

	if( !InitChacha() ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: InitChacha failed, not stirring" );
		return;
	}

	time_of_last_stir = now;
	bytes_since_stir = 0;
}

void CSPRNG( void * buf, size_t n ) {
	Lock( mtx );

	Stir();
	bytes_since_stir += n;

	ctr = crypto_chacha20_ctr( ( u8 * ) buf, NULL, n, entropy, entropy + 32, ctr );

	Unlock( mtx );
}
