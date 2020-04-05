////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __MATH_TRANSFORM_H__
#define __MATH_TRANSFORM_H__


#include <NsCore/Noesis.h>
#include <NsMath/VectorMathApi.h>
#include <NsMath/Vector.h>
#include <NsMath/Matrix.h>
#include <NsCore/Error.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/NSTLForwards.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Affine 2D transformation. 
/// Internally, the transform is implemented in terms of a Matrix2 and a Vector2.
/// The interface of Transform2 is similar to Matrix3, but instead of storing 9 elements, it 
/// stores only 6 elements (last column is supposed to be (0,0,1))
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class Transform2
{
public:
    /// Default Constructor. Does no initialization
    inline Transform2() {}
    
    // Constructor from explicit floats
    inline Transform2(T v00, T v01, T v10, T v11, T v20, T v21)
    {
        mVal[0] = Vector2<T>(v00, v01);
        mVal[1] = Vector2<T>(v10, v11);
        mVal[2] = Vector2<T>(v20, v21);
    }

    // Constructor from explicit vectors
    inline Transform2(const Vector2<T>& v0, const Vector2<T>& v1, const Vector2<T>& v2)
    {
        mVal[0] = v0;
        mVal[1] = v1;
        mVal[2] = v2;
    }

    /// Constructor from pointer
    inline Transform2(const T* values)
    {
        NS_ASSERT(values != 0);
        
        mVal[0] = Vector2<T>(values[0], values[1]);
        mVal[1] = Vector2<T>(values[2], values[3]);
        mVal[2] = Vector2<T>(values[4], values[5]);
    }

    /// Copy constructor
    inline Transform2(const Transform2& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        mVal[2] = m[2];
    }

    /// Assignment
    inline Transform2& operator=(const Transform2& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        mVal[2] = m[2];
        
        return *this;
    }

    /// Accessors
    //@{
    inline Vector2<T>& operator[](uint32_t i)
    {
        NS_ASSERT(i < 3);
        return mVal[i];
    }
    inline const Vector2<T>& operator[](uint32_t i) const
    {
        NS_ASSERT(i < 3);
        return mVal[i];
    }
    inline const T* GetData() const
    {
        return mVal[0].GetData();
    }
    inline void SetUpper2x2(const Matrix2<T>& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
    }
    inline void SetTranslation(const Vector2<T>& v)
    {
        mVal[2] = v;
    }
    inline const Matrix2<T>& GetUpper2x2() const
    {
        return *reinterpret_cast<const Matrix2<T>*>(mVal[0].GetData());
    }
    inline const Vector2<T>& GetTranslation() const
    {
        return mVal[2];
    }
    //@}

    /// In-Place operators
    //@{
    inline Transform2& operator*=(const Transform2& m)
    {
        mVal[0] = Vector2<T>(mVal[0][0] * m[0][0] + mVal[0][1] * m[1][0],
            mVal[0][0] * m[0][1] + mVal[0][1] * m[1][1]);
                        
        mVal[1] = Vector2<T>(mVal[1][0] * m[0][0] + mVal[1][1] * m[1][0],
            mVal[1][0] * m[0][1] + mVal[1][1] * m[1][1]);
                           
        mVal[2] = Vector2<T>(mVal[2][0] * m[0][0] + mVal[2][1] * m[1][0] + m[2][0],
            mVal[2][0] * m[0][1] + mVal[2][1] * m[1][1] + m[2][1]);   

        return *this;
    }
    inline Transform2& operator*=(T v)
    {
        mVal[0] *= v;
        mVal[1] *= v;
        mVal[2] *= v;
        
        return *this;
    }
    inline Transform2& operator/=(T v)
    {
        mVal[0] /= v;
        mVal[1] /= v;
        mVal[2] /= v;
        
        return *this;
    }
    
    inline void RotateAt(T radians, T centerX, T centerY)
    {
        *this = PostTrans(*this, -centerX, -centerY);
        *this = PostRot(*this, radians);
        *this = PostTrans(*this, centerX, centerY);
    }
    
    inline void ScaleAt(T scaleX, T scaleY, T centerX, T centerY)
    {
        *this = PostTrans(*this, -centerX, -centerY);
        *this = PostScale(*this, scaleX, scaleY);
        *this = PostTrans(*this, centerX, centerY);
    }
    
    inline void Translate(T dx, T dy)
    {
        *this = PostTrans(*this, dx, dy);
    }
    //@}

    /// Logic operators
    //@{
    inline bool operator==(const Transform2& m) const
    {
        return mVal[0] == m[0] && mVal[1] == m[1] && mVal[2] == m[2];
    }
    inline bool operator!=(const Transform2& m) const
    {
        return !(operator==(m));
    }
    //@}

    /// Static functions
    //@{
    static Transform2 Identity()
    {
        return Transform2<T>(Vector2<T>(T(1.0), T(0.0)),
            Vector2<T>(T(0.0), T(1.0)),
            Vector2<T>(T(0.0), T(0.0)));
    }
    static Transform2 Scale(T scaleX, T scaleY)
    {
        return Transform2<T>(Vector2<T>(scaleX, T(0.0)),
            Vector2<T>(T(0.0), scaleY),
            Vector2<T>(T(0.0), T(0.0)));
    }

    static Transform2 Trans(T transX, T transY)
    {
        return Transform2<T>(Vector2<T>(T(1.0), T(0.0)),
            Vector2<T>(T(0.0), T(1.0)),
            Vector2<T>(transX, transY));
    }
    static Transform2 Rot(T radians)
    {
        T cs = cosf(radians);
        T sn = sinf(radians);
        
        return Transform2<T>(Vector2<T>(cs, sn), Vector2<T>(-sn, cs), Vector2<T>(T(0.0), T(0.0)));
    }
    static Transform2 ShearXY(T shear)
    {
        return Transform2<T>(Vector2<T>(T(1.0), T(0.0)), Vector2<T>(shear, T(1.0)),
            Vector2<T>(T(0.0), T(0.0)));
    }

    static Transform2 ShearYX(T shear)
    {
        return Transform2<T>(Vector2<T>(T(1.0), shear), Vector2<T>(T(0.0), T(1.0)),
            Vector2<T>(T(0.0), T(0.0)));
    }
    static Transform2 Skew(T radiansX, T radiansY)
    {
        return Transform2<T>(Vector2<T>(T(1.0), radiansY), Vector2<T>(radiansX, T(1.0)),
            Vector2<T>(T(0.0), 0.f));
    }

    /// Creates a Transform from a string with a list of values
    static bool TryParse(const char* str, Transform2& output);

    /// Returns a string that represents the current transform
    NsString ToString() const;

    /// Returns a hash code for the current transform
    uint32_t GetHashCode() const;

private:
    Vector2<T> mVal[3];

    NS_DECLARE_REFLECTION(Transform2, NoParent)
};

