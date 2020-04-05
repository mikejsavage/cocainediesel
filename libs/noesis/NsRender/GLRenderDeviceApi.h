////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __RENDER_GLRENDERDEVICEAPI_H__
#define __RENDER_GLRENDERDEVICEAPI_H__


#include <NsCore/CompilerSettings.h>


#if defined(NS_RENDER_GLRENDERDEVICE_PRIVATE)
    #define NS_RENDER_GLRENDERDEVICE_API
#elif defined(NS_RENDER_GLRENDERDEVICE_EXPORTS)
    #define NS_RENDER_GLRENDERDEVICE_API NS_DLL_EXPORT
#else
    #define NS_RENDER_GLRENDERDEVICE_API NS_DLL_IMPORT
#endif


#endif
