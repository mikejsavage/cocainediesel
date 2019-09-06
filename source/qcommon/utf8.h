#pragma once

#include <stdint.h>

uint32_t DecodeUTF8( uint32_t * state, uint32_t * codep, uint32_t byte );
uint32_t DecodeUTF8( uint32_t * state, uint32_t * codep, char byte );

char * StrChrUTF8( char * p, uint32_t c );
const char * StrChrUTF8( const char * p, uint32_t c );

const char * FindNextColorToken( const char * str, char * token );
