#include "qcommon/platform.h"

#if PLATFORM_WINDOWS

#define _WIN32_WINNT 0x4000
#include "qcommon/platform/windows_mini_windows_h.h"
#include <stdio.h>
#include <stdlib.h>
#include <crtdbg.h>
#include <shellapi.h>

// video drivers pick these up and make sure the game runs on the good GPU
extern "C" __declspec( dllexport ) DWORD NvOptimusEnablement = 1;
extern "C" __declspec( dllexport ) int AmdPowerXpressRequestHighPerformance = 1;

void ShowErrorMessage( const char * msg, const char * file, int line ) {
#if NDEBUG
	MessageBoxA( NULL, msg, "Error", MB_OK );
#else
	if( IsDebuggerPresent() != 0 ) {
		__debugbreak();
	}
	else if( _CrtDbgReport( _CRT_ERROR, file, line, NULL, "%s", msg ) == 1 ) {
		_CrtDbgBreak();
	}
#endif
}

bool IsRenderDocAttached() {
	return GetModuleHandleA( "renderdoc.dll" ) != NULL;
}

bool OpenInWebBrowser( const char * url ) {
	int ok = int( intptr_t( ShellExecuteA( NULL, "open", url, NULL, NULL, SW_SHOWDEFAULT ) ) );
	return ok > 32;
}

#endif // #if PLATFORM_WINDOWS
