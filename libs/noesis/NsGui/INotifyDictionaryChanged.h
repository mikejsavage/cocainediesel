////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_INOTIFYDICTIONARYCHANGED_H__
#define __GUI_INOTIFYDICTIONARYCHANGED_H__


#include <NsCore/Noesis.h>
#include <NsGui/CoreApi.h>
#include <NsCore/Interface.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

class BaseComponent;
template<class T> class Delegate;
NS_INTERFACE IResourceKey;

////////////////////////////////////////////////////////////////////////////////////////////////////
enum NotifyDictionaryChangedAction
{
    /// One item was added to the collection. 
    NotifyDictionaryChangedAction_Add,
    /// One item was removed from the collection. 
    NotifyDictionaryChangedAction_Remove,
    /// One item was replaced in the collection. 
    NotifyDictionaryChangedAction_Replace, 
    /// The content of the collection changed dramatically. 
    NotifyDictionaryChangedAction_Reset,
    NotifyDictionaryChangedAction_PreReset
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Args passed on dictionary changed event notification.
////////////////////////////////////////////////////////////////////////////////////////////////////
NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

struct NS_GUI_CORE_API NotifyDictionaryChangedEventArgs
{
public:
    NotifyDictionaryChangedAction action;
    const IResourceKey* key;
    const BaseComponent* oldValue;
    const BaseComponent* newValue;

    NotifyDictionaryChangedEventArgs(NotifyDictionaryChangedAction act, const IResourceKey* k,
        const BaseComponent* oldVal, const BaseComponent* newVal);

private:
    NS_DECLARE_REFLECTION(NotifyDictionaryChangedEventArgs, NoParent)
};

NS_WARNING_POP

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef Delegate<void (BaseComponent*, const NotifyDictionaryChangedEventArgs&)>
    NotifyDictionaryChangedEventHandler;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Notifies listeners of dynamic changes, such as when items get added and removed or the whole
/// dictionary is refreshed.
////////////////////////////////////////////////////////////////////////////////////////////////////
NS_INTERFACE INotifyDictionaryChanged: public Interface
{
    /// Occurs when the dictionary changes
    virtual NotifyDictionaryChangedEventHandler& DictionaryChanged() = 0;

    NS_IMPLEMENT_INLINE_REFLECTION_(INotifyDictionaryChanged, Interface)
};

}


#endif
