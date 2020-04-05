////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_INAMESCOPETYPES_H__
#define __GUI_INAMESCOPETYPES_H__


#include <NsCore/Noesis.h>
#include <NsCore/BaseComponent.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
enum NameScopeChangedAction
{
    NameScopeChangedAction_Register,
    NameScopeChangedAction_Unregister,
    NameScopeChangedAction_Update,
    NameScopeChangedAction_Destroy
};

////////////////////////////////////////////////////////////////////////////////////////////////////
struct NameScopeChangedArgs
{
    NameScopeChangedAction action;
    const char* name;
    BaseComponent* newElement;
    BaseComponent* oldElement;

    NameScopeChangedArgs(): name(0), newElement(0), oldElement(0) { }
};

}


#endif
