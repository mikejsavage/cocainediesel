#include <float.h>

void EnableFPE() {
	_controlfp( _EM_INEXACT, _MCW_EM );
}

void DisableFPE() {
	_clearfp();
}
