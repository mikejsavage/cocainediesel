#pragma once

#include "qcommon/types.h"

struct Cvar;
extern Cvar * vid_mode;

struct WindowMode {
	bool fullscreen;
	int monitor;
	int x, y;
	int width, height;
};

void format( FormatBuffer * fb, WindowMode mode, const FormatOpts & opts );

bool operator!=( WindowMode lhs, WindowMode rhs );

struct SDL_Window;
SDL_Window * VID_Init();

SDL_Window * CreateWindow( WindowMode mode );
void DestroyWindow();

void GetFramebufferSize( u32 * width, u32 * height, bool * minimized );
float GetContentScale();
Vec2 GetRelativeMouseMovement();
Vec2 GetJoystickMovement();
void VID_CheckChanges();

void FlashWindow();

bool IsWindowFocused();

WindowMode GetWindowMode();
void SetWindowMode( WindowMode mode );
