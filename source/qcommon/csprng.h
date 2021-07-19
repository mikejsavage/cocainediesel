#pragma once

#include <stddef.h>

void InitCSPRNG();
void ShutdownCSPRNG();
void CSPRNG( void * buf, size_t n );
