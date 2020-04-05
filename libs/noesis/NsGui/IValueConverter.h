////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_IVALUECONVERTER_H__
#define __CORE_IVALUECONVERTER_H__


#include <NsCore/Noesis.h>
#include <NsCore/Interface.h>


namespace Noesis
{

template<class T> class Ptr;
class BaseComponent;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Provides a way to apply custom logic to a binding.
////////////////////////////////////////////////////////////////////////////////////////////////////
NS_INTERFACE IValueConverter: public Interface
{
    /// The data binding engine calls this method when it propagates a value from the binding
    /// source to the binding target
    virtual bool TryConvert(BaseComponent* value, const Type* targetType, BaseComponent* parameter, 
        Ptr<BaseComponent>& result) = 0;

    /// The data binding engine calls this method when it propagates a value from the binding
    /// target to the binding source
    virtual bool TryConvertBack(BaseComponent* value, const Type* targetType,
        BaseComponent* parameter, Ptr<BaseComponent>& result) = 0;

    NS_IMPLEMENT_INLINE_REFLECTION_(IValueConverter, Interface)
};

}


#endif
