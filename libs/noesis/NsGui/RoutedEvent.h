////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_ROUTEDEVENT_H__
#define __GUI_ROUTEDEVENT_H__


#include <NsCore/Noesis.h>
#include <NsCore/BaseComponent.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/ReflectionImplementEmpty.h>
#include <NsCore/Symbol.h>
#include <NsCore/CompilerTools.h>
#include <NsGui/CoreApi.h>
#include <NsGui/Events.h>


namespace Noesis
{

template<class T> class Delegate;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Defines the routing strategy of the dependency event.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum RoutingStrategy
{
    /// Event is routed to children elements of the tree
    RoutingStrategy_Tunnel,

    /// Event is routed to the root of the element tree
    RoutingStrategy_Bubble,

    /// Event is routed directly to a specific element in the tree
    RoutingStrategy_Direct
};

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// An event instance that propagates through a tree of related elements. There are three types of
/// routing: bubbling, tunneling and direct. With bubbling, the event instance moves from the
/// source of the event up to the top of the tree. With tunneling, the event instance starts at
/// the top of the tree and moves down to the source of the event. With direct routing, the event
/// instance behaves like a standard event.
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API RoutedEvent: public BaseComponent
{
public:
    RoutedEvent(NsSymbol name);
    RoutedEvent(NsSymbol name, const TypeClass* ownerType, RoutingStrategy routingStrategy);
    ~RoutedEvent();

    /// Gets event name
    inline NsSymbol GetName() const;

    /// Gets or sets event owner type
    //@{
    inline const TypeClass* GetOwnerType() const;
    void SetOwnerType(const TypeClass* owner);
    //@}

    /// Gets or sets event routing strategy
    //@{
    inline RoutingStrategy GetRoutingStrategy() const;
    void SetRoutingStrategy(RoutingStrategy strategy);
    //@}

private:
    NsSymbol mName;
    const TypeClass* mOwnerType;
    RoutingStrategy mRoutingStrategy;

    NS_DECLARE_REFLECTION(RoutedEvent, BaseComponent)
};

NS_WARNING_POP

/// Search for routed events in the class hierarchy
NS_GUI_CORE_API const RoutedEvent* FindRoutedEvent(const TypeClass* type, NsSymbol eventId);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Contains state information and event arguments associated with a routed event.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API RoutedEventArgs: public EventArgs
{
public:
    BaseComponent* source;
    const RoutedEvent* routedEvent;
    mutable bool handled;

    RoutedEventArgs(BaseComponent* s, const RoutedEvent* e);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Represents the delegate for handlers that receive dependency events.
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef Noesis::Delegate<void (BaseComponent*, const RoutedEventArgs&)> RoutedEventHandler;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// RoutedPropertyChangedEventArgs
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct RoutedPropertyChangedEventArgs: public RoutedEventArgs
{
    T oldValue;
    T newValue;

    RoutedPropertyChangedEventArgs(BaseComponent* s, const RoutedEvent* e, 
        typename Param<T>::Type oldV, typename Param<T>::Type newV): 
        RoutedEventArgs(s, e), oldValue(oldV), newValue(newV)
    {
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct RoutedPropertyChangedEventHandler
{
    typedef Delegate<void (BaseComponent*, const RoutedPropertyChangedEventArgs<T>&)> Handler;
};

}

#include <NsGui/RoutedEvent.inl>


#endif
