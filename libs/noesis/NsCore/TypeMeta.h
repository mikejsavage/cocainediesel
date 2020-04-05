////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #816]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_TYPEMETA_H__
#define __CORE_TYPEMETA_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>
#include <NsCore/Type.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/MetaData.h>


namespace Noesis
{

class Symbol;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Type with associated metadata
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_CORE_KERNEL_API TypeMeta: public Type
{
public:
    /// Constructor
    TypeMeta(const TypeInfo& typeInfo);

    /// Destructor
    virtual ~TypeMeta() = 0;
    
    /// Returns the container of metadatas
    inline MetaData& GetMetaData();
    inline const MetaData& GetMetaData() const;

    /// Types deriving from TypeMeta can have a portable TypeId associated to them. This TypeId
    /// is needed in different parts of Noesis to uniquely identify a type: component factory,
    /// serialization, etc.
    //@{
    void SetTypeId(Symbol typeId);
    void SetTypeId(const char* typeId);
    Symbol TryGetTypeId() const;
    Symbol GetTypeId() const;
    //@}

private:
    MetaData mMetaData;

    NS_DECLARE_REFLECTION(TypeMeta, Type)
};

}

#include <NsCore/TypeMeta.inl>

#endif
