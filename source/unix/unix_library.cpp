
#include <dlfcn.h>

void* Sys_Library_Open( const char* name )
{
	return dlopen( name, RTLD_NOW );
}

void* Sys_Library_ProcAddress( void* lib, const char* apifuncname )
{
	return (void *)dlsym( lib, apifuncname );
}
