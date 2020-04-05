////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_TYPEOFPOINTER_H__
#define __CORE_TYPEOFPOINTER_H__


#ifndef __INCLUDED_FROM_TYPEOF_H__
    #error Do not include this file directly. Use <TypeOf.h> instead
#endif


#include <NsCore/Noesis.h>
#include <NsCore/TypeOfBase.h>
#include <NsCore/TypePointer.h>
#include <NsCore/DynamicCast.h>


namespace Noesis
{

namespace Reflection
{
/// Auxiliary struct to create specific classes for reference types, because typeid() generates
/// the same type_info for T and T& types
template<class S> struct Ref { };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeOfHelper specialization for reference types.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class TypeOfHelper<T&>: public TypeOfHelperBase<
    TypeReference, TypeReference, T&, Reflection::Ref<T> >
{
public:
    static void Fill(Type* type)
    {
        TypeReference* typeReference = static_cast<TypeReference*>(type);
        typeReference->SetContentType(TypeOf<T>());
    }
};

namespace Reflection
{
/// Auxiliary struct to create specific classes for const types, because typeid() generates
/// the same type_info for T and T const types (although T* and T const* are different)
template<class T> struct Const { };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeOfHelper specialization for const types.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class TypeOfHelper<const T>: public TypeOfHelperBase<
    TypeConst, TypeConst, const T, Reflection::Const<T> >
{
public:
    static void Fill(Type* type)
    {
        TypeConst* typeConst = static_cast<TypeConst*>(type);
        typeConst->SetContentType(TypeOf<T>());
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeOfHelper specialization for pointer types.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class TypeOfHelper<T*>: public TypeOfHelperBase<TypePointer, TypePointer, T*>
{
public:
    static void Fill(Type* type)
    {
        TypePointer* typePointer = static_cast<TypePointer*>(type);
        typePointer->SetStaticContentType(TypeOf<T>());
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeOfHelper specialization for Ptr types.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class TypeOfHelper<Ptr<T> >: public TypeOfHelperBase<TypePtr, TypePtr, Ptr<T> >
{
public:
    static void Fill(Type* type)
    {
        TypePtr* typePtr = static_cast<TypePtr*>(type);
        typePtr->SetStaticContentType(TypeOf<T>());
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeOfHelper specialization for array types.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, uint32_t N> class TypeOfHelper<T[N]>: public TypeOfHelperBase<
    TypeCollection, TypeArray, T[N]>
{
public:
    static void Fill(Type* type)
    {
        TypeArray* typeArray = static_cast<TypeArray*>(type);
        typeArray->SetElemCount(N);
        typeArray->SetElemSize(sizeof(T));
        typeArray->SetElemType(TypeOf<T>());
    }
};

}

#endif
