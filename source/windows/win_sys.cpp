#include "windows/miniwindows.h"

void EnableFPE() { }
void DisableFPE() { }

void Sys_Init() {
	SetConsoleOutputCP( CP_UTF8 );
}
