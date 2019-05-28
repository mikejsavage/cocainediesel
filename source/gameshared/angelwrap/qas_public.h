#pragma once

struct angelwrap_api_s;

void QAS_Init();
struct angelwrap_api_s *QAS_GetAngelExport();
void QAS_Shutdown();
