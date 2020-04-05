////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_NSFACTORY_H__
#define __CORE_NSFACTORY_H__


namespace Noesis
{

class Symbol;
template<class T> class Ptr;
class BaseComponent;

}

template<class T>
inline Noesis::Ptr<T> NsCreateComponent(Noesis::Symbol classId);

#include <NsCore/NsFactory.inl>

#endif
