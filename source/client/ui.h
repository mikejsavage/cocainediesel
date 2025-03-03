#pragma once

#include "qcommon/types.h"

void UI_Init();
void UI_Shutdown();

void UI_Refresh();

void UI_ShowMainMenu();
void UI_ShowConnectingScreen();
void UI_ShowGameMenu();
void UI_ShowLoadoutMenu( Loadout loadout );
void UI_ShowDemoMenu();
void UI_HideMenu();

struct ImGuiColorToken {
	u8 token[ 5 ];
	ImGuiColorToken( u8 r, u8 g, u8 b, u8 a );
	ImGuiColorToken( RGB8 rgb );
	ImGuiColorToken( RGBA8 rgba );
};

void format( FormatBuffer * fb, const ImGuiColorToken & token, const FormatOpts & opts );

void CenterTextY( Span< const char > str, float height );

void CellCenter( float item_width );
void CellCenterText( Span< const char > str );
void ColumnCenterText( Span< const char > str );
void ColumnRightText( Span< const char > str );
void WindowCenterTextXY( Span< const char > str );

Vec4 CustomAttentionGettingColor( Vec4 from, Vec4 to, Time period );
Vec4 AttentionGettingColor();
Vec4 PlantableColor();
Vec4 AttentionGettingRed();
