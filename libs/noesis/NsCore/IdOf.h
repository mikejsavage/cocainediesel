////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_IDOF_H__
#define __CORE_IDOF_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>
#include <NsCore/TypeOfForward.h>
#include <NsCore/Symbol.h>


namespace Noesis
{

class Type;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helpers for generating TypeIds from types. The functions receiving a base argument composes
/// it with the type. For example, 
///     - IdOf<Test0>("Base") == "Base<Test0>"
///     - IdOf<Test0, Test1>("Base") == "Base<Test0,Test1>"
////////////////////////////////////////////////////////////////////////////////////////////////////
//@{
NS_CORE_KERNEL_API NsSymbol IdOf(const Type* type);
NS_CORE_KERNEL_API NsSymbol IdOf(const char* base, const Type* type0);
NS_CORE_KERNEL_API NsSymbol IdOf(const char* base, const Type* type0, const Type* type1);

template<class T0>
static NsSymbol IdOf()
{
    return IdOf(TypeOf<T0>());
}

template<class T0>
static NsSymbol IdOf(const char* base)
{
    return IdOf(base, TypeOf<T0>());
}

template<class T0, class T1>
static NsSymbol IdOf(const char* base)
{
    return IdOf(base, TypeOf<T0>(), TypeOf<T1>());
}
//@}

}

#endif
