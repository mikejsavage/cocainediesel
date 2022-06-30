#include <immintrin.h>

void EnableFPE() {
	_mm_setcsr( _mm_getcsr() & ~_MM_MASK_INVALID );
}

void DisableFPE() {
	_mm_setcsr( _mm_getcsr() | _MM_MASK_INVALID );
}
