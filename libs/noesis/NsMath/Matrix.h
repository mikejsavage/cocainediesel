////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __MATH_MATRIX_H__
#define __MATH_MATRIX_H__


#include <NsCore/Noesis.h>
#include <NsMath/Vector.h>
#include <NsMath/VectorMathApi.h>
#include <NsCore/Error.h>
#include <NsCore/ReflectionDeclare.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// 2x2 Matrix
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class Matrix2
{
public:
    /// Default Constructor. Does no initialization
    inline Matrix2() {}

    // Constructor from explicit floats
    inline Matrix2(T v00, T v01, T v10, T v11)
    {
        mVal[0] = Vector2<T>(v00, v01);
        mVal[1] = Vector2<T>(v10, v11);
    }

    // Constructor from explicit vectors
    inline Matrix2(const Vector2<T>& v0, const Vector2<T>& v1)
    {
        mVal[0] = v0;
        mVal[1] = v1;
    }
    
    /// Constructor from pointer
    inline Matrix2(const T* values)
    {
        NS_ASSERT(values != 0);
        
        mVal[0] = Vector2<T>(values[0], values[1]);
        mVal[1] = Vector2<T>(values[2], values[3]);
    }

    /// Copy constructor
    inline Matrix2(const Matrix2& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
    }
    
    /// Assignment
    inline Matrix2& operator=(const Matrix2& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        
        return *this;
    }

    /// Accessors
    //@{
    inline Vector2<T>& operator[](uint32_t i)
    {
        NS_ASSERT(i < 2);
        return mVal[i];
    }
    inline const Vector2<T>& operator[](uint32_t i) const
    {
        NS_ASSERT(i < 2);
        return mVal[i];
    }
    inline const T* GetData() const
    {
        return mVal[0].GetData();
    }
    //@}

    /// In-Place operators
    //@{
    inline Matrix2& operator*=(const Matrix2& m)
    {
        mVal[0] = Vector2<T>(mVal[0][0] * m[0][0] + mVal[0][1] * m[1][0], mVal[0][0] * m[0][1] + 
            mVal[0][1] * m[1][1]);

        mVal[1] = Vector2<T>(mVal[1][0] * m[0][0] + mVal[1][1] * m[1][0], mVal[1][0] * m[0][1] + 
            mVal[1][1] * m[1][1]);

        return *this;
    }
    inline Matrix2& operator*=(T v)
    {
        mVal[0] *= v;
        mVal[1] *= v;
        
        return *this;
    }
    inline Matrix2& operator/=(T v)
    {
        mVal[0] /= v;
        mVal[1] /= v;
        
        return *this;
    }
    //@}

    /// Logic operators
    //@{
    inline bool operator==(const Matrix2& m) const
    {
        return mVal[0] == m[0] && mVal[1] == m[1];
    }
    inline bool operator!=(const Matrix2& m) const
    {
        return !(operator==(m));
    }
    //@}

    /// Static functions
    //@{
    static Matrix2 Identity()
    {
        return Matrix2<T>(Vector2<T>(T(1.0), T(0.0)), Vector2<T>(T(0.0), T(1.0)));
    }
    static Matrix2 Scale(T scaleX, T scaleY)
    {
        return Matrix2<T>(Vector2<T>(scaleX, T(0.0)), Vector2<T>(T(0.0), scaleY));
    }
    static Matrix2 Rot(T radians)
    {
        T cs = cosf(radians);
        T sn = sinf(radians);
        
        return Matrix2<T>(Vector2<T>(cs, sn), Vector2<T>(-sn, cs));
    }
    static Matrix2 ShearXY(T shear)
    {
        return Matrix2<T>(Vector2<T>(T(1.0), T(0.0)), Vector2<T>(shear, T(1.0)));
    }
    static Matrix2 ShearYX(T shear)
    {
        return Matrix2<T>(Vector2<T>(T(1.0), shear), Vector2<T>(T(0.0), T(1.0)));
    }
    //@}

    /// Returns a string that represents the current matrix
    NsString ToString() const;

    /// Returns a hash code for the current matrix
    uint32_t GetHashCode() const;

private:
    Vector2<T> mVal[2];

    NS_DECLARE_REFLECTION(Matrix2, NoParent)
};

/// Unary operators
//@{
/// Compute the transpose of the matrix
template<class T> Matrix2<T> Transpose(const Matrix2<T>& m);

/// Compute the inverse of the matrix
/// \note Returns an unpredictable value is m has no inverse
template<class T> Matrix2<T> Inverse(const Matrix2<T>& m);

