////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_CONVERTER_H__
#define __CORE_CONVERTER_H__


#include <NsCore/Noesis.h>
#include <NsCore/TypeConverter.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>
#include <NsCore/IdOf.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Converter for types implementing TryParse()
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class Converter: public TypeConverter
{
public:
    /// From TypeConverter
    //@{
    bool CanConvertFrom(const Type* type) const override;
    bool TryConvertFromString(const char* str, Ptr<BaseComponent>& result) const override;
    //@}

    NS_IMPLEMENT_INLINE_REFLECTION(Converter, TypeConverter)
    {
        NsMeta<TypeId>(IdOf<T>("Converter"));
    }
};

}


#include <NsCore/Converter.inl>


#endif
