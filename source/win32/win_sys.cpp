#define _WIN32_WINNT 0x4000
#include <windows.h>

bool Sys_BeingDebugged() {
	return IsDebuggerPresent() != 0;
}
