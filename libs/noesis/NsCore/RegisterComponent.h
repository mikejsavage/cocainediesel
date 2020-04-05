////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_REGISTERCOMPONENT_H__
#define __CORE_REGISTERCOMPONENT_H__


#include <NsCore/TypeOf.h>
#include <NsCore/KernelApi.h>
#include <NsCore/ComponentFactory.h>


namespace Noesis
{

NS_CORE_KERNEL_API void RegisterComponent(const TypeClass* typeClass,
    ComponentFactory::CreatorFn creatorFn);
NS_CORE_KERNEL_API void UnregisterComponent(const TypeClass* typeClass);

}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> Noesis::BaseComponent* NsComponentCreator(Noesis::Symbol)
{
    return new T;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> void NsRegisterComponent()
{
    Noesis::RegisterComponent(Noesis::TypeOf<T>(), NsComponentCreator<T>);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> void NsUnregisterComponent()
{
    Noesis::UnregisterComponent(Noesis::TypeOf<T>());
}

#endif
