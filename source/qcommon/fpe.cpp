#if !__arm64__

#include <immintrin.h>

void EnableFPE() {
	_mm_setcsr( _mm_getcsr() & ~_MM_MASK_INVALID );
}

void DisableFPE() {
	_mm_setcsr( _mm_getcsr() | _MM_MASK_INVALID );
}

#else

/*
 * TODO arm impl
 * __builtin_arm_get_fpscr()
 * __builtin_arm_set_fpscr( ... )
 * https://developer.arm.com/documentation/dui0068/b/Vector-Floating-point-Programming/VFP-system-registers/FPSCR--the-floating-point-status-and-control-register
 */

void EnableFPE() { }
void DisableFPE() { }

#endif
