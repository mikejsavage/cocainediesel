#pragma once

#include "qcommon/types.h"

struct Cvar;
extern Cvar * vid_mode;

struct VideoMode {
	int width, height;
	float refresh_rate;
};

struct WindowMode {
	VideoMode video_mode;
	int monitor;
	int x, y;
	bool fullscreen;
};

void format( FormatBuffer * fb, VideoMode mode, const FormatOpts & opts );
void format( FormatBuffer * fb, WindowMode mode, const FormatOpts & opts );

bool operator!=( WindowMode lhs, WindowMode rhs );

void VID_Init();

void CreateWindow( WindowMode mode );
void DestroyWindow();

void GetFramebufferSize( u32 * width, u32 * height, bool * minimized );
float GetContentScale();
Vec2 GetRelativeMouseMovement();
Vec2 GetJoystickMovement();
void VID_CheckChanges();

void FlashWindow();

VideoMode GetVideoMode( int monitor );
bool IsWindowFocused();

WindowMode GetWindowMode();
void SetWindowMode( WindowMode mode );
