#pragma once

#include "qcommon/types.h"
#include "gameshared/gs_synctypes.h"

void UI_Init();
void UI_Shutdown();

void UI_Refresh();

void UI_ShowMainMenu();
void UI_ShowConnectingScreen();
void UI_ShowGameMenu();
void UI_ShowLoadoutMenu( Loadout loadout );
void UI_ShowDemoMenu();
void UI_HideMenu();

void UI_AddToServerList( const char *adr, const char *info );

struct ImGuiColorToken {
	u8 token[ 6 ];
	ImGuiColorToken( u8 r, u8 g, u8 b, u8 a );
	ImGuiColorToken( RGB8 rgb );
	ImGuiColorToken( RGBA8 rgba );
};

void format( FormatBuffer * fb, const ImGuiColorToken & token, const FormatOpts & opts );

void CellCenter( float item_width );
void CellCenterText( const char * str );
void ColumnCenterText( const char * str );
void ColumnRightText( const char * str );
void WindowCenterTextXY( const char * str );

Vec4 CustomAttentionGettingColor( Vec4 from, Vec4 to, float div );
Vec4 AttentionGettingColor();
Vec4 PlantableColor();
