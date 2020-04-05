////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_EASINGKEYFRAME_H__
#define __GUI_EASINGKEYFRAME_H__


#include <NsCore/Noesis.h>
#include <NsGui/AnimationApi.h>
#include <NsGui/KeyFrame.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

class EasingFunctionBase;
template<class T> const char* EasingKeyFrameIdOf();

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Defines a property that enables you to associate an easing function with an 
/// AnimationUsingKeyFrames key-frame animation.
///
/// Existing types are:
/// `EasingDoubleKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.easingdoublekeyframe.aspx>`_,
/// `EasingInt16KeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.easingint16keyframe.aspx>`_,
/// `EasingInt32KeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.easingint32keyframe.aspx>`_,
/// `EasingColorKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.easingcolorkeyframe.aspx>`_,
/// `EasingPointKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.easingpointkeyframe.aspx>`_,
/// `EasingRectKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.easingrectkeyframe.aspx>`_,
/// `EasingSizeKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.easingsizekeyframe.aspx>`_ and
/// `EasingThicknessKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.easingthicknesskeyframe.aspx>`_.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class NS_GUI_ANIMATION_API EasingKeyFrame: public KeyFrame<T>
{
public:
    EasingKeyFrame();
    ~EasingKeyFrame();

    /// Gets or sets the easing function applied to the key frame
    //@{
    EasingFunctionBase* GetEasingFunction() const;
    void SetEasingFunction(EasingFunctionBase* function);
    //@}

    // Hides Freezable methods for convenience
    //@{
    Ptr<EasingKeyFrame<T>> Clone() const;
    Ptr<EasingKeyFrame<T>> CloneCurrentValue() const;
    //@}

public:
    /// Dependency properties
    //@{
    static const DependencyProperty* EasingFunctionProperty;
    //@}

protected:
    /// From Freezable
    //@{
    Ptr<Freezable> CreateInstanceCore() const override;
    //@}

    /// From KeyFrame
    //@{
    typedef typename Noesis::Param<T>::Type ParamType;
    T InterpolateValueCore(ParamType baseValue, float keyFrameProgress) override;
    //@}

    NS_DECLARE_REFLECTION(EasingKeyFrame, KeyFrame<T>)
};

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef EasingKeyFrame<float> EasingDoubleKeyFrame;
typedef EasingKeyFrame<int16_t> EasingInt16KeyFrame;
typedef EasingKeyFrame<int32_t> EasingInt32KeyFrame;
typedef EasingKeyFrame<Noesis::Color> EasingColorKeyFrame;
typedef EasingKeyFrame<Noesis::Point> EasingPointKeyFrame;
typedef EasingKeyFrame<Noesis::Rect> EasingRectKeyFrame;
typedef EasingKeyFrame<Noesis::Size> EasingSizeKeyFrame;
typedef EasingKeyFrame<Noesis::Thickness> EasingThicknessKeyFrame;

////////////////////////////////////////////////////////////////////////////////////////////////////
template<> inline const char* EasingKeyFrameIdOf<DoubleKeyFrame>() 
{ return "EasingDoubleKeyFrame"; }
template<> inline const char* EasingKeyFrameIdOf<Int16KeyFrame>() 
{ return "EasingInt16KeyFrame"; }
template<> inline const char* EasingKeyFrameIdOf<Int32KeyFrame>() 
{ return "EasingInt32KeyFrame"; }
template<> inline const char* EasingKeyFrameIdOf<ColorKeyFrame>() 
{ return "EasingColorKeyFrame"; }
template<> inline const char* EasingKeyFrameIdOf<PointKeyFrame>() 
{ return "EasingPointKeyFrame"; }
template<> inline const char* EasingKeyFrameIdOf<RectKeyFrame>() 
{ return "EasingRectKeyFrame"; }
template<> inline const char* EasingKeyFrameIdOf<SizeKeyFrame>() 
{ return "EasingSizeKeyFrame"; }
template<> inline const char* EasingKeyFrameIdOf<ThicknessKeyFrame>() 
{ return "EasingThicknessKeyFrame"; }

NS_WARNING_POP

}


#endif
