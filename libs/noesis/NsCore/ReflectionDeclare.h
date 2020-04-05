////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_REFLECTIONDECLARE_H__
#define __CORE_REFLECTIONDECLARE_H__


#include <NsCore/CompilerTools.h>


namespace Noesis
{

class TypeClass;
class TypeClassCreator;
typedef void NoParent;
template<class T> struct TypeTag;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
#define NS_DECLARE_STATIC_REFLECTION(classType, parentType) \
    /* Note that this function is overloaded with self type to avoid incorrectly using the \
       version of the parent in case the child does not have reflection macros */ \
    static const Noesis::TypeClass* StaticGetClassType(Noesis::TypeTag<classType>*);\
\
private:\
    typedef classType SelfClass;\
    typedef parentType ParentClass;\
\
    friend class Noesis::TypeClassCreator;\
    struct Rebind_;\
\
    /* This is templatized on purpose to avoid this function being exported */ \
    template<class VOID_> static void StaticFillClassType(Noesis::TypeClassCreator& helper);

// Supress clang "-Winconsistent-missing-override" this way because push/pop is not working
#ifdef __clang__
#pragma clang system_header
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Declares reflection for a class (interface, component or simple class/struct)
////////////////////////////////////////////////////////////////////////////////////////////////////
#define NS_DECLARE_REFLECTION(classType, parentType) \
public:\
    const Noesis::TypeClass* GetClassType() const;\
    NS_DECLARE_STATIC_REFLECTION(classType, parentType)


#endif