/// Compute the inverse of the matrix. The determinant is passed to this function instead of
/// being calculated. This function is useful if you want to check if a matrix is invertible 
/// checking if the determinant is nonzero. The calculated determinant can be passed to this
/// function avoiding redundant calculations.
/// \note Returns an unpredictable value if determinant is zero
template<class T> Matrix2<T> Inverse(const Matrix2<T>& m, T determinant);

/// Compute the determinant of the matrix
template<class T> T Determinant(const Matrix2<T>& m);

/// Left multiply a matrix by a scale matrix
template<class T> Matrix2<T> PreScale(T scaleX, T scaleY, const Matrix2<T>& m);

/// Left multiply a matrix by a zrotation matrix
template<class T> Matrix2<T> PreRot(T radians, const Matrix2<T>& m);

/// Right multiply a matrix by a scale matrix
template<class T> Matrix2<T> PostScale(const Matrix2<T>& m, T scaleX, T scaleY);

/// Right multiply a matrix by a zrotation matrix
template<class T> Matrix2<T> PostRot(const Matrix2<T>& m, T radians);
//@}

/// Binary operators
//@{
template<class T> const Matrix2<T> operator*(const Matrix2<T>& m, T v);
template<class T> const Matrix2<T> operator*(T v, const Matrix2<T>& m);
template<class T> const Matrix2<T> operator/(const Matrix2<T>& m, T v);
template<class T> const Vector2<T> operator*(const Vector2<T>& v, const Matrix2<T>& m);
template<class T> const Matrix2<T> operator*(const Matrix2<T>& m0, const Matrix2<T>& m1);
//@}


template<class T> class Transform2;
typedef Transform2<float> Transform2f;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// 3x3 Matrix
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class Matrix3
{
public:
    /// Default Constructor. Does no initialization
    inline Matrix3() {}

    // Constructor from explicit floats
    inline Matrix3(T v00, T v01, T v02, T v10, T v11, T v12, T v20, T v21, T v22)
    {
        mVal[0] = Vector3<T>(v00, v01, v02);
        mVal[1] = Vector3<T>(v10, v11, v12);
        mVal[2] = Vector3<T>(v20, v21, v22);
    }

    // Constructor from explicit vectors
    inline Matrix3(const Vector3<T>& v0, const Vector3<T>& v1, const Vector3<T>& v2)
    {
        mVal[0] = v0;
        mVal[1] = v1;
        mVal[2] = v2;
    }
    
    /// Constructor from pointer
    inline Matrix3(const T* values)
    {
        NS_ASSERT(values != 0);
        
        mVal[0] = Vector3<T>(values[0], values[1], values[2]);
        mVal[1] = Vector3<T>(values[3], values[4], values[5]);
        mVal[2] = Vector3<T>(values[6], values[7], values[8]);
    }
    
    /// Constructor from Transform
    Matrix3(const Transform2<T>& transform);

    /// Copy constructor
    inline Matrix3(const Matrix3& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        mVal[2] = m[2];
    }

    /// Assignment
    inline Matrix3& operator=(const Matrix3& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        mVal[2] = m[2];
        
        return *this;
    }

    /// Accessors
    //@{
    inline Vector3<T>& operator[](uint32_t i)
    {
        NS_ASSERT(i < 3);
        return mVal[i];
    }
    inline const Vector3<T>& operator[](uint32_t i) const
    {
        NS_ASSERT(i < 3);
        return mVal[i];
    }
    inline const T* GetData() const
    {
        return mVal[0].GetData();
    }
    //@}

    /// In-Place operators
    //@{
    inline Matrix3& operator*=(const Matrix3& m)
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

        return *this;
    }
    inline Matrix3& operator*=(T v)
    {
        mVal[0] *= v;
        mVal[1] *= v;
        mVal[2] *= v;
        
        return *this;
    }
    inline Matrix3& operator/=(T v)
    {
        mVal[0] /= v;
        mVal[1] /= v;
        mVal[2] /= v;
        
        return *this;
    }
    //@}

    /// Logic operators
    //@{
    inline bool operator==(const Matrix3& m) const
    {
        return mVal[0] == m[0] && mVal[1] == m[1] && mVal[2] == m[2];
    }
    inline bool operator!=(const Matrix3& m) const
    {
        return !(operator==(m));
    }
    //@}

    /// Static functions
    //@{
    static Matrix3 Identity()
    {
        return Matrix3<T>(Vector3<T>(T(1.0), T(0.0), T(0.0)), Vector3<T>(T(0.0), T(1.0), T(0.0)),
            Vector3<T>(T(0.0), T(0.0), T(1.0)));
    }
    static Matrix3 Scale(T scaleX, T scaleY, T scaleZ)
    {
        return Matrix3<T>(Vector3<T>(scaleX, T(0.0), T(0.0)), Vector3<T>(T(0.0), scaleY, T(0.0)),
            Vector3<T>(T(0.0), T(0.0), scaleZ));
    }
    static Matrix3 RotX(T radians)
    {
        T cs = cosf(radians);
        T sn = sinf(radians);
        
        return Matrix3<T>(Vector3<T>(T(1.0), T(0.0), T(0.0)), Vector3<T>(T(0.0), cs, sn),
            Vector3<T>(T(0.0), -sn, cs));
    }
    static Matrix3 RotY(T radians)
    {
        T cs = cosf(radians);
        T sn = sinf(radians);
        
        return Matrix3<T>(Vector3<T>(cs, T(0.0), sn), Vector3<T>(T(0.0), T(1.0), T(0.0)),
            Vector3<T>(-sn, T(0.0), cs));
    }
    static Matrix3 RotZ(T radians)
    {
        T cs = cosf(radians);
        T sn = sinf(radians);
        
        return Matrix3<T>(Vector3<T>(cs, sn, T(0.0)), Vector3<T>(-sn, cs, T(0.0)),
            Vector3<T>(T(0.0), T(0.0), T(1.0)));
    }

    /// Returns a string that represents the current matrix
    NsString ToString() const;

    /// Returns a hash code for the current matrix
    uint32_t GetHashCode() const;

