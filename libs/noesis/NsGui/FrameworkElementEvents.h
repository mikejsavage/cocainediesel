////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_FRAMEWORKELEMENTEVENTARGS_H__
#define __GUI_FRAMEWORKELEMENTEVENTARGS_H__


#include <NsCore/Noesis.h>
#include <NsGui/CoreApi.h>
#include <NsGui/RoutedEvent.h>
#include <NsDrawing/Rect.h>


namespace Noesis
{

class UIElement;
class DependencyObject;

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Provides data for the context menu event. 
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API ContextMenuEventArgs: public RoutedEventArgs
{
    /// Gets the object that has the ContextMenu that will be opened
    mutable DependencyObject* targetElement;

    /// Gets the horizontal position of the mouse
    float cursorLeft;

    /// Gets the vertical position of the mouse
    float cursorTop;

    ContextMenuEventArgs(BaseComponent* s, const RoutedEvent* e,
        float left = -1.0f, float top = -1.0f);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Provides event information for events that occur when a tooltip opens or closes.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API ToolTipEventArgs: public RoutedEventArgs
{
    ToolTipEventArgs(BaseComponent* s, const RoutedEvent* e);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Provides data for the RequestBringIntoView routed event
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API RequestBringIntoViewEventArgs: public RoutedEventArgs
{
    /// Gets the object that should be made visible in response to the event. 
    DependencyObject* targetObject;

    /// Gets the rectangular region in the object's coordinate space which should be made visible. 
    Rect targetRect;

    RequestBringIntoViewEventArgs(BaseComponent* s, DependencyObject* object, const Rect& rect);
};

NS_WARNING_POP

}

#endif
