////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_IUITREENODETYPES_H__
#define __GUI_IUITREENODETYPES_H__


#include <NsCore/Noesis.h>
#include <NsCore/Interface.h>

namespace Noesis
{

class BaseComponent;
NS_INTERFACE INameScope;

////////////////////////////////////////////////////////////////////////////////////////////////////
struct ObjectWithNameScope
{
    BaseComponent* object;
    INameScope* scope;

    ObjectWithNameScope(BaseComponent* o = 0, INameScope* s = 0) : object(o), scope(s) { }
};

}


#endif