private:
    Vector3<T> mVal[3];

    NS_DECLARE_REFLECTION(Matrix3, NoParent)
};

/// Unary operators
//@{
/// Compute the transpose of the matrix
template<class T> Matrix3<T> Transpose(const Matrix3<T>& m);

/// Affinity checking (last column is 0 0 1)
template<class T> bool IsAffine(const Matrix3<T>& m);

/// Compute the inverse of the matrix
/// \note Returns an unpredictable value is m has no inverse
template<class T> Matrix3<T> Inverse(const Matrix3<T>& m);

/// Compute the inverse of the matrix. The determinant is passed to this function instead of
/// being calculated. This function is useful is you want to check if a matrix is invertible 
/// checking if the determinant is nonzero. The calculated determinant can be passed to this function
/// avoiding redundant calculations.
/// \note Returns an unpredictable value is determinant is zero
template<class T> Matrix3<T> Inverse(const Matrix3<T>& m, T determinant);

/// Compute the determinant of the matrix
template<class T> T Determinant(const Matrix3<T>& m);

/// Left multiply a matrix by a scale matrix
template<class T> Matrix3<T> PreScale(T scaleX, T scaleY, T scaleZ, const Matrix3<T>& m);

/// Left multiply a matrix by a xrotation matrix
template<class T> Matrix3<T> PreRotX(T radians, const Matrix3<T>& m);

/// Left multiply a matrix by a yrotation matrix
template<class T> Matrix3<T> PreRotY(T radians, const Matrix3<T>& m);

/// Left multiply a matrix by a zrotation matrix
template<class T> Matrix3<T> PreRotZ(T radians, const Matrix3<T>& m);


/// Right multiply a matrix by a scale matrix
template<class T> Matrix3<T> PostScale(const Matrix3<T>& m, T scaleX, T scaleY, T scaleZ);

/// Right multiply a matrix by a xrotation matrix
template<class T> Matrix3<T> PostRotX(const Matrix3<T>& m, T radians);

/// Right multiply a matrix by a yrotation matrix
template<class T> Matrix3<T> PostRotY(const Matrix3<T>& m, T radians);

/// Right multiply a matrix by a zrotation matrix
template<class T> Matrix3<T> PostRotZ(const Matrix3<T>& m, T radians);
//@}

/// Binary operators
//@{
template<class T> const Matrix3<T> operator*(const Matrix3<T>& m, T v);
template<class T> const Matrix3<T> operator*(T v, const Matrix3<T>& m);
template<class T> const Matrix3<T> operator/(const Matrix3<T>& m, T v);
template<class T> const Vector3<T> operator*(const Vector3<T>& v, const Matrix3<T>& m);
template<class T> const Matrix3<T> operator*(const Matrix3<T>& m0, const Matrix3<T>& m1);
//@}