/// Unary operators
//@{
/// Compute the inverse of the matrix
/// \note Returns an unpredictable value is m has no inverse
template<class T> Transform2<T> Inverse(const Transform2<T>& m);
template<class T> Transform2<T> Inverse(const Transform2<T>& m, T determinant);

/// Left multiply a matrix by a scale matrix
template<class T> Transform2<T> PreScale(T scaleX, T scaleY, const Transform2<T>& m);

/// Left multiply a matrix by a translation matrix
template<class T> Transform2<T> PreTrans(T transX, T transY, const Transform2<T>& m);

/// Left multiply a matrix by a xrotation matrix
template<class T> Transform2<T> PreRot(T radians, const Transform2<T>& m);

/// Left multiply a matrix by a skew matrix
template<class T> Transform2<T> PreSkew(T radiansX, T radiansY, const Transform2<T>& m);

/// Right multiply a matrix by a scale matrix
template<class T> Transform2<T> PostScale(const Transform2<T>& m, T scaleX, T scaleY);

/// Right multiply a matrix by a translation matrix
template<class T> Transform2<T> PostTrans(const Transform2<T>& m, T transX, T transY);

/// Right multiply a matrix by a xrotation matrix
template<class T> Transform2<T> PostRot(const Transform2<T>& m, T radians);

/// Right multiply a matrix by a skew matrix
template<class T> Transform2<T> PostSkew(const Transform2<T>& m, T radiansX, T radiansY);
//@}

/// Binary operators
//@{
template<class T> const Transform2<T> operator*(const Transform2<T>& m, T v);
template<class T> const Transform2<T> operator*(T v, const Transform2<T>& m);
template<class T> const Transform2<T> operator/(const Transform2<T>& m, T v);

/// Mutiply vector by transform matrix. The vector is considered a point (expanded to (x, y, 1)
/// and the translation is applied
template<class T> const Vector2<T> PointTransform(const Vector2<T>& v, const Transform2<T>& m);

