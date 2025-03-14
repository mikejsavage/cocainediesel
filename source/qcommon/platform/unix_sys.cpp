#include "qcommon/platform.h"

#if PLATFORM_UNIX

void Sys_Init() { }

Optional< int > SystemMemoryUsagePercent() {
	return NONE;
}

#endif // #if PLATFORM_UNIX