template<class T> class Transform3;
typedef Transform3<float> Transform3f;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// 4x4 Matrix
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class Matrix4
{
public:
    /// Default Constructor. Does no initialization
    inline Matrix4() {}

    // Constructor from explicit floats
    inline Matrix4(T v00, T v01, T v02, T v03, T v10, T v11, T v12, T v13, T v20, T v21, T v22, 
        T v23, T v30, T v31, T v32, T v33)
    {
        mVal[0] = Vector4<T>(v00, v01, v02, v03);
        mVal[1] = Vector4<T>(v10, v11, v12, v13);
        mVal[2] = Vector4<T>(v20, v21, v22, v23);
        mVal[3] = Vector4<T>(v30, v31, v32, v33);
    }

    // Constructor from explicit vectors
    inline Matrix4(const Vector4<T>& v0, const Vector4<T>& v1, const Vector4<T>& v2, 
        const Vector4<T>& v3)
    {
        mVal[0] = v0;
        mVal[1] = v1;
        mVal[2] = v2;
        mVal[3] = v3;
    }

    /// Constructor from pointer
    inline Matrix4(const T* values)
    {
        NS_ASSERT(values != 0);
        
        mVal[0] = Vector4<T>(values[0], values[1], values[2], values[3]);
        mVal[1] = Vector4<T>(values[4], values[5], values[6], values[7]);
        mVal[2] = Vector4<T>(values[8], values[9], values[10], values[11]);
        mVal[3] = Vector4<T>(values[12], values[13], values[14], values[15]);
    }

    /// Constructor from Transform
    Matrix4(const Transform3<T>& transform);
    
    /// Copy constructor
    inline Matrix4(const Matrix4& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        mVal[2] = m[2];
        mVal[3] = m[3];
    }

    /// Assignment
    inline Matrix4& operator=(const Matrix4& m)
    {
        mVal[0] = m[0];
        mVal[1] = m[1];
        mVal[2] = m[2];
        mVal[3] = m[3];
        
        return *this;
    }

    /// Accessors
    //@{
    inline Vector4<T>& operator[](uint32_t i)
    {
        NS_ASSERT(i < 4);
        return mVal[i];
    }
    inline const Vector4<T>& operator[](uint32_t i) const
    {
        NS_ASSERT(i < 4);
        return mVal[i];
    }
    inline const T* GetData() const
    {
        return mVal[0].GetData();
    }
    //@}

    /// In-Place operators
    //@{
    Matrix4& operator*=(const Matrix4& m);

    inline Matrix4& operator*=(T v)
    {
        mVal[0] *= v;
        mVal[1] *= v;
        mVal[2] *= v;
        mVal[3] *= v;
        
        return *this;
    }
    inline Matrix4& operator/=(T v)
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
    inline bool operator==(const Matrix4& m) const
    {
        return mVal[0] == m[0] && mVal[1] == m[1] && mVal[2] == m[2] && mVal[3] == m[3];
    }
    inline bool operator!=(const Matrix4& m) const
    {
        return !(operator==(m));
    }
    //@}
    
    /// Static functions
    //@{
    static Matrix4 Identity()
    {
        return Matrix4<T>(Vector4<T>(T(1.0), T(0.0), T(0.0), T(0.0)),
            Vector4<T>(T(0.0), T(1.0), T(0.0), T(0.0)),
            Vector4<T>(T(0.0), T(0.0), T(1.0), T(0.0)),
            Vector4<T>(T(0.0), T(0.0), T(0.0), T(1.0)));
    }
    static Matrix4 Scale(T scaleX, T scaleY, T scaleZ)
    {
        return Matrix4<T>(Vector4<T>(scaleX, T(0.0), T(0.0), T(0.0)),
            Vector4<T>(T(0.0), scaleY, T(0.0), T(0.0)),
            Vector4<T>(T(0.0), T(0.0), scaleZ, T(0.0)),
            Vector4<T>(T(0.0), T(0.0), T(0.0), T(1.0)));
    }
    static Matrix4 RotX(T radians)
    {
        T cs = Cos(radians);
        T sn = Sin(radians);
        
        return Matrix4<T>(Vector4<T>(T(1.0), T(0.0), T(0.0), T(0.0)),
            Vector4<T>(T(0.0), cs, sn, T(0.0)),
            Vector4<T>(T(0.0), -sn, cs, T(0.0)),
            Vector4<T>(T(0.0), T(0.0), T(0.0), T(1.0)));
    }
    static Matrix4 RotY(T radians)
    {
        T cs = Cos(radians);
        T sn = Sin(radians);
        
        return Matrix4<T>(Vector4<T>(cs, T(0.0), sn, T(0.0)), 
            Vector4<T>(T(0.0), T(1.0), T(0.0), T(0.0)),
            Vector4<T>(-sn, T(0.0), cs, T(0.0)),
            Vector4<T>(T(0.0), T(0.0), T(0.0), T(1.0)));
    }
    static Matrix4 RotZ(T radians)
    {
        T cs = Cos(radians);
        T sn = Sin(radians);
        
        return Matrix4<T>(Vector4<T>(cs, sn, T(0.0), T(0.0)), 
            Vector4<T>(-sn, cs, T(0.0), T(0.0)),
            Vector4<T>(T(0.0), T(0.0), T(1.0), T(0.0)),
            Vector4<T>(T(0.0), T(0.0), T(0.0), T(1.0)));
    }

    /// Returns a symmetric perspective projection matrix with far plane at infinite
    /// \param fovY Vertical Field of View, in radians
    /// \param aspect Aspect ratio, width divided by height
    /// \param zNear Z near plane
    static Matrix4 PerspectiveFov(T fovY, T aspect, T zNear);

    /// Returns a viewport matrix that transform from normalized homogeneus space to screen space
    static Matrix4 Viewport(T width, T height);
    //@}

    /// Returns a string that represents the current matrix
    NsString ToString() const;

    /// Returns a hash code for the current matrix
    uint32_t GetHashCode() const;

private:
    Vector4<T> mVal[4];

    NS_DECLARE_REFLECTION(Matrix4, NoParent)
};

