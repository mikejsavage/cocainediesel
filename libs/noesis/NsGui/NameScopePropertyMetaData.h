////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_NAMESCOPEPROPERTYMETADATA_H__
#define __GUI_NAMESCOPEPROPERTYMETADATA_H__


#include <NsCore/Noesis.h>
#include <NsGui/CoreApi.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/TypeMetaData.h>
#include <NsCore/Symbol.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// The component that contains this metadata can be registered into a NameScope using the value of
/// the property specified by the metadata
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API NameScopePropertyMetaData: public TypeMetaData
{
public:
    NameScopePropertyMetaData(const char* nameScopeProperty);
    NameScopePropertyMetaData(NsSymbol nameScopeProperty);
    ~NameScopePropertyMetaData();

    NsSymbol GetNameScopeProperty() const;

private:
    NsSymbol mNameScopeProperty;

    NS_DECLARE_REFLECTION(NameScopePropertyMetaData, TypeMetaData)
};

}


#endif
