////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_KEYBOARDNAVIGATION_H__
#define __GUI_KEYBOARDNAVIGATION_H__


#include <NsCore/Noesis.h>
#include <NsGui/CoreApi.h>
#include <NsGui/UIElementEvents.h>
#include <NsGui/Enums.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/BaseComponent.h>
#include <NsCore/ReflectionDeclareEnum.h>


namespace Noesis
{

class DependencyObject;
class DependencyProperty;
class Visual;
class UIElement;
class FrameworkElement;
class Keyboard;
class FocusVisualLayer;

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Provides logical and directional navigation between focusable objects.
///
/// http://msdn.microsoft.com/en-us/library/system.windows.input.keyboardnavigation.aspx
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API KeyboardNavigation final: public BaseComponent
{
public:
    KeyboardNavigation();
    KeyboardNavigation(Keyboard* keyboard, Visual* root);
    ~KeyboardNavigation();

    /// Gets View root
    Visual* GetRoot() const;

    /// Gets or sets the logical tab navigation behavior for the children of the element that this 
    /// property is set on.
    //@{
    static KeyboardNavigationMode GetTabNavigation(const DependencyObject* element);
    static void SetTabNavigation(DependencyObject* element, KeyboardNavigationMode mode);
    //@}

    /// Gets or sets the logical control tab navigation behavior for the children of the element
    /// that this property is set on.
    //@{
    static KeyboardNavigationMode GetControlTabNavigation(const DependencyObject* element);
    static void SetControlTabNavigation(DependencyObject* element, KeyboardNavigationMode mode);
    //@}

    /// Gets or sets the directional navigation behavior for the children of the element that this
    /// property is set on. 
    //@{
    static KeyboardNavigationMode GetDirectionalNavigation(const DependencyObject* element);
    static void SetDirectionalNavigation(DependencyObject* element, KeyboardNavigationMode mode);
    //@}

    /// Gets or sets a value indicating whether the Return character is accepted by a control.
    //@{
    static bool GetAcceptsReturn(const DependencyObject* element);
    static void SetAcceptsReturn(DependencyObject* element, bool value);
    //@}

    /// Gets or sets a value indicating whether the element that this property is set on is a tab
    /// stop.
    //@{
    static bool GetIsTabStop(const DependencyObject* element);
    static void SetIsTabStop(DependencyObject* element, bool value);
    //@}

    /// Gets or sets the tab index for the element that this property is set on. 
    //@{
    static int32_t GetTabIndex(const DependencyObject* element);
    static void SetTabIndex(DependencyObject* element, int32_t value);
    //@}

    /// Request to move the focus from source element in the specified direction. Returns true if
    /// focus is moved successfully, returns false if there is no next element.
    /// Note: Internally used by FrameworkElement to implement *MoveFocus()*
    bool MoveFocus(FrameworkElement* source, const TraversalRequest& request);

    /// Returns the element that would receive focus for a specified focus traversal direction,
    /// without actually moving focus to that element.
    /// Note: Internally used by FrameworkElement to implement *PredictFocus()*
    FrameworkElement* PredictFocus(FrameworkElement* source, FocusNavigationDirection direction);

public:
    static const DependencyProperty* AcceptsReturnProperty; // Attached
    static const DependencyProperty* ControlTabNavigationProperty; // Attached
    static const DependencyProperty* DirectionalNavigationProperty; // Attached
    static const DependencyProperty* IsTabStopProperty; // Attached
    static const DependencyProperty* TabIndexProperty; // Attached
    static const DependencyProperty* TabNavigationProperty; // Attached

private:
    friend class Keyboard;
    bool OnKeyDown(Key key);
    void OnFocused(UIElement* focusedElement);

    void UpdateFocus(FrameworkElement* newFocused);
    void UpdateDirectionalFocus(UIElement* focused, FocusNavigationDirection direction,
        bool enableFocusEngagement);

    void SetFocusStyle(FrameworkElement* focusedElement);
    void HideFocusStyle();

private:
    Keyboard* mKeyboard;
    Visual* mRoot;

    Ptr<FocusVisualLayer> mFocusVisualLayer;
    bool mKeyboardFocused;

    NS_DECLARE_REFLECTION(KeyboardNavigation, BaseComponent)
};

NS_WARNING_POP

}


#endif
