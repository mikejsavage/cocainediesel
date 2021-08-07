/*
 * Live++ license means we have to use a shim DLL to load it, ask Mike
 */

#include "windows/miniwindows.h"

#include "qcommon/qcommon.h"

using F = void ( * )();

static HMODULE livepp;
static F livepp_Frame = NULL;

void InitLivePP() {
	ZoneScoped;

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
	ZoneScoped;

	if( livepp_Frame != NULL ) {
		livepp_Frame();
	}
}
