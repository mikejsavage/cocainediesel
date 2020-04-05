////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_TYPEENUMCREATOR_H__
#define __CORE_TYPEENUMCREATOR_H__


#include <NsCore/Noesis.h>


namespace Noesis
{

class Type;
class TypeInfo;
class TypeEnum;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeEnumCreator. Helps in the creation of TypeEnum reflection.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class EnumT>
class TypeEnumCreator
{
public:
    /// Creates a TypeEnum for the specified type info
    static Type* Create(const TypeInfo& typeInfo);

    /// Fills TypeEnum with members of the enum
    static void Fill(Type* type);

    /// Adds meta data to the type enum
    template<class T, class ...Args>
    void Meta(Args... args);

    /// Adds an enum value to the type enum
    void Val(const char* name, EnumT value);

private:
    /// Constructor
    TypeEnumCreator(TypeEnum* typeEnum);

private:
    TypeEnum* mTypeEnum;
};

}

/// Inline include
#include <NsCore/TypeEnumCreator.inl>

#endif
