#include "qcommon/platform.h"
#include "renderdoc/renderdoc_app.h"

#include <stdlib.h>

static RENDERDOC_API_1_0_0 * api = NULL;

[[maybe_unused]] static void LoadAPI( void * get_api ) {
	if( get_api == NULL )
		return;

	RENDERDOC_API_1_0_0 * maybe_api = NULL;
	if( pRENDERDOC_GetAPI( get_api )( eRENDERDOC_API_Version_1_0_0, ( void ** ) &maybe_api ) == 1 ) {
		api = maybe_api;
	}
}

#if PLATFORM_WINDOWS

#include "qcommon/platform/windows_mini_windows_h.h"

void InitRenderDoc() {
	HMODULE dll = GetModuleHandleA( "renderdoc.dll" );
	if( dll == NULL )
		return;
	LoadAPI( GetProcAddress( dll, "RENDERDOC_GetAPI" ) );
}
#elif PLATFORM_MACOS

// RenderDoc doesn't run on macOS
void InitRenderDoc() { }

#elif PLATFORM_LINUX

#include <dlfcn.h>

void InitRenderDoc() {
	void * dll = dlopen( "librenderdoc.so", RTLD_NOW | RTLD_NOLOAD );
	if( dll == NULL )
		return;
	LoadAPI( dlsym( dll, "RENDERDOC_GetAPI" ) );
	dlclose( dll );
}

#endif

bool RunningInRenderDoc() {
	return api != NULL;
}

void TriggerRenderDocCapture() {
	if( api == NULL )
		return;
	api->TriggerCapture();
}
