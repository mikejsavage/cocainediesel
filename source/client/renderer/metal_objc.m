#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include "glfw3/GLFW/glfw3.h"
#include "glfw3/GLFW/glfw3native.h"

#import <QuartzCore/CAMetalLayer.h>

struct GLFWwindow;
extern GLFWwindow * window;

void PleaseGLFWDoThisForMe( CAMetalLayer * swapchain ) {
	NSWindow * nswindow = glfwGetCocoaWindow( window );
	nswindow.contentView.layer = swapchain;
	nswindow.contentView.wantsLayer = YES;
}