/// Mutiply vector by transform matrix. The vector is expanded to (x, y, 0)
/// and the translation stored in the transform is ignored
template<class T> const Vector2<T> VectorTransform(const Vector2<T>& v, const Transform2<T>& m);

/// This operator is implemented using PointTransform
template<class T> const Vector2<T> operator*(const Vector2<T>& v, const Transform2<T>& m);

template<class T> const Vector3<T> operator*(const Vector3<T>& v, const Transform2<T>& m);
template<class T> const Transform2<T> operator*(const Transform2<T>& m0, const Transform2<T>& m1);
template<class T> const Matrix3<T> operator*(const Transform2<T>& m0, const Matrix3<T>& m1);
template<class T> const Matrix3<T> operator*(const Matrix3<T>& m0, const Transform2<T>& m1);

/// Returns the x-scale applied to the given transform
inline float ScaleX(const Transform2f& m);

/// Returns the y-scale applied to the given transform
inline float ScaleY(const Transform2f& m);

/// Returns the max scale applied to the given transform
inline float MaxScale(const Transform2f& m);

/// Returns the horizontal displacement applied to the given transform
inline float TransX(const Transform2f& m);

/// Returns the vertical displacement applied to the given transform
inline float TransY(const Transform2f& m);

/// Returns whether there are rotations applied to the given transform
inline bool IsRotating(const Transform2f& m);
//@}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Affine 3D transformation
/// Internally, the transform is implemented in terms of a Matrix3 and a Vector3
/// The interface of Transform3 is similar to Matrix4, but instead of storing 16 elements, it 
/// stores only 12 elements (last column is supposed to be (0,0,0,1))
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class Transform3
{
public:
    /// Default Constructor. Does no initialization
    inline Transform3() {}

    // Constructor from explicit floats
    inline Transform3(T v00, T v01, T v02, T v10, T v11, T v12, T v20, T v21, T v22,
        T v30, T v31, T v32)
    {
        mVal[0] = Vector3<T>(v00, v01, v02);
        mVal[1] = Vector3<T>(v10, v11, v12);
        mVal[2] = Vector3<T>(v20, v21, v22);
        mVal[3] = Vector3<T>(v30, v31, v32);
    }

    // Constructor from explicit vectors
    inline Transform3(const Vector3<T>& v0, const Vector3<T>& v1, const Vector3<T>& v2, 
        const Vector3<T>& v3)
    {
        mVal[0] = v0;
        mVal[1] = v1;
        mVal[2] = v2;
        mVal[3] = v3;
    }

    /// Constructor from pointer
    inline Transform3(const T* values)
    {
        NS_ASSERT(values != 0);
        
        mVal[0] = Vector3<T>(values[0], values[1], values[2]);
        mVal[1] = Vector3<T>(values[3], values[4], values[5]);
        mVal[2] = Vector3<T>(values[6], values[7], values[8]);
        mVal[3] = Vector3<T>(values[9], values[10], values[11]);
    }

    /// Copy constructor
    inline Transform3(const Transform3& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        mVal[2] = m[2];
        mVal[3] = m[3];
    }

    /// Assignment
    inline Transform3& operator=(const Transform3& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        mVal[2] = m[2];
        mVal[3] = m[3];
        
        return *this;
    }

    /// Accessors
    //@{
    inline Vector3<T>& operator[](uint32_t i)
    {
        NS_ASSERT(i < 4);
        return mVal[i];
    }
    inline const Vector3<T>& operator[](uint32_t i) const
    {
        NS_ASSERT(i < 4);
        return mVal[i];
    }
    inline const T* GetData() const
    {
        return mVal[0].GetData();
    }
    inline void SetUpper3x3(const Matrix3<T>& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        mVal[2] = m[2];
    }
    inline void SetTranslation(const Vector3<T>& v)
    {
        mVal[3] = v;
    }
    inline const Matrix3<T>& GetUpper3x3() const
    {
        return *reinterpret_cast<const Matrix3<T>*>(mVal[0].GetData());
    }
    inline const Vector3<T>& GetTranslation() const
    {
        return mVal[3];
    }
    //@}

    /// In-Place operators
    //@{
    inline Transform3& operator*=(const Transform3& m)
    {
        mVal[0] = Vector3<T>(mVal[0][0] * m[0][0] + mVal[0][1] * m[1][0] + mVal[0][2] * m[2][0],
            mVal[0][0] * m[0][1] + mVal[0][1] * m[1][1] + mVal[0][2] * m[2][1],
            mVal[0][0] * m[0][2] + mVal[0][1] * m[1][2] + mVal[0][2] * m[2][2]);

        mVal[1] = Vector3<T>(mVal[1][0] * m[0][0] + mVal[1][1] * m[1][0] + mVal[1][2] * m[2][0],
            mVal[1][0] * m[0][1] + mVal[1][1] * m[1][1] + mVal[1][2] * m[2][1],
            mVal[1][0] * m[0][2] + mVal[1][1] * m[1][2] + mVal[1][2] * m[2][2]);

        mVal[2] = Vector3<T>(mVal[2][0] * m[0][0] + mVal[2][1] * m[1][0] + mVal[2][2] * m[2][0],
            mVal[2][0] * m[0][1] + mVal[2][1] * m[1][1] + mVal[2][2] * m[2][1],
            mVal[2][0] * m[0][2] + mVal[2][1] * m[1][2] + mVal[2][2] * m[2][2]);

        mVal[3] = Vector3<T>(mVal[3][0] * m[0][0] + mVal[3][1] * m[1][0] + mVal[3][2] * m[2][0] + 
            m[3][0],
            mVal[3][0] * m[0][1] + mVal[3][1] * m[1][1] + mVal[3][2] * m[2][1] + m[3][1],
            mVal[3][0] * m[0][2] + mVal[3][1] * m[1][2] + mVal[3][2] * m[2][2] + m[3][2]
        );

        return *this;
    }
    inline Transform3& operator*=(T v)
    {
        mVal[0] *= v;
        mVal[1] *= v;
        mVal[2] *= v;
        mVal[3] *= v;
        
        return *this;
    }
    inline Transform3& operator/=(T v)
    {
        mVal[0] /= v;
        mVal[1] /= v;
        mVal[2] /= v;
        mVal[3] /= v;
        
        return *this;
    }
    //@}

    /// Logic operators
    //@{
    inline bool operator==(const Transform3& m) const
    {
        return mVal[0] == m[0] && mVal[1] == m[1] && mVal[2] == m[2] && mVal[3] == m[3];
    }
    inline bool operator!=(const Transform3& m) const
    {
        return !(operator==(m));
    }
    //@}

    /// Static functions
    //@{
    static Transform3 Identity()
    {
        return Transform3<T>(Vector3<T>(T(1.0), T(0.0), T(0.0)),
            Vector3<T>(T(0.0), T(1.0), T(0.0)),
            Vector3<T>(T(0.0), T(0.0), T(1.0)),
            Vector3<T>(T(0.0), T(0.0), T(0.0)));
    }
    static Transform3 Scale(T scaleX, T scaleY, T scaleZ)
    {
        return Transform3<T>(Vector3<T>(scaleX, T(0.0), T(0.0)),
            Vector3<T>(T(0.0), scaleY, T(0.0)),
            Vector3<T>(T(0.0), T(0.0), scaleZ),
            Vector3<T>(T(0.0), T(0.0), T(0.0)));
    }
    static Transform3 Trans(T transX, T transY, T transZ)
    {
        return Transform3<T>(Vector3<T>(T(1.0), T(0.0), T(0.0)),
            Vector3<T>(T(0.0), T(1.0), T(0.0)),
            Vector3<T>(T(0.0), T(0.0), T(1.0)),
            Vector3<T>(transX, transY, transZ));
    }
    static Transform3 RotX(T radians)
    {
        T cs = cosf(radians);
        T sn = sinf(radians);
        
        return Transform3<T>(Vector3<T>(T(1.0), T(0.0), T(0.0)),
            Vector3<T>(T(0.0), cs, sn),
            Vector3<T>(T(0.0), -sn, cs),
            Vector3<T>(T(0.0), T(0.0), T(0.0)));
    }
    static Transform3 RotY(T radians)
    {
        T cs = cosf(radians);
        T sn = sinf(radians);
        
        return Transform3<T>(Vector3<T>(cs, T(0.0), sn), 
            Vector3<T>(T(0.0), T(1.0), T(0.0)),
            Vector3<T>(-sn, T(0.0), cs),
            Vector3<T>(T(0.0), T(0.0), T(0.0)));
    }
    static Transform3 RotZ(T radians)
    {
        T cs = cosf(radians);
        T sn = sinf(radians);
        
        return Transform3<T>(Vector3<T>(cs, sn, T(0.0)), 
            Vector3<T>(-sn, cs, T(0.0)),
            Vector3<T>(T(0.0), T(0.0), T(1.0)),
            Vector3<T>(T(0.0), T(0.0), T(0.0)));
    }
    //@}

    /// Returns a string that represents the current transform
    NsString ToString() const;

    /// Returns a hash code for the current transform
    uint32_t GetHashCode() const;

private:
    Vector3<T> mVal[4];

    NS_DECLARE_REFLECTION(Transform3, NoParent)
};

