#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/threads.h"

#include "gg/ggentropy.h"

#include "monocypher/monocypher.h"

static Mutex * mtx;
static crypto_chacha_ctx chacha;
static s64 time_of_last_stir;
static size_t bytes_since_stir;

static bool InitChacha() {
	u8 entropy[ 32 + 8 ];
	if( !ggentropy( entropy, sizeof( entropy ) ) )
		return false;

	crypto_chacha20_init( &chacha, entropy, entropy + 32 );
	crypto_wipe( entropy, sizeof( entropy ) );

	return true;
}

void InitCSPRNG() {
	mtx = NewMutex();

	time_of_last_stir = Sys_Milliseconds();
	bytes_since_stir = 0;

	if( !InitChacha() ) {
		Com_Error( ERR_FATAL, "InitChacha" );
	}
}

void ShutdownCSPRNG() {
	crypto_wipe( &chacha, sizeof( chacha ) );
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

	crypto_chacha20_stream( &chacha, ( u8 * ) buf, n );

	Unlock( mtx );
}
