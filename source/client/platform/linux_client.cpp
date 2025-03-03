#include "qcommon/platform.h"

#if PLATFORM_LINUX

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

void ShowErrorMessage( const char * msg, const char * file, int line ) {
	printf( "%s (%s:%d)\n", msg, file, line );
}

bool Sys_OpenInWebBrowser( const char * url ) {
	int child = fork();
	if( child == -1 )
		return false;

	if( child != 0 )
		return true;

	execlp( "xdg-open", "xdg-open", url, ( char * ) 0 );

	return false;
}

bool IsRenderDocAttached() {
	return dlopen( "librenderdoc.so", RTLD_NOW | RTLD_NOLOAD ) != NULL;
}

#endif // #if PLATFORM_LINUX