/// Unary operators
//@{
/// Compute the inverse of the matrix
/// \note Returns an unpredictable value is m has no inverse
template<class T> Transform3<T> Inverse(const Transform3<T>& m);

/// Left multiply a matrix by a scale matrix
template<class T> Transform3<T> PreScale(T scaleX, T scaleY, T scaleZ, const Transform3<T>& m);

/// Left multiply a matrix by a translation matrix
template<class T> Transform3<T> PreTrans(T transX, T transY, T transZ, const Transform3<T>& m);

/// Left multiply a matrix by a xrotation matrix
template<class T>  Transform3<T> PreRotX(T radians, const Transform3<T>& m);

/// Left multiply a matrix by a yrotation matrix
template<class T> Transform3<T> PreRotY(T radians, const Transform3<T>& m);

/// Left multiply a matrix by a zrotation matrix
template<class T> Transform3<T> PreRotZ(T radians, const Transform3<T>& m);


/// Right multiply a matrix by a scale matrix
template<class T> Transform3<T> PostScale(const Transform3<T>& m, T scaleX, T scaleY, T scaleZ);

/// Right multiply a matrix by a translation matrix
template<class T> Transform3<T> PostTrans(const Transform3<T>& m, T transX, T transY, T transZ);

/// Right multiply a matrix by a xrotation matrix
template<class T> Transform3<T> PostRotX(const Transform3<T>& m, T radians);

