////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_LINEARKEYFRAME_H__
#define __GUI_LINEARKEYFRAME_H__


#include <NsCore/Noesis.h>
#include <NsGui/AnimationApi.h>
#include <NsGui/KeyFrame.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

template<class T> const char* LinearKeyFrameIdOf();

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Animates from the T value of the previous key frame to its own *Value* using linear
/// interpolation.
///
/// Existing types are:
/// `LinearDoubleKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.lineardoublekeyframe.aspx>`_,
/// `LinearInt16KeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.linearint16keyframe.aspx>`_,
/// `LinearInt32KeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.linearint32keyframe.aspx>`_,
/// `LinearColorKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.linearcolorkeyframe.aspx>`_,
/// `LinearPointKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.linearpointkeyframe.aspx>`_,
/// `LinearRectKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.linearrectkeyframe.aspx>`_,
/// `LinearSizeKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.linearsizekeyframe.aspx>`_ and
/// `LinearThicknessKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.linearthicknesskeyframe.aspx>`_.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class NS_GUI_ANIMATION_API LinearKeyFrame: public KeyFrame<T>
{
public:
    LinearKeyFrame();
    ~LinearKeyFrame();

    // Hides Freezable methods for convenience
    //@{
    Ptr<LinearKeyFrame<T>> Clone() const;
    Ptr<LinearKeyFrame<T>> CloneCurrentValue() const;
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

    NS_DECLARE_REFLECTION(LinearKeyFrame, KeyFrame<T>)
};

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef LinearKeyFrame<float> LinearDoubleKeyFrame;
typedef LinearKeyFrame<int16_t> LinearInt16KeyFrame;
typedef LinearKeyFrame<int32_t> LinearInt32KeyFrame;
typedef LinearKeyFrame<Noesis::Color> LinearColorKeyFrame;
typedef LinearKeyFrame<Noesis::Point> LinearPointKeyFrame;
typedef LinearKeyFrame<Noesis::Rect> LinearRectKeyFrame;
typedef LinearKeyFrame<Noesis::Size> LinearSizeKeyFrame;
typedef LinearKeyFrame<Noesis::Thickness> LinearThicknessKeyFrame;

////////////////////////////////////////////////////////////////////////////////////////////////////
template<> inline const char* LinearKeyFrameIdOf<DoubleKeyFrame>() 
{ return "LinearDoubleKeyFrame"; }
template<> inline const char* LinearKeyFrameIdOf<Int16KeyFrame>() 
{ return "LinearInt16KeyFrame"; }
template<> inline const char* LinearKeyFrameIdOf<Int32KeyFrame>() 
{ return "LinearInt32KeyFrame"; }
template<> inline const char* LinearKeyFrameIdOf<ColorKeyFrame>() 
{ return "LinearColorKeyFrame"; }
template<> inline const char* LinearKeyFrameIdOf<PointKeyFrame>() 
{ return "LinearPointKeyFrame"; }
template<> inline const char* LinearKeyFrameIdOf<RectKeyFrame>() 
{ return "LinearRectKeyFrame"; }
template<> inline const char* LinearKeyFrameIdOf<SizeKeyFrame>() 
{ return "LinearSizeKeyFrame"; }
template<> inline const char* LinearKeyFrameIdOf<ThicknessKeyFrame>() 
{ return "LinearThicknessKeyFrame"; }

NS_WARNING_POP

}


#endif
