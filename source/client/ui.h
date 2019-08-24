#pragma once

const char * FindNextColorToken( const char * str, char * token );
void ExpandColorTokens( String< 256 > * str, const char * name, uint8_t alpha );

void UI_Init();
void UI_Shutdown();
void UI_TouchAllAssets();
void UI_KeyEvent( bool mainContext, int key, bool down );
void UI_CharEvent( bool mainContext, wchar_t key );
void UI_Refresh();
void UI_UpdateConnectScreen();

void SCR_UpdateScoreboardMessage( const char *string );
void UI_ShowMainMenu();
void UI_ShowGameMenu( bool spectating, bool ready );
void UI_ShowLoadoutMenu( int primary, int secondary );
bool UI_ScoreboardShown();
void UI_ShowScoreboard();
void UI_ShowDemoMenu();
void UI_HideMenu();

void UI_AddToServerList( const char *adr, const char *info );
void UI_MouseSet( bool mainContext, int mx, int my, bool showCursor );
