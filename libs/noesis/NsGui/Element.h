
////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_ELEMENT_H__
#define __GUI_ELEMENT_H__


#include <NsCore/Noesis.h>
#include <NsGui/CoreApi.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

class DependencyProperty;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Extends UI elements with properties that are not standard in WPF.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API Element
{
    /// Dependency properties
    //@{
    static const DependencyProperty* ProjectionProperty;
    static const DependencyProperty* IsFocusEngagedProperty;
    static const DependencyProperty* IsFocusEngagementEnabledProperty;
    static const DependencyProperty* SupportsFocusEngagementProperty;
    static const DependencyProperty* PPAAModeProperty;
    //@}

    NS_DECLARE_REFLECTION(Element, NoParent)
};

}

#endif
