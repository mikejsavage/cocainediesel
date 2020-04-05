////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_ANIMATIONUSINGKEYFRAMES_H__
#define __GUI_ANIMATIONUSINGKEYFRAMES_H__


#include <NsCore/Noesis.h>
#include <NsGui/AnimationApi.h>
#include <NsGui/AnimationTimeline.h>
#include <NsGui/Duration.h>
#include <NsDrawing/Color.h>
#include <NsDrawing/Point.h>
#include <NsDrawing/Rect.h>
#include <NsDrawing/Size.h>
#include <NsDrawing/Thickness.h>
#include <NsCore/String.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

template<class T> class KeyFrame;
template<class T> class FreezableCollection;
template<class T> const char* AnimationUsingKeyFramesIdOf();

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Animates the value of a T property along a set of *KeyFrames*.
///
/// Existing types are:
/// `BooleanAnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.booleananimationusingkeyframes.aspx>`_,
/// `DoubleAnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.doubleanimationusingkeyframes.aspx>`_,
/// `Int16AnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.int16animationusingkeyframes.aspx>`_,
/// `Int32AnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.int32animationusingkeyframes.aspx>`_,
/// `ColorAnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.coloranimationusingkeyframes.aspx>`_,
/// `PointAnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.pointanimationusingkeyframes.aspx>`_,
/// `RectAnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.rectanimationusingkeyframes.aspx>`_,
/// `SizeAnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.sizeanimationusingkeyframes.aspx>`_,
/// `ThicknessAnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.thicknessanimationusingkeyframes.aspx>`_,
/// `ObjectAnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.objectanimationusingkeyframes.aspx>`_ and
/// `StringAnimationUsingKeyFrames <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.stringanimationusingkeyframes.aspx>`_
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class NS_GUI_ANIMATION_API AnimationUsingKeyFrames final: public AnimationTimeline
{
public:
    AnimationUsingKeyFrames();
    ~AnimationUsingKeyFrames();

    /// Gets the type of value this animation generates
    const Type* GetTargetPropertyType() const override;

    /// Gets the collection of KeyFrame objects that define the animation
    FreezableCollection<KeyFrame<T>>* GetKeyFrames() const;

    // Hides Freezable methods for convenience
    //@{
    Ptr<AnimationUsingKeyFrames<T>> Clone() const;
    Ptr<AnimationUsingKeyFrames<T>> CloneCurrentValue() const;
    //@}

    /// From AnimationTimeline
    //@{
    Ptr<BaseComponent> GetAnimatedValue(BaseComponent* defaultOrigin,
        BaseComponent* defaultDestination, AnimationClock* clock) override;
    //@}

private:
    void EnsureKeyFrames() const;

    void OrderFrames(Clock* clock);
    int FindPrevFrame(uint32_t current, bool checkUniforms) const;
    int FindNextFrame(uint32_t current, bool checkUniforms) const;

    /// From DependencyObject
    //@{
    void OnInit() override;
    //@}

    /// From Freezable
    //@{
    void CloneCommonCore(const Freezable* source) override;
    Ptr<Freezable> CreateInstanceCore() const override;
    //@}

    /// From Timeline
    //@{
    Duration GetNaturalDuration(Clock* clock) const override;
    Duration GetEffectiveDuration() const override;
    //@}

    /// From AnimationTimeline
    //@{
    Ptr<AnimationTimeline> CreateTransitionFrom() const override;
    Ptr<AnimationTimeline> CreateTransitionTo() const override;
    //@}

private:
    mutable Ptr<FreezableCollection<KeyFrame<T>>> mKeyFrames;

    NsVector<eastl::pair<Ptr<KeyFrame<T>>, double>> mOrderedFrames;
    mutable Duration mNaturalDuration;

    NS_DECLARE_REFLECTION(AnimationUsingKeyFrames, AnimationTimeline)
};

NS_WARNING_POP

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef AnimationUsingKeyFrames<bool> BooleanAnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<float> DoubleAnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<int16_t> Int16AnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<int32_t> Int32AnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<Noesis::Color> ColorAnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<Noesis::Point> PointAnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<Noesis::Rect> RectAnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<Noesis::Size> SizeAnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<Noesis::Thickness> ThicknessAnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<Ptr<Noesis::BaseComponent>> ObjectAnimationUsingKeyFrames;
typedef AnimationUsingKeyFrames<NsString> StringAnimationUsingKeyFrames;

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef FreezableCollection<KeyFrame<bool>> BooleanKeyFrameCollection;
typedef FreezableCollection<KeyFrame<float>> DoubleKeyFrameCollection;
typedef FreezableCollection<KeyFrame<int16_t>> Int16KeyFrameCollection;
typedef FreezableCollection<KeyFrame<int32_t>> Int32KeyFrameCollection;
typedef FreezableCollection<KeyFrame<Noesis::Color>> ColorKeyFrameCollection;
typedef FreezableCollection<KeyFrame<Noesis::Point>> PointKeyFrameCollection;
typedef FreezableCollection<KeyFrame<Noesis::Rect>> RectKeyFrameCollection;
typedef FreezableCollection<KeyFrame<Noesis::Size>> SizeKeyFrameCollection;
typedef FreezableCollection<KeyFrame<Noesis::Thickness>> ThicknessKeyFrameCollection;
typedef FreezableCollection<KeyFrame<Ptr<Noesis::BaseComponent>>> ObjectKeyFrameCollection;
typedef FreezableCollection<KeyFrame<NsString>> StringKeyFrameCollection;

////////////////////////////////////////////////////////////////////////////////////////////////////
template<> inline const char* AnimationUsingKeyFramesIdOf<BooleanAnimationUsingKeyFrames>() 
{ return "BooleanAnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<DoubleAnimationUsingKeyFrames>() 
{ return "DoubleAnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<Int16AnimationUsingKeyFrames>() 
{ return "Int16AnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<Int32AnimationUsingKeyFrames>() 
{ return "Int32AnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<ColorAnimationUsingKeyFrames>() 
{ return "ColorAnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<PointAnimationUsingKeyFrames>() 
{ return "PointAnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<RectAnimationUsingKeyFrames>() 
{ return "RectAnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<SizeAnimationUsingKeyFrames>() 
{ return "SizeAnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<ThicknessAnimationUsingKeyFrames>() 
{ return "ThicknessAnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<ObjectAnimationUsingKeyFrames>() 
{ return "ObjectAnimationUsingKeyFrames"; }
template<> inline const char* AnimationUsingKeyFramesIdOf<StringAnimationUsingKeyFrames>() 
{ return "StringAnimationUsingKeyFrames"; }

}


#endif