/// Unary operators
//@{
/// Compute the transpose of the matrix
template<class T> Matrix4<T> Transpose(const Matrix4<T>& m);

/// Affinity checking (last column is 0 0 0 1)
template<class T> bool IsAffine(const Matrix4<T>& m);

/// Compute the inverse of the matrix
/// \note Returns an unpredictable value is m has no inverse
template<class T> Matrix4<T> Inverse(const Matrix4<T>& m);

/// Compute the inverse of the matrix. The determinant is passed to this function instead of
/// being calculated. This function is useful is you want to check if a matrix is invertible 
/// checking if the determinant is nonzero. The calculated determinant can be passed to this function
/// avoiding redundant calculations.
/// \note Returns an unpredictable value is determinant is zero
template<class T> Matrix4<T> Inverse(const Matrix4<T>& m, T determinant);

/// Compute the determinant of the matrix
template<class T> T Determinant(const Matrix4<T>& m);
//@}

/// Binary operators
//@{
template<class T> const Matrix4<T> operator*(const Matrix4<T>& m, T v);
template<class T> const Matrix4<T> operator*(T v, const Matrix4<T>& m);
template<class T> const Matrix4<T> operator/(const Matrix4<T>& m, T v);
template<class T> const Vector4<T> operator*(const Vector4<T>& v, const Matrix4<T>& m);
template<class T> const Matrix4<T> operator*(const Matrix4<T>& m0, const Matrix4<T>& m1);
//@}

typedef Matrix2<float> Matrix2f;
typedef Matrix3<float> Matrix3f;
typedef Matrix4<float> Matrix4f;

#define NS_TEMPLATE extern template NS_MATH_VECTORMATH_API

NS_TEMPLATE NsString Matrix2f::ToString() const;
NS_TEMPLATE uint32_t Matrix2f::GetHashCode() const;
NS_TEMPLATE const TypeClass* Matrix2f::StaticGetClassType(TypeTag<Matrix2f>*);

NS_TEMPLATE NsString Matrix3f::ToString() const;
NS_TEMPLATE uint32_t Matrix3f::GetHashCode() const;
NS_TEMPLATE Matrix3f::Matrix3(const Transform2f&);
NS_TEMPLATE const TypeClass* Matrix3f::StaticGetClassType(TypeTag<Matrix3f>*);

NS_TEMPLATE NsString Matrix4f::ToString() const;
NS_TEMPLATE uint32_t Matrix4f::GetHashCode() const;
NS_TEMPLATE Matrix4f::Matrix4(const Transform3f&);
NS_TEMPLATE Matrix4f& Matrix4f::operator*=(const Matrix4f& m);
NS_TEMPLATE Matrix4f Matrix4f::PerspectiveFov(float, float, float);
NS_TEMPLATE Matrix4f Matrix4f::Viewport(float, float);
NS_TEMPLATE Matrix4f Inverse(const Matrix4f&);
NS_TEMPLATE Matrix4f Inverse(const Matrix4f&, float);
NS_TEMPLATE float Determinant(const Matrix4f&);
NS_TEMPLATE const TypeClass* Matrix4f::StaticGetClassType(TypeTag<Matrix4f>*);

#undef NS_TEMPLATE

}

// Inline include
#include <NsMath/Matrix.inl>

#endif
