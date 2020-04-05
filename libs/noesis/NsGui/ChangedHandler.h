////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_CHANGEDHANDLER_H__
#define __GUI_CHANGEDHANDLER_H__


#include <NsCore/Noesis.h>
#include <NsGui/FreezableEventReason.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

class Freezable;
class DependencyObject;
class DependencyProperty;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChangedHandler. Handlers are created to check changes of objects stored inside a dependency
/// property (a freezable object or an expression object).
////////////////////////////////////////////////////////////////////////////////////////////////////
class ChangedHandler
{
public:
    ChangedHandler();
    ~ChangedHandler();

    /// Connects this handler to the specified freezable
    void Attach(Freezable* object, DependencyObject* owner, const DependencyProperty* ownerProp);

    /// Disconnects this handler from the freezable
    void Detach();

private:
    /// Callback function for object changes
    void OnChanged(Freezable* freezable, FreezableEventReason reason);

private:
    Freezable* mFreezable;
    DependencyObject* mOwner;
    const DependencyProperty* mOwnerProperty;
};

}

#endif
