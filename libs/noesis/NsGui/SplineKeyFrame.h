////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_SPLINEKEYFRAME_H__
#define __GUI_SPLINEKEYFRAME_H__


#include <NsCore/Noesis.h>
#include <NsGui/AnimationApi.h>
#include <NsGui/KeyFrame.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

class KeySpline;
template<class T> const char* SplineKeyFrameIdOf();

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Animates from the value of the previous key frame to its own value using splined interpolation.
///
/// Existing types are:
/// `SplineDoubleKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.splinedoublekeyframe.aspx>`_,
/// `SplineInt16KeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.splineint16keyframe.aspx>`_,
/// `SplineInt32KeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.splineint32keyframe.aspx>`_,
/// `SplineColorKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.splinecolorkeyframe.aspx>`_,
/// `SplinePointKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.splinepointkeyframe.aspx>`_,
/// `SplineRectKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.splinerectkeyframe.aspx>`_,
/// `SplineSizeKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.splinesizekeyframe.aspx>`_ and
/// `SplineThicknessKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.splinethicknesskeyframe.aspx>`_.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class NS_GUI_ANIMATION_API SplineKeyFrame: public KeyFrame<T>
{
public:
    SplineKeyFrame();
    ~SplineKeyFrame();

    /// Gets or sets the two control points that define animation progress for this key frame
    //@{
    KeySpline* GetKeySpline() const;
    void SetKeySpline(KeySpline* spline);
    //@}

    // Hides Freezable for convenience
    //@{
    Ptr<SplineKeyFrame<T>> Clone() const;
    Ptr<SplineKeyFrame<T>> CloneCurrentValue() const;
    //@}

public:
    /// Dependency properties
    //@{
    static const DependencyProperty* KeySplineProperty;
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
    
    NS_DECLARE_REFLECTION(SplineKeyFrame, KeyFrame<T>)
};

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef SplineKeyFrame<float> SplineDoubleKeyFrame;
typedef SplineKeyFrame<int16_t> SplineInt16KeyFrame;
typedef SplineKeyFrame<int32_t> SplineInt32KeyFrame;
typedef SplineKeyFrame<Noesis::Color> SplineColorKeyFrame;
typedef SplineKeyFrame<Noesis::Point> SplinePointKeyFrame;
typedef SplineKeyFrame<Noesis::Rect> SplineRectKeyFrame;
typedef SplineKeyFrame<Noesis::Size> SplineSizeKeyFrame;
typedef SplineKeyFrame<Noesis::Thickness> SplineThicknessKeyFrame;

////////////////////////////////////////////////////////////////////////////////////////////////////
template<>
inline const char* SplineKeyFrameIdOf<DoubleKeyFrame>() { return "SplineDoubleKeyFrame"; }
template<>
inline const char* SplineKeyFrameIdOf<Int16KeyFrame>() { return "SplineInt16KeyFrame"; }
template<>
inline const char* SplineKeyFrameIdOf<Int32KeyFrame>() { return "SplineInt32KeyFrame"; }
template<>
inline const char* SplineKeyFrameIdOf<ColorKeyFrame>() { return "SplineColorKeyFrame"; }
template<>
inline const char* SplineKeyFrameIdOf<PointKeyFrame>() { return "SplinePointKeyFrame"; }
template<>
inline const char* SplineKeyFrameIdOf<RectKeyFrame>() { return "SplineRectKeyFrame"; }
template<>
inline const char* SplineKeyFrameIdOf<SizeKeyFrame>() { return "SplineSizeKeyFrame"; }
template<>
inline const char* SplineKeyFrameIdOf<ThicknessKeyFrame>() { return "SplineThicknessKeyFrame"; }

NS_WARNING_POP

}


#endif
