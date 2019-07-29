#pragma once

#include <stddef.h>

void CSPRNG_Init();
void CSPRNG_Shutdown();
void CSPRNG_Bytes( void * buf, size_t n );
