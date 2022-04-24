#include <fenv.h>

void Sys_Init() { }

void EnableFPE() {
	feenableexcept( FE_INVALID );
}

void DisableFPE() {
	fedisableexcept( FE_INVALID );
}
