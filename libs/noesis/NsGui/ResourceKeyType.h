////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #1215]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_RESOURCEKEYTYPE_H__
#define __GUI_RESOURCEKEYTYPE_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/BaseComponent.h>
#include <NsGui/CoreApi.h>
#include <NsGui/IResourceKey.h>


namespace Noesis
{

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A Type being used as a resource dictionary key.
/// Example:
///     <ResourceDictionary>
///         <SolidColorBrush x:Key="{x:Type Button}" Color="Red"/>
///     </ResourceDictionary>
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API ResourceKeyType: public BaseComponent, public IResourceKey
{
public:
    ResourceKeyType();
    ~ResourceKeyType();
    
    // Creates a ResourceKeyType with the type passed as parameter
    static Ptr<ResourceKeyType> Create(const Type* key);

    /// Gets type key
    const Type* Get() const;

    /// From IResourceKey
    //@{
    using BaseComponent::Equals;
    bool Equals(const IResourceKey* resourceKey) const override;
    bool IsLessThan(const IResourceKey* resourceKey) const override;
    NsString GetStr() const override;
    //@}

    NS_IMPLEMENT_INTERFACE_FIXUP

private:
    const Type* mType;

    NS_DECLARE_REFLECTION(ResourceKeyType, BaseComponent)
};

NS_WARNING_POP

}

#endif
