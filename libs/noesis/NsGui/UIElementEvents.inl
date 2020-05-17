////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
inline InputEventArgs::InputEventArgs(BaseComponent* s, const RoutedEvent* e): RoutedEventArgs(s, e) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline MouseButtonEventArgs::MouseButtonEventArgs(BaseComponent* s, const RoutedEvent* e,
    MouseButton button, MouseButtonState state, uint32_t clicks): MouseEventArgs(s, e),
    changedButton(button), buttonState(state), clickCount(clicks) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline MouseWheelEventArgs::MouseWheelEventArgs(BaseComponent* s, const RoutedEvent* e, int delta)
    : MouseEventArgs(s, e), wheelRotation(delta) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline TouchEventArgs::TouchEventArgs(BaseComponent* s, const RoutedEvent* e, const Point& touchPoint_,
    uint64_t touchDevice_) : InputEventArgs(s, e), touchPoint(touchPoint_), touchDevice(touchDevice_) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline ManipulationStartingEventArgs::ManipulationStartingEventArgs(BaseComponent* s,
    const RoutedEvent* e, Visual* manipulationContainer_) : InputEventArgs(s, e),
    manipulationContainer(manipulationContainer_) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline ManipulationStartedEventArgs::ManipulationStartedEventArgs(BaseComponent* s,
    const RoutedEvent* e, Visual* manipulationContainer_, const Point& manipulationOrigin_):
    InputEventArgs(s, e), manipulationContainer(manipulationContainer_),
    manipulationOrigin(manipulationOrigin_) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline ManipulationDeltaEventArgs::ManipulationDeltaEventArgs(BaseComponent* s, const RoutedEvent* e,
    Visual* manipulationContainer_, const Point& manipulationOrigin_,
    const ManipulationDelta& deltaManipulation_, const ManipulationDelta& cumulativeManipulation_,
    const ManipulationVelocities& velocities_, bool isInertial_) : InputEventArgs(s, e),
    manipulationContainer(manipulationContainer_), manipulationOrigin(manipulationOrigin_),
    deltaManipulation(deltaManipulation_), cumulativeManipulation(cumulativeManipulation_),
    velocities(velocities_), isInertial(isInertial_) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline ManipulationInertiaStartingEventArgs::ManipulationInertiaStartingEventArgs(BaseComponent* s,
    const RoutedEvent* e, Visual* manipulationContainer_, const Point& manipulationOrigin_,
    const ManipulationVelocities& initialVelocities_) : InputEventArgs(s, e),
    manipulationContainer(manipulationContainer_), manipulationOrigin(manipulationOrigin_),
    initialVelocities(initialVelocities_) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline ManipulationCompletedEventArgs::ManipulationCompletedEventArgs(BaseComponent* s,
    const RoutedEvent* e, Visual* manipulationContainer_, const Point& manipulationOrigin_,
    const ManipulationVelocities& finalVelocities_, const ManipulationDelta& totalManipulation_,
    bool isInertial_) : InputEventArgs(s, e), manipulationContainer(manipulationContainer_),
    manipulationOrigin(manipulationOrigin_), finalVelocities(finalVelocities_),
    totalManipulation(totalManipulation_), isInertial(isInertial_) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline KeyboardEventArgs::KeyboardEventArgs(BaseComponent* s, const RoutedEvent* e):
    InputEventArgs(s, e) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline KeyboardFocusChangedEventArgs::KeyboardFocusChangedEventArgs(BaseComponent* s,
    const RoutedEvent* e, UIElement* o, UIElement* n) : KeyboardEventArgs(s, e), oldFocus(o), newFocus(n) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline TextCompositionEventArgs::TextCompositionEventArgs(BaseComponent* s, const RoutedEvent* e,
    uint32_t ch_) : InputEventArgs(s, e), ch(ch_) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline QueryCursorEventArgs::QueryCursorEventArgs(BaseComponent* s, const RoutedEvent* e):
    MouseEventArgs(s, e) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline QueryContinueDragEventArgs::QueryContinueDragEventArgs(BaseComponent* source,
    const RoutedEvent* event, bool escapePressed_, uint32_t keyStates_):
    RoutedEventArgs(source, event), escapePressed(escapePressed_), keyStates(keyStates_) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline GiveFeedbackEventArgs::GiveFeedbackEventArgs(BaseComponent* source, const RoutedEvent* event,
    uint32_t effects_, bool useDefaultCursors_): RoutedEventArgs(source, event), effects(effects_),
    useDefaultCursors(useDefaultCursors_) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline DragEventArgs::DragEventArgs(BaseComponent* source, const RoutedEvent* event,
    BaseComponent* data_, uint32_t keyStates_, uint32_t allowedEffects_, UIElement* target_,
    const Point& point): RoutedEventArgs(source, event), data(data_), keyStates(keyStates_),
        allowedEffects(allowedEffects_), effects(allowedEffects_), target(target_), dropPoint(point)
{
    NS_ASSERT(target != nullptr);
}

}
