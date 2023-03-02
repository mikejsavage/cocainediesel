#include "qcommon/platform.h"

#if PLATFORM_WINDOWS

/*
 * Live++ license means we have to use a shim DLL to load it, ask Mike
 */

#include "qcommon/platform/windows_mini_windows_h.h"

#include "qcommon/base.h"

using F = void ( * )();

static HMODULE livepp;
static F livepp_Frame = NULL;

void InitLivePP() {
	if( is_public_build )
		return;

	TracyZoneScoped;
	livepp = LoadLibraryW( L"livepp_shim.dll" );
	if( livepp == NULL )
		return;

	F livepp_Init = ( F ) GetProcAddress( livepp, "Init" );
	livepp_Frame = ( F ) GetProcAddress( livepp, "Frame" );

	if( livepp_Init != NULL ) {
		livepp_Init();
	}
}

void LivePPFrame() {
	TracyZoneScoped;

	if( livepp_Frame != NULL ) {
		livepp_Frame();
	}
}

#endif // #ifdef PLATFORM_WINDOWS
