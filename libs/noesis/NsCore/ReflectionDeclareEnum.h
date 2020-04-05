////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_REFLECTIONDECLAREENUM_H__
#define __CORE_REFLECTIONDECLAREENUM_H__


#include <NsCore/Noesis.h>


namespace Noesis
{
class TypeEnum;
template<class EnumT> class TypeEnumCreator;
template<class T> struct TypeEnumFiller;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Declares an enum to be used in reflection.
////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(NS_COMPILER_GCC) && NS_COMPILER_VERSION < 7000

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56480
#define NS_DECLARE_REFLECTION_ENUM_EXPORT(API, T) \
namespace Noesis \
{ \
template<> struct TypeEnumFiller<T> \
{ \
    API static const TypeEnum* GetType(); \
    static void Fill(TypeEnumCreator<T>& helper); \
}; \
}

#else

#define NS_DECLARE_REFLECTION_ENUM_EXPORT(API, T) \
template<> struct Noesis::TypeEnumFiller<T> \
{ \
    API static const Noesis::TypeEnum* GetType(); \
    static void Fill(Noesis::TypeEnumCreator<T>& helper); \
};

#endif

#define NS_DECLARE_REFLECTION_ENUM(T) NS_DECLARE_REFLECTION_ENUM_EXPORT(, T)

#endif
