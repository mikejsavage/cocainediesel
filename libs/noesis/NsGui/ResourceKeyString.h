////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #1215]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_RESOURCEKEYSTRING_H__
#define __GUI_RESOURCEKEYSTRING_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/BaseComponent.h>
#include <NsCore/String.h>
#include <NsGui/CoreApi.h>
#include <NsGui/IResourceKey.h>


namespace Noesis
{

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A string being used as a resource dictionary key.
/// Example:
///     <ResourceDictionary>
///         <SolidColorBrush x:Key="DisabledForegroundBrush" Color="Black"/>
///     </ResourceDictionary>
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API ResourceKeyString: public BaseComponent, public IResourceKey
{
public:
    ResourceKeyString();
    ~ResourceKeyString();
    
    // Creates a ResourceKeyString with the string passed as parameter
    static Ptr<ResourceKeyString> Create(const char* str);

    /// Safe version of Create function that could return null
    static Ptr<ResourceKeyString> TryCreate(const char* str);
    
    // Converter helper
    static bool TryParse(const char*, Ptr<ResourceKeyString>& result);

    /// Gets the resource key string
    const char* Get() const;

    /// From IResourceKey
    //@{
    using BaseComponent::Equals;
    bool Equals(const IResourceKey* resourceKey) const override;
    bool IsLessThan(const IResourceKey* resourceKey) const override;
    NsString GetStr() const override;
    //@}

    NS_IMPLEMENT_INTERFACE_FIXUP

protected:
    /// From BaseRefCounted
    //@{
    int32_t OnDestroy() const override;
    //@}

private:
    const NsString* mString;

    NS_DECLARE_REFLECTION(ResourceKeyString, BaseComponent)
};

NS_WARNING_POP

}

#endif
