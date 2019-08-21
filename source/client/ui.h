#pragma once

void UI_Init();
void UI_Shutdown();
void UI_TouchAllAssets();
void UI_KeyEvent( bool mainContext, int key, bool down );
void UI_CharEvent( bool mainContext, wchar_t key );
void UI_Refresh();
void UI_UpdateConnectScreen();

void UI_ShowMainMenu();
void UI_ShowGameMenu( bool spectating, bool ready );
void UI_ShowLoadoutMenu( int primary, int secondary );
void UI_ShowScoreboard( bool show );
void UI_ShowDemoMenu();
void UI_HideMenu();

void UI_AddToServerList( const char *adr, const char *info );
void UI_MouseSet( bool mainContext, int mx, int my, bool showCursor );
