////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_ANIMATIONAPI_H__
#define __GUI_ANIMATIONAPI_H__


#include <NsCore/CompilerSettings.h>


#if defined(NS_GUI_ANIMATION_PRIVATE)
    #define NS_GUI_ANIMATION_API
#elif defined(NS_GUI_ANIMATION_EXPORTS)
    #define NS_GUI_ANIMATION_API NS_DLL_EXPORT
#else
    #define NS_GUI_ANIMATION_API NS_DLL_IMPORT
#endif


#endif
