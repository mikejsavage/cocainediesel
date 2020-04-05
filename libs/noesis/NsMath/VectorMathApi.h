////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __MATH_VECTORMATHAPI_H__
#define __MATH_VECTORMATHAPI_H__


#include <NsCore/CompilerSettings.h>


#if defined(NS_MATH_VECTORMATH_PRIVATE)
    #define NS_MATH_VECTORMATH_API
#elif defined(NS_MATH_VECTORMATH_EXPORTS)
    #define NS_MATH_VECTORMATH_API NS_DLL_EXPORT
#else
    #define NS_MATH_VECTORMATH_API NS_DLL_IMPORT
#endif


#endif
