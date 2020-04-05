////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_SIZECHANGEDINFO_H__
#define __GUI_SIZECHANGEDINFO_H__


#include <NsCore/Noesis.h>
#include <NsGui/CoreApi.h>
#include <NsGui/RoutedEvent.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsDrawing/Size.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Reports a size change. It is used as a parameter in OnRenderSizeChanged overrides.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SizeChangedInfo
{
    /// Gets the new size of the object
    Size newSize;
    /// Gets the previous size of the object
    Size previousSize;
    /// Gets a value that indicates whether the *Width* component of the Size changed
    bool widthChanged;
    /// Gets a value that indicates whether the *Height* component of the Size changed
    bool heightChanged;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Provides data related to the FrameworkElement.SizeChanged event.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API SizeChangedEventArgs: public RoutedEventArgs
{
    SizeChangedInfo sizeChangedInfo;

    SizeChangedEventArgs(BaseComponent* s, const RoutedEvent* e, const SizeChangedInfo& info);
};

}


#endif
