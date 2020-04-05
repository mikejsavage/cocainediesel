////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #1476]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_ANIMATION_H__
#define __GUI_ANIMATION_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsGui/BaseAnimation.h>
#include <NsGui/AnimationApi.h>
#include <NsDrawing/Color.h>
#include <NsDrawing/Point.h>
#include <NsDrawing/Rect.h>
#include <NsDrawing/Size.h>
#include <NsDrawing/Thickness.h>


namespace Noesis
{

template<class T> class Nullable;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Animates the value of a T property between two 'target values' using linear interpolation over
/// a specified Duration.
///
/// See `SingleTimelines <Gui.Animation.SingleTimelines.html>`_ for an explanation of target values.
///
/// Existing types are:
/// `DoubleAnimation <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.doubleanimation.aspx>`_,
/// `Int16Animation <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.int16animation.aspx>`_,
/// `Int32Animation <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.int32animation.aspx>`_,
/// `ColorAnimation <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.coloranimation.aspx>`_,
/// `PointAnimation <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.pointanimation.aspx>`_,
/// `RectAnimation <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.rectanimation.aspx>`_,
/// `SizeAnimation <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.sizeanimation.aspx>`_ and
/// `ThicknessAnimation <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.thicknessanimation.aspx>`_
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class NS_GUI_ANIMATION_API Animation: public BaseAnimation
{
public:
    Animation();
    ~Animation();

    /// Gets the type of value this animation generates
    const Type* GetTargetPropertyType() const override;

    /// Gets or sets the total amount by which the animation changes its starting value
    //@{
    const Nullable<T>& GetBy() const;
    void SetBy(const Nullable<T>& by);
    //@}

    /// Gets or sets the animation's starting value
    //@{
    const Nullable<T>& GetFrom() const;
    void SetFrom(const Nullable<T>& from);
    //@}

    /// Gets or sets the animation's ending value
    //@{
    const Nullable<T>& GetTo() const;
    void SetTo(const Nullable<T>& to);
    //@}

    /// From Freezable
    //@{
    Ptr<Animation<T>> Clone() const;
    Ptr<Animation<T>> CloneCurrentValue() const;
    //@}

public:
    /// Dependency properties
    //@{
    static const DependencyProperty* ByProperty;
    static const DependencyProperty* FromProperty;
    static const DependencyProperty* ToProperty;
    //@}

private:
    void ValidateAnimationType();

    T GetEffectiveFrom(bool isAdditive, typename Param<T>::Type origin) const;
    T GetEffectiveTo(bool isAdditive, typename Param<T>::Type origin) const;

    /// From DependencyObject
    //@{
    bool OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) override;
    //@}

    /// From Freezable
    //@{
    void CloneCommonCore(const Freezable* source) override;
    Ptr<Freezable> CreateInstanceCore() const override;
    //@}

    /// From AnimationTimeline
    //@{
    Ptr<BaseComponent> GetAnimatedValue(BaseComponent* defaultOrigin, 
        BaseComponent* defaultDestination, AnimationClock* clock) override;
    Ptr<AnimationTimeline> CreateTransitionFrom() const override;
    Ptr<AnimationTimeline> CreateTransitionTo() const override;
    //@}

private:
    enum AnimationType
    {
        AnimationType_Unknown,
        AnimationType_FromTo,
        AnimationType_FromBy,
        AnimationType_From,
        AnimationType_To,
        AnimationType_By,
        AnimationType_Reset
    };

    AnimationType mAnimationType;

    NS_DECLARE_REFLECTION(Animation, BaseAnimation)
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: Animation<float> is named as DoubleAnimation to be compatible with Blend types
typedef Animation<float> DoubleAnimation;
typedef Animation<int16_t> Int16Animation;
typedef Animation<int32_t> Int32Animation;
typedef Animation<Noesis::Color> ColorAnimation;
typedef Animation<Noesis::Point> PointAnimation;
typedef Animation<Noesis::Rect> RectAnimation;
typedef Animation<Noesis::Size> SizeAnimation;
typedef Animation<Noesis::Thickness> ThicknessAnimation;

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> const char* AnimationIdOf();
template<> inline const char* AnimationIdOf<DoubleAnimation>() { return "DoubleAnimation";}
template<> inline const char* AnimationIdOf<Int16Animation>() { return "Int16Animation";}
template<> inline const char* AnimationIdOf<Int32Animation>() { return "Int32Animation";}
template<> inline const char* AnimationIdOf<ColorAnimation>() { return "ColorAnimation";}
template<> inline const char* AnimationIdOf<PointAnimation>() { return "PointAnimation";}
template<> inline const char* AnimationIdOf<RectAnimation>() { return "RectAnimation";}
template<> inline const char* AnimationIdOf<SizeAnimation>() { return "SizeAnimation";}
template<> inline const char* AnimationIdOf<ThicknessAnimation>() { return "ThicknessAnimation";}

}


#endif
