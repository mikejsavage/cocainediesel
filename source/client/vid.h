#include "qcommon/types.h"

extern cvar_t * vid_mode;

struct VideoMode {
	int width, height;
	int frequency;
};

enum FullscreenMode {
	FullscreenMode_Windowed,
	FullscreenMode_Borderless,
	FullscreenMode_Fullscreen,
};

struct WindowMode {
	VideoMode video_mode;
	int monitor;
	int x, y;
	FullscreenMode fullscreen;
};

void format( FormatBuffer * fb, VideoMode mode, const FormatOpts & opts );
void format( FormatBuffer * fb, WindowMode mode, const FormatOpts & opts );

bool operator!=( WindowMode lhs, WindowMode rhs );

void VID_Init();
void VID_Shutdown();

void CreateWindow( WindowMode mode );
void DestroyWindow();

void GlfwInputFrame();
void SwapBuffers();

void GetFramebufferSize( int * width, int * height );
Vec2 GetMouseMovement();
void VID_CheckChanges();

void FlashWindow();

VideoMode GetVideoMode( int monitor );
bool IsWindowFocused();

WindowMode GetWindowMode();
void SetWindowMode( WindowMode mode );

void EnableVSync( bool enabled );
