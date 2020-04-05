////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_KERNELAPI_H__
#define __CORE_KERNELAPI_H__


#include <NsCore/CompilerSettings.h>


#if defined(NS_CORE_KERNEL_PRIVATE)
    #define NS_CORE_KERNEL_API
#elif defined(NS_CORE_KERNEL_EXPORTS)
    #define NS_CORE_KERNEL_API NS_DLL_EXPORT
#else
    #define NS_CORE_KERNEL_API NS_DLL_IMPORT
#endif


#endif