/// Right multiply a matrix by a yrotation matrix
template<class T> Transform3<T> PostRotY(const Transform3<T>& m, T radians);

/// Right multiply a matrix by a zrotation matrix
template<class T> Transform3<T> PostRotZ(const Transform3<T>& m, T radians);
//@}

/// Binary operators
//@{
template<class T> const Transform3<T> operator*(const Transform3<T>& m, T v);
template<class T> const Transform3<T> operator*(T v, const Transform3<T>& m);
template<class T> const Transform3<T> operator/(const Transform3<T>& m, T v);

/// Mutiply vector by transform matrix. The vector is considered a point (expanded to (x, y, z, 1)
/// and the translation is applied
template<class T> const Vector3<T> PointTransform(const Vector3<T>& v, const Transform3<T>& m);

/// Mutiply vector by transform matrix. The vector is expanded to (x, y, z, 0)
/// and the translation stored in the transform is ignored
template<class T> const Vector3<T> VectorTransform(const Vector3<T>& v, const Transform3<T>& m);

/// This operator is implemented using PointTransform
template<class T> const Vector3<T> operator*(const Vector3<T>& v, const Transform3<T>& m);

template<class T> const Vector4<T> operator*(const Vector4<T>& v, const Transform3<T>& m);
template<class T> const Transform3<T> operator*(const Transform3<T>& m0, const Transform3<T>& m1);
template<class T> const Matrix4<T> operator*(const Transform3<T>& m0, const Matrix4<T>& m1);
template<class T> const Matrix4<T> operator*(const Matrix4<T>& m0, const Transform3<T>& m1);
//@}


typedef Transform2<float> Transform2f;
typedef Transform3<float> Transform3f;

#define NS_TEMPLATE extern template NS_MATH_VECTORMATH_API

NS_TEMPLATE bool Transform2f::TryParse(const char*, Transform2&);
NS_TEMPLATE NsString Transform2f::ToString() const;
NS_TEMPLATE uint32_t Transform2f::GetHashCode() const;
NS_TEMPLATE const TypeClass* Transform2f::StaticGetClassType(TypeTag<Transform2f>*);

NS_TEMPLATE NsString Transform3f::ToString() const;
NS_TEMPLATE uint32_t Transform3f::GetHashCode() const;
NS_TEMPLATE const TypeClass* Transform3f::StaticGetClassType(TypeTag<Transform3f>*);

#undef NS_TEMPLATE

}

// Inline include
#include <NsMath/Transform.inl>

#endif
