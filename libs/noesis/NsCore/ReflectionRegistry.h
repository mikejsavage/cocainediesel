////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #533]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_REFLECTIONREGISTRY_H__
#define __CORE_REFLECTIONREGISTRY_H__


#include <NsCore/CompilerSettings.h>


namespace Noesis
{

class Symbol;
class Type;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Registers reflection types.
////////////////////////////////////////////////////////////////////////////////////////////////////
class ReflectionRegistry
{
public:
    /// Gets a reflection type given a type identifier. This function may fail due to:
    ///  - Not all the types have a TypeId associated to them. To associate a TypeId to a type
    ///    the metadata TypeId must be used
    ///  - Only already registered type are looked up in this query. A type is registered if it
    ///    has been used anytime since initialization. Type registration can be forced in the
    ///    REGISTER_REFLECTION section of each package
    virtual const Type* GetType(Symbol typeId) const = 0;

    /// Safe version of GetType (may return 0)
    virtual const Type* TryGetType(Symbol typeId) const = 0;

    /// The fallback handler is invoked when requesting a type that is not registered
    typedef void (*FallbackHandler)(Symbol typeId, ReflectionRegistry* registry);
    virtual void SetFallbackHandler(FallbackHandler handler) = 0;
};

}

#endif
