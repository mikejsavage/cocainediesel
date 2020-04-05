////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_INOTIFYCOLLECTIONCHANGED_H__
#define __GUI_INOTIFYCOLLECTIONCHANGED_H__


#include <NsCore/Noesis.h>
#include <NsGui/CoreApi.h>
#include <NsCore/Interface.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

class BaseComponent;
template<class T> class Delegate;

////////////////////////////////////////////////////////////////////////////////////////////////////
enum NotifyCollectionChangedAction
{
    /// One item was added to the collection
    NotifyCollectionChangedAction_Add,
    /// One item was removed from the collection
    NotifyCollectionChangedAction_Remove,
    /// One item was replaced in the collection
    NotifyCollectionChangedAction_Replace,
    // One items was moved within the collection
    NotifyCollectionChangedAction_Move,
    /// The content of the collection changed dramatically
    NotifyCollectionChangedAction_Reset,
    NotifyCollectionChangedAction_PreReset
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Args passed on collection changed event notification.
////////////////////////////////////////////////////////////////////////////////////////////////////
NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

struct NS_GUI_CORE_API NotifyCollectionChangedEventArgs
{
public:
    NotifyCollectionChangedAction action;
    int oldIndex;
    int newIndex;
    BaseComponent* oldValue;
    BaseComponent* newValue;

    NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction a,
        int oldIdx, int newIdx, BaseComponent* oldVal, BaseComponent* newVal);

private:
    NS_DECLARE_REFLECTION(NotifyCollectionChangedEventArgs, NoParent)
};

NS_WARNING_POP

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef Delegate<void (BaseComponent*, const NotifyCollectionChangedEventArgs&)>
    NotifyCollectionChangedEventHandler;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Notifies listeners of dynamic changes, such as when items get added and removed or the whole
/// list is refreshed.
////////////////////////////////////////////////////////////////////////////////////////////////////
NS_INTERFACE INotifyCollectionChanged: public Interface
{
    /// Occurs when the collection changes
    virtual NotifyCollectionChangedEventHandler& CollectionChanged() = 0;

    NS_IMPLEMENT_INLINE_REFLECTION_(INotifyCollectionChanged, Interface)
};

}


#endif
