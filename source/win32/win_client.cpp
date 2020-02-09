#define _WIN32_WINNT 0x4000
#include <windows.h>
#include <stdlib.h>
#include <shellapi.h>

// video drivers pick these up and make sure the game runs on the good GPU
extern "C" __declspec( dllexport ) DWORD NvOptimusEnablement = 1;
extern "C" __declspec( dllexport ) int AmdPowerXpressRequestHighPerformance = 1;

void Sys_InitTime();

void Sys_ShowErrorMessage( const char * msg ) {
	MessageBoxA( NULL, msg, "Error", MB_OK );
}

void Sys_Init() {
	SetConsoleOutputCP( CP_UTF8 );
	Sys_InitTime();
}

bool Sys_BeingDebugged() {
	return IsDebuggerPresent() != 0;
}

bool Sys_OpenInWebBrowser( const char * url ) {
	int ok = int( intptr_t( ShellExecuteA( NULL, "open", url, NULL, NULL, SW_SHOWDEFAULT ) ) );
	return ok > 32;
}

int main( int argc, char ** argv );
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, char * szCmdLine, int iCmdShow ) {
	return main( __argc, __argv );
}
