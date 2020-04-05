////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_REFLECTIONIMPLEMENTEMPTY_H__
#define __CORE_REFLECTIONIMPLEMENTEMPTY_H__


#include <NsCore/Noesis.h>
#include <NsCore/CompilerTools.h>
#include <NsCore/TypeCreate.h>
#include <NsCore/TypeInfo.h>
#include <NsCore/TypeClassCreatorEmpty.h>


namespace Noesis
{
typedef void NoParent;
template<class T> struct TypeTag;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Implements reflection for a class outside class definition
////////////////////////////////////////////////////////////////////////////////////////////////////
#define NS_IMPLEMENT_REFLECTION_(classType) \
const Noesis::TypeClass* classType::StaticGetClassType(Noesis::TypeTag<classType>*)\
{\
    static const Noesis::TypeClass* type;\
\
    if (type == 0)\
    {\
        type = (const Noesis::TypeClass*)(Noesis::TypeCreate::Create(\
            NS_TYPEID(classType),\
            Noesis::TypeClassCreatorEmpty<SelfClass, ParentClass>::Create,\
            Noesis::TypeClassCreatorEmpty<SelfClass, ParentClass>::Fill));\
    }\
\
    return type;\
}\
\
const Noesis::TypeClass* classType::GetClassType() const\
{\
    return StaticGetClassType((Noesis::TypeTag<classType>*)nullptr);\
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Implements static reflection functions for a class inside class definition
////////////////////////////////////////////////////////////////////////////////////////////////////
#define NS_IMPLEMENT_INLINE_STATIC_REFLECTION_(classType, parentType) \
public:\
    static const Noesis::TypeClass* StaticGetClassType(Noesis::TypeTag<classType>*)\
    {\
        static const Noesis::TypeClass* type;\
\
        if (type == 0)\
        {\
            type = (const Noesis::TypeClass*)(Noesis::TypeCreate::Create(\
                NS_TYPEID(classType),\
                Noesis::TypeClassCreatorEmpty<SelfClass, ParentClass>::Create,\
                Noesis::TypeClassCreatorEmpty<SelfClass, ParentClass>::Fill));\
        }\
\
        return type;\
    }\
\
private:\
    typedef classType SelfClass;\
    typedef parentType ParentClass;

// Supress clang "-Winconsistent-missing-override" this way because push/pop is not working
#ifdef __clang__
#pragma clang system_header
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Implements reflection for a class inside class definition (templates must use this one)
////////////////////////////////////////////////////////////////////////////////////////////////////
#define NS_IMPLEMENT_INLINE_REFLECTION_(classType, parentType) \
public:\
    const Noesis::TypeClass* GetClassType() const\
    {\
        return StaticGetClassType((Noesis::TypeTag<classType>*)nullptr);\
    } \
    NS_IMPLEMENT_INLINE_STATIC_REFLECTION_(classType, parentType)

#endif
