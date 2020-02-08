#include <windows.h>
#include <stdlib.h>

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

int main( int argc, char ** argv );
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, char * szCmdLine, int iCmdShow ) {
	return main( __argc, __argv );
}
