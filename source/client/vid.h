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

void VID_Init();

void CreateWindow( WindowMode mode );
void DestroyWindow();

void SwapBuffers();

void GetFramebufferSize( int * width, int * height );
float GetContentScale();
Vec2 GetRelativeMouseMovement();
Vec2 GetJoystickMovement();
void VID_CheckChanges();

void FlashWindow();

bool IsWindowFocused();

WindowMode GetWindowMode();
void SetWindowMode( WindowMode mode );

void EnableVSync( bool enabled );
