#include "qcommon/types.h"

void VID_Init();
void VID_Shutdown();

void VID_GetViewportSize( u32 * width, u32 * height );
void VID_CheckChanges();

void VID_FlashWindow();

void VID_AppActivate( bool active, bool minimize );
bool VID_AppIsActive();
bool VID_AppIsMinimized();
