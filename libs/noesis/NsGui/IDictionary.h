////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_IDICTIONARY_H__
#define __GUI_IDICTIONARY_H__


#include <NsCore/Noesis.h>
#include <NsCore/Interface.h>


namespace Noesis
{

template<class T> class Ptr;
class BaseComponent;
NS_INTERFACE IResourceKey;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Interface to a dictionary container of generic items using strings or Types as keys.
/// Note: String keys are case-sensitive.
////////////////////////////////////////////////////////////////////////////////////////////////////
NS_INTERFACE IDictionary: public Interface
{
    /// Looks for an element in the dictionary.
    /// Returns true if element was found, false otherwise
    virtual bool Find(IResourceKey* key, Ptr<BaseComponent>& resource) const = 0;

    /// Sets the element with the specified key
    virtual void Set(IResourceKey* key, BaseComponent* value) = 0;

    /// Adds an element with the provided key and value to the dictionary
    virtual void Add(IResourceKey* key, BaseComponent* value) = 0;

    NS_IMPLEMENT_INLINE_REFLECTION_(IDictionary, Interface)
};

}


#endif
