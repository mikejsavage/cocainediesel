#include <float.h>

void EnableFPE() {
	// _controlfp( _EM_INVALID, _MCW_EM );
}

void DisableFPE() {
	_clearfp();
}
