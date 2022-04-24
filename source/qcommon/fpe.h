#pragma once

void EnableFPE();
void DisableFPE();

#if PUBLIC_BUILD
#define DisableFPEScoped
#else
#define DisableFPEScoped DisableFPE(); defer { EnableFPE(); }
#endif
