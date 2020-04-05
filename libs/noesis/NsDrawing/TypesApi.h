////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __DRAWING_TYPESAPI_H__
#define __DRAWING_TYPESAPI_H__


#include <NsCore/CompilerSettings.h>


#if defined(NS_DRAWING_TYPES_PRIVATE)
    #define NS_DRAWING_TYPES_API
#elif defined(NS_DRAWING_TYPES_EXPORTS)
    #define NS_DRAWING_TYPES_API NS_DLL_EXPORT
#else
    #define NS_DRAWING_TYPES_API NS_DLL_IMPORT
#endif


#endif
