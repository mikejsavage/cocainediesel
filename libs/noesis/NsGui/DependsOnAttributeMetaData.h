////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_DEPENDSONATTRIBUTEMETADATA_H__
#define __GUI_DEPENDSONATTRIBUTEMETADATA_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/TypeMetaData.h>
#include <NsCore/Symbol.h>
#include <NsGui/CoreApi.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Indicates that the attributed property is dependent on the value of another property.
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API DependsOnAttributeMetaData: public TypeMetaData
{
public:
    DependsOnAttributeMetaData(const char* propertyName);
    DependsOnAttributeMetaData(NsSymbol propertyName);

    /// Gets content property
    NsSymbol GetDependsOnProperty() const;

private:
    NsSymbol mDependsOnProperty;

    NS_DECLARE_REFLECTION(DependsOnAttributeMetaData, TypeMetaData)
};

}


#endif
