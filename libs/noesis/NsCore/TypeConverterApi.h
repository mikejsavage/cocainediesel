////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_TYPECONVERTERAPI_H__
#define __CORE_TYPECONVERTERAPI_H__


#include <NsCore/CompilerSettings.h>


#if defined(NS_CORE_TYPECONVERTER_PRIVATE)
    #define NS_CORE_TYPECONVERTER_API
#elif defined(NS_CORE_TYPECONVERTER_EXPORTS)
    #define NS_CORE_TYPECONVERTER_API NS_DLL_EXPORT
#else
    #define NS_CORE_TYPECONVERTER_API NS_DLL_IMPORT
#endif


#endif
