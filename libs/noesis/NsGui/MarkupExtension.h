////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #911]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_MARKUPEXTENSION_H__
#define __GUI_MARKUPEXTENSION_H__


#include <NsCore/Noesis.h>
#include <NsCore/BaseComponent.h>
#include <NsGui/CoreApi.h>


namespace Noesis
{

template<class T> class Ptr;
class ValueTargetProvider;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Markup extensions return objects based on string attribute values in XAML, similar to type
/// converters but more powerful. Markups are enclosed in curly braces {}
///
/// Example:
///      <Path Fill="{StaticResource GlyphBrush}"
///            Data="{Binding Path=Content, RelativeSource={RelativeSource TemplatedParent}}"/>
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API MarkupExtension: public BaseComponent
{
public:
    /// Gets the value resulting of evaluating this extension; it might be null
    virtual Ptr<BaseComponent> ProvideValue(const ValueTargetProvider* provider) = 0;

    NS_DECLARE_REFLECTION(MarkupExtension, BaseComponent)
};

}


#endif
