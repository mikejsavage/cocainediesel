////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_CONTENTPROPERTYMETADATA_H__
#define __GUI_CONTENTPROPERTYMETADATA_H__


#include <NsCore/Noesis.h>
#include <NsCore/BaseComponent.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/TypeMetaData.h>
#include <NsCore/Symbol.h>
#include <NsGui/DependencySystemApi.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// ContentPropertyMetaData. Stores information about the content property of an object
/// Note: A class can't have both ContentPropertyMetaData and DependsOnAttributeMetaData
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_DEPENDENCYSYSTEM_API ContentPropertyMetaData: public TypeMetaData
{
public:
    /// Constructor
    ContentPropertyMetaData(const char* propertyName);

    /// Constructor
    ContentPropertyMetaData(NsSymbol contentProperty);

    /// Gets content property
    NsSymbol GetContentProperty() const;

private:
    NsSymbol mContentProperty;

    NS_DECLARE_REFLECTION(ContentPropertyMetaData, TypeMetaData)
};

}

#endif
