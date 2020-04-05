////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_REFLECTIONIMPLEMENT_H__
#define __CORE_REFLECTIONIMPLEMENT_H__


////////////////////////////////////////////////////////////////////////////////////////////////////
/// Shortcuts for defining reflection members
////////////////////////////////////////////////////////////////////////////////////////////////////

/// Adds metadatas
#define NsMeta helper.Meta

/// Indicates that type implements an interface
#define NsImpl Rebind_(helper).template Impl

/// Adds properties
#define NsProp helper.Prop

/// Adds events
#define NsEvent helper.Event


////////////////////////////////////////////////////////////////////////////////////////////////////
/// Implements reflection for a class outside class definition
////////////////////////////////////////////////////////////////////////////////////////////////////
#define NS_IMPLEMENT_REFLECTION(classType) \
const Noesis::TypeClass* classType::StaticGetClassType(Noesis::TypeTag<classType>*)\
{\
    static const Noesis::TypeClass* type;\
\
    if (NS_UNLIKELY(type == 0))\
    {\
        type = static_cast<const Noesis::TypeClass*>(Noesis::TypeCreate::Create(\
            NS_TYPEID(classType),\
            Noesis::TypeClassCreator::Create<SelfClass>,\
            Noesis::TypeClassCreator::Fill<SelfClass, ParentClass>));\
    }\
\
    return type;\
}\
\
const Noesis::TypeClass* classType::GetClassType() const\
{\
    return StaticGetClassType((Noesis::TypeTag<classType>*)nullptr);\
}\
\
struct classType::Rebind_ \
{ \
    NS_DISABLE_COPY(Rebind_) \
    NS_FORCE_INLINE Rebind_(Noesis::TypeClassCreator& helper_): helper(helper_) {} \
    template<class IFACE> NS_FORCE_INLINE void Impl() { helper.Impl<classType, IFACE>(); } \
    Noesis::TypeClassCreator& helper; \
}; \
\
template <class VOID_> \
NS_COLD_FUNC void classType::StaticFillClassType(Noesis::TypeClassCreator& helper)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Implements reflection for a templated class with one template param outside class definition
////////////////////////////////////////////////////////////////////////////////////////////////////
#define NS_IMPLEMENT_REFLECTION_T1(classType) \
template<class T1>\
const Noesis::TypeClass* classType<T1>::StaticGetClassType(Noesis::TypeTag<classType>*)\
{\
    static const Noesis::TypeClass* type;\
\
    if (NS_UNLIKELY(type == 0))\
    {\
        type = static_cast<const Noesis::TypeClass*>(Noesis::TypeCreate::Create(\
            NS_TYPEID(classType<T1>),\
            Noesis::TypeClassCreator::Create<SelfClass>,\
            Noesis::TypeClassCreator::Fill<SelfClass, ParentClass>));\
    }\
\
    return type;\
}\
\
template<class T1>\
const Noesis::TypeClass* classType<T1>::GetClassType() const\
{\
    return StaticGetClassType((Noesis::TypeTag<classType>*)nullptr);\
}\
\
template<class T1>\
struct classType<T1>::Rebind_ \
{ \
    NS_DISABLE_COPY(Rebind_) \
    NS_FORCE_INLINE Rebind_(Noesis::TypeClassCreator& helper_): helper(helper_) {} \
    template<class IFACE> NS_FORCE_INLINE void Impl() { helper.Impl<classType, IFACE>(); } \
    Noesis::TypeClassCreator& helper; \
}; \
\
template<class T1> template<class VOID_> \
NS_COLD_FUNC void classType<T1>::StaticFillClassType(Noesis::TypeClassCreator& helper)

// Supress clang "-Winconsistent-missing-override" this way because push/pop is not working
#ifdef __clang__
#pragma clang system_header
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Implements reflection for a class inside class definition (templates must use this one)
////////////////////////////////////////////////////////////////////////////////////////////////////
#define NS_IMPLEMENT_INLINE_REFLECTION(classType, parentType) \
public:\
    static const Noesis::TypeClass* StaticGetClassType(Noesis::TypeTag<classType>*)\
    {\
        static const Noesis::TypeClass* type;\
\
        if (NS_UNLIKELY(type == 0))\
        {\
            type = static_cast<const Noesis::TypeClass*>(Noesis::TypeCreate::Create(\
                NS_TYPEID(classType),\
                Noesis::TypeClassCreator::Create<SelfClass>,\
                Noesis::TypeClassCreator::Fill<SelfClass, ParentClass>));\
        }\
\
        return type;\
    }\
\
    const Noesis::TypeClass* GetClassType() const\
    {\
        return StaticGetClassType((Noesis::TypeTag<classType>*)nullptr);\
    }\
\
private:\
    typedef classType SelfClass;\
    typedef parentType ParentClass;\
\
    friend class Noesis::TypeClassCreator;\
\
    struct Rebind_ \
    { \
        NS_DISABLE_COPY(Rebind_) \
        NS_FORCE_INLINE Rebind_(Noesis::TypeClassCreator& helper_): helper(helper_) {} \
        template<class IFACE> NS_FORCE_INLINE void Impl() { helper.Impl<classType, IFACE>(); } \
        Noesis::TypeClassCreator& helper; \
    }; \
\
    template <class VOID_> \
    NS_COLD_FUNC static void StaticFillClassType(Noesis::TypeClassCreator& helper)


#include <NsCore/Noesis.h> 
#include <NsCore/CompilerTools.h>
#include <NsCore/TypeCreate.h>
#include <NsCore/TypeClassCreator.h>
#include <NsCore/TypeOf.h>


#endif
