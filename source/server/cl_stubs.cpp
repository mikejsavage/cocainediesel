#include "qcommon/types.h"

void CL_Init() { }
void CL_Shutdown() { }
void CL_Frame( int realmsec, int gamemsec ) { }
void CL_Disconnect( const char * message ) { }
bool CL_DemoPlaying() { return false; }

void Con_Print( Span< const char > str ) { }

void Key_Init() { }
void Key_Shutdown() { }
