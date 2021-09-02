#pragma once

void* Sys_Library_Open( const char* name );
void* Sys_Library_ProcAddress( void* lib, const char* apifuncname );
