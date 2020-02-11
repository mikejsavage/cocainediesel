#define _WIN32_WINNT 0x4000
#include <windows.h>
#include <stdlib.h>
#include <shellapi.h>

#include "qcommon/qcommon.h"
#include "glfw3/GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw3/GLFW/glfw3native.h"

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

void Sys_ReallyGoBorderless( GLFWwindow * window, bool borderless ) {
	HWND handle = glfwGetWin32Window( window );

	DWORD style = GetWindowLongW( handle, GWL_STYLE );
	DWORD exstyle = GetWindowLongW( handle, GWL_EXSTYLE );

	Com_GGPrint( "{b} style   ", style );
	Com_GGPrint( "{b} caption ", DWORD(WS_CAPTION) );
	Com_GGPrint( "{b} sysmenu ", DWORD(WS_SYSMENU) );
	Com_GGPrint( "{b} thickfr ", DWORD(WS_THICKFRAME) );
	Com_GGPrint( "{b} minbox  ", DWORD(WS_MINIMIZEBOX) );
	Com_GGPrint( "{b} maxbox  ", DWORD(WS_MAXIMIZEBOX) );
	Com_GGPrint( "-----" );

	Com_GGPrint( "{b} exstyle ", style );
	Com_GGPrint( "{b} dlgmodf ", DWORD(WS_EX_DLGMODALFRAME) );
	Com_GGPrint( "{b} composi ", DWORD(WS_EX_COMPOSITED) );
	Com_GGPrint( "{b} windowe ", DWORD(WS_EX_WINDOWEDGE) );
	Com_GGPrint( "{b} cliente ", DWORD(WS_EX_CLIENTEDGE) );
	Com_GGPrint( "{b} layered ", DWORD(WS_EX_LAYERED) );
	Com_GGPrint( "{b} statice ", DWORD(WS_EX_STATICEDGE) );
	Com_GGPrint( "{b} toolwin ", DWORD(WS_EX_TOOLWINDOW) );
	Com_GGPrint( "{b} appwind ", DWORD(WS_EX_APPWINDOW) );
	Com_GGPrint( "-----" );

	DWORD flags = WS_SYSMENU | WS_MINIMIZEBOX;
	DWORD exflags = WS_EX_LAYERED | WS_EX_STATICEDGE;

	if( borderless ) {
		style |= flags;
		exstyle |= exflags;
	}
	else {
		style &= ~flags;
		exstyle &= ~exflags;
	}

	SetWindowLongW( handle, GWL_STYLE, style );
	SetWindowLongW( handle, GWL_EXSTYLE, exstyle );
}

bool Sys_OpenInWebBrowser( const char * url ) {
	int ok = int( intptr_t( ShellExecuteA( NULL, "open", url, NULL, NULL, SW_SHOWDEFAULT ) ) );
	return ok > 32;
}

int main( int argc, char ** argv );
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, char * szCmdLine, int iCmdShow ) {
	return main( __argc, __argv );
}
