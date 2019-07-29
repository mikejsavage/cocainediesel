#include "qcommon/base.h"
#include "qcommon/ggentropy.h"
#include "qcommon/qcommon.h"

#include "monocypher/monocypher.h"

static qmutex_t * mtx;
static crypto_chacha_ctx chacha;
static int64_t time_of_last_stir;
static size_t bytes_since_stir;

void CSPRNG_Init() {
	u8 entropy[ 32 + 8 ];
	bool ok = ggentropy( entropy, sizeof( entropy ) );
	if( !ok )
		Com_Error( ERR_FATAL, "ggentropy" );

	mtx = QMutex_Create();

	time_of_last_stir = Sys_Milliseconds();
	bytes_since_stir = 0;

	crypto_chacha20_init( &chacha, entropy, entropy + 32 );
}

void CSPRNG_Shutdown() {
	QMutex_Destroy( &mtx );
}

static void Stir() {
	// stir every 64k or 5 minutes
	if( bytes_since_stir < 64000 )
		return;

	int64_t now = Sys_Milliseconds();
	if( now - time_of_last_stir < 300 * 1000 )
		return;

	u8 entropy[ 32 ];
	bool ok = ggentropy( entropy, sizeof( entropy ) );
	if( !ok ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: ggentropy failed, not stirring" );
		return;
	}

	u8 ciphertext[ 32 ];
	crypto_chacha20_encrypt( &chacha, ciphertext, entropy, sizeof( entropy ) );

	time_of_last_stir = now;
	bytes_since_stir = 0;
}

void CSPRNG_Bytes( void * buf, size_t n ) {
	QMutex_Lock( mtx );

	Stir();
	bytes_since_stir += n;

	crypto_chacha20_stream( &chacha, ( u8 * ) buf, n );

	QMutex_Unlock( mtx );
}
