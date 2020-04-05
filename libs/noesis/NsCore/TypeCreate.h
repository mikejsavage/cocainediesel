////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_TYPECREATE_H__
#define __CORE_TYPECREATE_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>


namespace Noesis
{

class TypeInfo;
class Type;
class Symbol;

namespace TypeCreate
{

/// Used by TypeOf helper and Reflection Macros to create and register a new type into the 
/// ReflectionRegistry. It shouldn't be used directly.
/// There are two versions. In the first one, a function for type creation is given.
/// This function will be invoked by the ReflectionSystem whenever it needs the type.
/// The second version receives an additional function pointer to be invoked when information
/// about the type is needed. For example, for the classtype the fill function will add
/// class members and functions. Having two function pointers allows for circular references
/// between types.
/// \param regName Registration name of the type
/// \param typeId Identifier of the reflection type
/// \param typeCreator Function used to create the reflection type
/// \return Recently registered type
//@{
typedef Type* (*CreatorFn) (const TypeInfo& typeInfo);
NS_CORE_KERNEL_API Type* Create(const TypeInfo& typeInfo, CreatorFn typeCreator);

typedef void (*FillerFn) (Type* type);
NS_CORE_KERNEL_API Type* Create(const TypeInfo& typeInfo, CreatorFn typeCreator, 
    FillerFn typeFiller);
//@}

}

namespace TypeRegister
{
NS_CORE_KERNEL_API void Register(const TypeInfo& typeInfo, Type* type, Symbol typeId);
NS_CORE_KERNEL_API void Unregister(const Type* type);
}

}

#endif
