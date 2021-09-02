
#include "windows/miniwindows.h"

void* Sys_Library_Open( const char* name )
{
	return LoadLibraryA( name );
}

void* Sys_Library_ProcAddress( void* lib, const char* apifuncname )
{
	return GetProcAddress( (HMODULE)lib, apifuncname );
}
