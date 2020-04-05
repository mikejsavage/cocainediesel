////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_KEYFRAME_H__
#define __GUI_KEYFRAME_H__


#include <NsCore/Noesis.h>
#include <NsGui/AnimationApi.h>
#include <NsGui/Freezable.h>
#include <NsGui/IUITreeNode.h>
#include <NsDrawing/Color.h>
#include <NsDrawing/Point.h>
#include <NsDrawing/Rect.h>
#include <NsDrawing/Size.h>
#include <NsDrawing/Thickness.h>
#include <NsCore/String.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

class KeyTime;
template<class T> const char* KeyFrameIdOf();

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Defines an animation segment with its own target value and interpolation method for an
/// AnimationUsingKeyFrames.
///
/// Existing types are:
/// `BooleanKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.booleankeyframe.aspx>`_,
/// `DoubleKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.doublekeyframe.aspx>`_,
/// `Int16KeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.int16keyframe.aspx>`_,
/// `Int32KeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.int32keyframe.aspx>`_,
/// `ColorKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.colorkeyframe.aspx>`_,
/// `PointKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.pointkeyframe.aspx>`_,
/// `RectKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.rectkeyframe.aspx>`_,
/// `SizeKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.sizekeyframe.aspx>`_,
/// `ThicknessKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.thicknesskeyframe.aspx>`_,
/// `ObjectKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.objectkeyframe.aspx>`_ and
/// `StringKeyFrame <http://msdn.microsoft.com/en-us/library/system.windows.media.animation.stringkeyframe.aspx>`_.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class NS_GUI_ANIMATION_API KeyFrame: public Freezable, public IUITreeNode
{
public:
    KeyFrame();
    ~KeyFrame();

    /// Gets or sets the time at which the key frame's target KeyFrame::Value should be reached
    //@{
    const KeyTime& GetKeyTime() const;
    void SetKeyTime(const KeyTime& time);
    //@}

    // Property Type  | ValueType
    // -----------------------------------------
    // Ptr<T>           T*
    // NsString         const char*
    // T                T (basic types) or T&
    typedef typename Noesis::SetValueType<T>::Type ValueType;

    /// Gets or sets the key frame's target value
    //@{
    ValueType GetValue() const;
    void SetValue(ValueType value);
    //@}

    // Property Type | ParamType
    // --------------------------------
    // Ptr<T>          const Ptr<T>&
    // Color           const Color&
    // NsString        const NsString&
    // T               T (basic types)
    typedef typename Noesis::Param<T>::Type ParamType;

    /// Interpolates the base value by the specified progress to reach this KeyFrame value
    Ptr<BaseComponent> InterpolateValue(ParamType baseValue, float keyFrameProgress);

    /// Helper methods to be used from AnimationUsingKeyFrames (could be eliminated if the later
    /// class uses another template argument with the type of the value)
    //@{
    static Ptr<BaseComponent> BoxValue(ParamType value);
    static T UnboxValue(BaseComponent* value);

    /// Calculates the distance between 2 values
    static float Len(ParamType a, ParamType b);
    //@}

    // Hides Freezable methods for convenience
    //@{
    Ptr<KeyFrame<T>> Clone() const;
    Ptr<KeyFrame<T>> CloneCurrentValue() const;
    //@}

    /// From IUITreeNode
    //@{
    IUITreeNode* GetNodeParent() const override;
    void SetNodeParent(IUITreeNode* parent) override;
    BaseComponent* FindNodeResource(IResourceKey* key, bool fullElementSearch) const override;
    BaseComponent* FindNodeName(const char* name) const override;
    ObjectWithNameScope FindNodeNameAndScope(const char* name) const override;
    //@}

    NS_IMPLEMENT_INTERFACE_FIXUP

public:
    /// Dependency properties
    //@{
    static const DependencyProperty* KeyTimeProperty;
    static const DependencyProperty* ValueProperty;
    //@}

protected:
    virtual T InterpolateValueCore(ParamType baseValue, float keyFrameProgress) = 0;

    /// From DependencyObject
    //@{
    void OnObjectValueSet(BaseComponent* oldValue, BaseComponent* newValue) override;
    //@}

private:
    IUITreeNode* mOwner;

    NS_DECLARE_REFLECTION(KeyFrame, Freezable)
};

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef KeyFrame<bool> BooleanKeyFrame;
typedef KeyFrame<float> DoubleKeyFrame;
typedef KeyFrame<int16_t> Int16KeyFrame;
typedef KeyFrame<int32_t> Int32KeyFrame;
typedef KeyFrame<Noesis::Color> ColorKeyFrame;
typedef KeyFrame<Noesis::Point> PointKeyFrame;
typedef KeyFrame<Noesis::Rect> RectKeyFrame;
typedef KeyFrame<Noesis::Size> SizeKeyFrame;
typedef KeyFrame<Noesis::Thickness> ThicknessKeyFrame;
typedef KeyFrame<Ptr<Noesis::BaseComponent> > ObjectKeyFrame;
typedef KeyFrame<NsString> StringKeyFrame;

////////////////////////////////////////////////////////////////////////////////////////////////////
template<> inline const char* KeyFrameIdOf<BooleanKeyFrame>() { return "BooleanKeyFrame"; }
template<> inline const char* KeyFrameIdOf<DoubleKeyFrame>() { return "DoubleKeyFrame"; }
template<> inline const char* KeyFrameIdOf<Int16KeyFrame>() { return "Int16KeyFrame"; }
template<> inline const char* KeyFrameIdOf<Int32KeyFrame>() { return "Int32KeyFrame"; }
template<> inline const char* KeyFrameIdOf<ColorKeyFrame>() { return "ColorKeyFrame"; }
template<> inline const char* KeyFrameIdOf<PointKeyFrame>() { return "PointKeyFrame"; }
template<> inline const char* KeyFrameIdOf<RectKeyFrame>() { return "RectKeyFrame"; }
template<> inline const char* KeyFrameIdOf<SizeKeyFrame>() { return "SizeKeyFrame"; }
template<> inline const char* KeyFrameIdOf<ThicknessKeyFrame>() { return "ThicknessKeyFrame"; }
template<> inline const char* KeyFrameIdOf<ObjectKeyFrame>() { return "ObjectKeyFrame"; }
template<> inline const char* KeyFrameIdOf<StringKeyFrame>() { return "StringKeyFrame"; }

NS_WARNING_POP

}


#endif
