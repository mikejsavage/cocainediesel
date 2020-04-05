////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_PLANEPROJECTION_H__
#define __GUI_PLANEPROJECTION_H__


#include <NsCore/Noesis.h>
#include <NsGui/Projection.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Represents a perspective transform on an object.
///
/// http://msdn.microsoft.com/en-us/library/system.windows.media.planeprojection.aspx
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API PlaneProjection final: public Projection
{
public:
    PlaneProjection();
    ~PlaneProjection();

    /// Constructs a 3D projection matrix from the PlaneProjection property values
    static Matrix4f ConstructProjectionMatrix(const Size& surface, const Size& size,
        float centerX, float centerY, float centerZ,
        float globalOffX, float globalOffY, float globalOffZ,
        float localOffX, float localOffY, float localOffZ,
        float rotX, float rotY, float rotZ);

    /// Gets or sets the x-coordinate of the center of rotation of the object you rotate
    //@{
    float GetCenterOfRotationX() const;
    void SetCenterOfRotationX(float centerX);
    //@}

    /// Gets or sets the y-coordinate of the center of rotation of the object you rotate
    //@{
    float GetCenterOfRotationY() const;
    void SetCenterOfRotationY(float centerY);
    //@}

    /// Gets or sets the z-coordinate of the center of rotation of the object you rotate
    //@{
    float GetCenterOfRotationZ() const;
    void SetCenterOfRotationZ(float centerZ);
    //@}

    /// Gets or sets the distance the object is translated along the x-axis of the screen
    //@{
    float GetGlobalOffsetX() const;
    void SetGlobalOffsetX(float offsetX);
    //@}

    /// Gets or sets the distance the object is translated along the y-axis of the screen
    //@{
    float GetGlobalOffsetY() const;
    void SetGlobalOffsetY(float offsetY);
    //@}

    /// Gets or sets the distance the object is translated along the z-axis of the screen
    //@{
    float GetGlobalOffsetZ() const;
    void SetGlobalOffsetZ(float offsetZ);
    //@}

    /// Gets or sets the distance the object is translated along the x-axis of the plane of the
    /// object
    //@{
    float GetLocalOffsetX() const;
    void SetLocalOffsetX(float offsetX);
    //@}

    /// Gets or sets the distance the object is translated along the y-axis of the plane of the
    /// object
    float GetLocalOffsetY() const;
    void SetLocalOffsetY(float offsetY);
    //@}

    /// Gets or sets the distance the object is translated along the z-axis of the plane of the
    /// object
    //@{
    float GetLocalOffsetZ() const;
    void SetLocalOffsetZ(float offsetZ);
    //@}

    /// Gets the projection matrix of this PlaneProjection
    const Matrix4f& GetProjectionMatrix() const;

    /// Gets or sets the number of degrees to rotate the object around the x-axis of rotation
    //@{
    float GetRotationX() const;
    void SetRotationX(float rotationX);
    //@}

    /// Gets or sets the number of degrees to rotate the object around the y-axis of rotation
    //@{
    float GetRotationY() const;
    void SetRotationY(float rotationY);
    //@}

    /// Gets or sets the number of degrees to rotate the object around the z-axis of rotation
    //@{
    float GetRotationZ() const;
    void SetRotationZ(float rotationZ);
    //@}

    // Hides Freezable methods for convenience
    //@{
    Ptr<PlaneProjection> Clone() const;
    Ptr<PlaneProjection> CloneCurrentValue() const;
    //@}

    /// From Projection
    //@{
    bool IsIdentity() const override;
    Matrix4f GetProjection(const Size& surface, const Size& size) const override;
    //@}

    /// From IRenderProxyCreator
    //@{
    void CreateRenderProxy(RenderTreeUpdater& updater, uint32_t proxyIndex) override;
    void UpdateRenderProxy(RenderTreeUpdater& updater, uint32_t proxyIndex) override;
    void UnregisterRenderer(ViewId viewId) override;
    //@}

public:
    /// Dependency properties
    //@{
    static const DependencyProperty* CenterOfRotationXProperty;
    static const DependencyProperty* CenterOfRotationYProperty;
    static const DependencyProperty* CenterOfRotationZProperty;
    static const DependencyProperty* GlobalOffsetXProperty;
    static const DependencyProperty* GlobalOffsetYProperty;
    static const DependencyProperty* GlobalOffsetZProperty;
    static const DependencyProperty* LocalOffsetXProperty;
    static const DependencyProperty* LocalOffsetYProperty;
    static const DependencyProperty* LocalOffsetZProperty;
    static const DependencyProperty* ProjectionMatrixProperty;
    static const DependencyProperty* RotationXProperty;
    static const DependencyProperty* RotationYProperty;
    static const DependencyProperty* RotationZProperty;
    //@}

protected:
    /// From DependencyObject
    //@{
    bool OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) override;
    //@}

    /// From Freezable
    //@{
    Ptr<Freezable> CreateInstanceCore() const override;
    //@}

private:
    void UpdateMatrix();
    Matrix4f ConstructMatrix(
        float cX, float cY, float cZ,
        float gX, float gY, float gZ,
        float lX, float lY, float lZ,
        float rX, float rY, float rZ) const;

private:
    RenderProxyCreatorFlags mUpdateFlags;

    enum UpdateFlags
    {
        UpdateFlags_CenterOfRotationX,
        UpdateFlags_CenterOfRotationY,
        UpdateFlags_CenterOfRotationZ,
        UpdateFlags_GlobalOffsetX,
        UpdateFlags_GlobalOffsetY,
        UpdateFlags_GlobalOffsetZ,
        UpdateFlags_LocalOffsetX,
        UpdateFlags_LocalOffsetY,
        UpdateFlags_LocalOffsetZ,
        UpdateFlags_RotationX,
        UpdateFlags_RotationY,
        UpdateFlags_RotationZ
    };

    NS_DECLARE_REFLECTION(PlaneProjection, Projection)
};

}


#endif
