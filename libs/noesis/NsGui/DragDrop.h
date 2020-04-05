////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_DRAGDROP_H__
#define __GUI_DRAGDROP_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/ReflectionDeclareEnum.h>
#include <NsCore/Delegate.h>
#include <NsGui/CoreApi.h>
#include <NsGui/RoutedEvent.h>
#include <NsDrawing/Point.h>


namespace Noesis
{

class BaseComponent;
class DependencyObject;
class DependencyProperty;
class UIElement;
struct MouseEventArgs;
struct MouseButtonEventArgs;
struct KeyEventArgs;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Specifies the effects of a drag-and-drop operation.
/// This enumaration allows a bitwise combination of its member values.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum DragDropEffects
{
    /// A drop would not be allowed
    DragDropEffects_None = 0,

    /// A copy operation would be performed
    DragDropEffects_Copy = 1,

    /// A move operation would be performed
    DragDropEffects_Move = 2,

    /// A link from the dropped data to the original data would be established
    DragDropEffects_Link = 4,

    /// A drag scroll operation is about to occur or is occurring in the target
    DragDropEffects_Scroll = (int)0x80000000,

    /// All operation is about to occur data is copied or removed from the drag source, and
    /// scrolled in the drop target
    DragDropEffects_All = DragDropEffects_Copy | DragDropEffects_Move | DragDropEffects_Scroll
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Specifies the current state of the modifier keys (SHIFT, CTRL, and ALT), as well as the state of
/// the mouse buttons.
/// This enumaration allows a bitwise combination of its member values.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum DragDropKeyStates
{
    /// No state set
    DragDropKeyStates_None = 0,

    /// The left mouse button
    DragDropKeyStates_LeftMouseButton = 1,

    /// The right mouse button
    DragDropKeyStates_RightMouseButton = 2,

    /// The SHIFT key
    DragDropKeyStates_ShiftKey = 4,

    /// The CTRL key
    DragDropKeyStates_ControlKey = 8,

    /// The middle mouse button
    DragDropKeyStates_MiddleMouseButton = 16,

    /// The ALT key
    DragDropKeyStates_AltKey = 32
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Specifies how and if a drag-and-drop operation should continue.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum DragAction
{
    /// The drag and drop can continue
    DragAction_Continue = 0,

    /// Drop operation should occur
    DragAction_Drop = 1,

    /// Drop operation is canceled
    DragAction_Cancel = 2
};

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251)

////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API QueryContinueDragEventArgs final: public RoutedEventArgs
{
    /// Indicates if Escape key was pressed
    bool escapePressed;

    /// Indicates current states for physical keyboard keys and mouse buttons (DragDropKeyStates)
    uint32_t keyStates;

    /// The action of drag operation
    mutable DragAction action;

    QueryContinueDragEventArgs(BaseComponent* source, const RoutedEvent* event, bool escapePressed,
        uint32_t dragDropKeyStates);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API GiveFeedbackEventArgs final: public RoutedEventArgs
{
    /// The effects of drag operation (DragDropEffects)
    uint32_t effects;

    /// Indicates if default cursors should be used
    mutable bool useDefaultCursors;

    GiveFeedbackEventArgs(BaseComponent* source, const RoutedEvent* event, uint32_t effects,
        bool useDefaultCursors);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API DragEventArgs final: public RoutedEventArgs
{
    /// The data object of drop operation
    BaseComponent* data;

    /// Indicates current states for physical keyboard keys and mouse buttons (DragDropKeyStates)
    uint32_t keyStates;

    /// The allowed effects of drag and drop operation (DragDropEffects)
    uint32_t allowedEffects;

    /// The effects of drag and drop operation (DragDropEffects)
    mutable uint32_t effects;

    /// Returns the point of drop operation that based on relativeTo
    Point GetPosition(UIElement* relativeTo) const;

    DragEventArgs(BaseComponent* source, const RoutedEvent* event, BaseComponent* data,
        uint32_t dragDropKeyStates, uint32_t allowedEffects, UIElement* target, const Point& point);

private:
    UIElement* target;
    Point dropPoint;
};

NS_WARNING_POP

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef Noesis::Delegate<void (DependencyObject* source, BaseComponent* data,
    UIElement* target, const Point& dropPoint, uint32_t effects)> DragDropCompletedCallback;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Provides helper methods and fields for initiating drag-and-drop operations.
///
/// https://msdn.microsoft.com/en-us/library/system.windows.dragdrop.aspx
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_GUI_CORE_API DragDrop
{
    /// Initiates a drag-and-drop operation
    static void DoDragDrop(DependencyObject* source, BaseComponent* data, uint32_t allowedEffects);
    static void DoDragDrop(DependencyObject* source, BaseComponent* data, uint32_t allowedEffects,
        const DragDropCompletedCallback& completedCallback);

    /// Identifies the *PreviewQueryContinueDrag* attached event
    /// \prop
    static const RoutedEvent* PreviewQueryContinueDragEvent;

    /// Identifies the *QueryContinueDrag* attached event
    /// \prop
    static const RoutedEvent* QueryContinueDragEvent;

    /// Identifies the *PreviewGiveFeedback* attached event
    /// \prop
    static const RoutedEvent* PreviewGiveFeedbackEvent;

    /// Identifies the *GiveFeedback* attached event
    /// \prop
    static const RoutedEvent* GiveFeedbackEvent;

    /// Identifies the *PreviewDragEnter* attached event
    /// \prop
    static const RoutedEvent* PreviewDragEnterEvent;

    /// Identifies the *DragEnter* attached event
    /// \prop
    static const RoutedEvent* DragEnterEvent;

    /// Identifies the *PreviewDragOver* attached event
    /// \prop
    static const RoutedEvent* PreviewDragOverEvent;

    /// Identifies the *DragOver* attached event
    /// \prop
    static const RoutedEvent* DragOverEvent;

    /// Identifies the *PreviewDragLeave* attached event
    /// \prop
    static const RoutedEvent* PreviewDragLeaveEvent;

    /// Identifies the *DragLeave* attached event
    /// \prop
    static const RoutedEvent* DragLeaveEvent;

    /// Identifies the *PreviewDrop* attached event
    /// \prop
    static const RoutedEvent* PreviewDropEvent;

    /// Identifies the *Drop* attached event
    /// \prop
    static const RoutedEvent* DropEvent;

    NS_DECLARE_REFLECTION(DragDrop, NoParent);
};

}

NS_DECLARE_REFLECTION_ENUM_EXPORT(NS_GUI_CORE_API, Noesis::DragDropEffects)
NS_DECLARE_REFLECTION_ENUM_EXPORT(NS_GUI_CORE_API, Noesis::DragDropKeyStates)
NS_DECLARE_REFLECTION_ENUM_EXPORT(NS_GUI_CORE_API, Noesis::DragAction)


#endif
