////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <limits>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix2<T> Transpose(const Matrix2<T>& m)
{
    return Matrix2<T>(m[0][0], m[1][0], m[0][1], m[1][1]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix2<T> Inverse(const Matrix2<T>& m)
{
    T determinant = Determinant(m);
    return Inverse(m, determinant);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix2<T> Inverse(const Matrix2<T>& m, T determinant)
{
    NS_ASSERT(fabsf(determinant) > std::numeric_limits<T>::epsilon());
    return Matrix2<T>(m[1][1], -m[0][1], -m[1][0], m[0][0]) / determinant;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
T Determinant(const Matrix2<T>& m)
{    
    return (m[0][0] * m[1][1] - m[0][1] * m[1][0]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix2<T> PreScale(T scaleX, T scaleY, const Matrix2<T>& m)
{
    Matrix2<T> res;
    
    res[0][0] = m[0][0] * scaleX;
    res[0][1] = m[0][1] * scaleX;
    
    res[1][0] = m[1][0] * scaleY;
    res[1][1] = m[1][1] * scaleY;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix2<T> PreRot(T radians, const Matrix2<T>& m)
{
    Matrix2<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs + m[1][0] * sn;
    res[0][1] = m[0][1] * cs + m[1][1] * sn;
    
    res[1][0] = m[1][0] * cs - m[0][0] * sn;
    res[1][1] = m[1][1] * cs - m[0][1] * sn;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix2<T> PostScale(const Matrix2<T>& m, T scaleX, T scaleY)
{
    Matrix2<T> res;
    
    res[0][0] = m[0][0] * scaleX;
    res[0][1] = m[0][1] * scaleY;
    
    res[1][0] = m[1][0] * scaleX;
    res[1][1] = m[1][1] * scaleY;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix2<T> PostRot(const Matrix2<T>& m, T radians)
{
    Matrix2<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs - m[0][1] * sn; 
    res[0][1] = m[0][1] * cs + m[0][0] * sn;
        
    res[1][0] = m[1][0] * cs - m[1][1] * sn;
    res[1][1] = m[1][1] * cs + m[1][0] * sn;
        
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix2<T> operator*(const Matrix2<T>& m, T v)
{
    return Matrix2<T>(m[0] * v, m[1] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix2<T> operator*(T v, const Matrix2<T>& m)
{
    return Matrix2<T>(m[0] * v, m[1] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix2<T> operator/(const Matrix2<T>& m, T v)
{
    return Matrix2<T>(m[0] / v, m[1] / v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector2<T> operator*(const Vector2<T>& v, const Matrix2<T>& m)
{
    return Vector2<T>(v[0] * m[0][0] + v[1] * m[1][0], v[0] * m[0][1] + v[1] * m[1][1]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix2<T> operator*(const Matrix2<T>& m0, const Matrix2<T>& m1)
{
    Matrix2<T> ret(m0);
    return ret *= m1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> Transpose(const Matrix3<T>& m)
{
    return Matrix3<T>(m[0][0], m[1][0], m[2][0], m[0][1], m[1][1], m[2][1], m[0][2], m[1][2],
        m[2][2]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
bool IsAffine(const Matrix3<T>& m)
{
    return fabsf(LengthSquared(Vector3<T>(m[0][2], m[1][2], m[2][2]) - Vector3<T>(T(0.0), T(0.0), 
        T(1.0)))) < 0.00001f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> Inverse(const Matrix3<T>& m)
{
    T determinant = Determinant(m);
    NS_ASSERT(fabsf(determinant) > std::numeric_limits<T>::epsilon());
    return Inverse(m, determinant);    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> Inverse(const Matrix3<T>& m, T determinant)
{
    NS_ASSERT(fabsf(determinant) > std::numeric_limits<T>::epsilon());
    
    return Matrix3<T>(m[1][1] * m[2][2] - m[1][2] * m[2][1],
        m[0][2] * m[2][1] - m[0][1] * m[2][2],
        m[0][1] * m[1][2] - m[0][2] * m[1][1],

        m[1][2] * m[2][0] - m[1][0] * m[2][2],
        m[0][0] * m[2][2] - m[0][2] * m[2][0],
        m[0][2] * m[1][0] - m[0][0] * m[1][2],

        m[1][0] * m[2][1] - m[1][1] * m[2][0],
        m[0][1] * m[2][0] - m[0][0] * m[2][1],
        m[0][0] * m[1][1] - m[0][1] * m[1][0]) / determinant; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
T Determinant(const Matrix3<T>& m)
{    
    return 
        (m[0][0] * m[1][1] * m[2][2] + m[0][1] * m[1][2] * m[2][0] + m[0][2] * m[1][0] * m[2][1]) - 
        (m[0][0] * m[1][2] * m[2][1] + m[0][1] * m[1][0] * m[2][2] + m[0][2] * m[1][1] * m[2][0]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> PreScale(T scaleX, T scaleY, T scaleZ, const Matrix3<T>& m)
{
    Matrix3<T> res;
    
    res[0][0] = m[0][0] * scaleX;
    res[0][1] = m[0][1] * scaleX;
    res[0][2] = m[0][2] * scaleX;
    
    res[1][0] = m[1][0] * scaleY;
    res[1][1] = m[1][1] * scaleY;
    res[1][2] = m[1][2] * scaleY;
    
    res[2][0] = m[2][0] * scaleZ;
    res[2][1] = m[2][1] * scaleZ;
    res[2][2] = m[2][2] * scaleZ;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> PreRotX(T radians, const Matrix3<T>& m)
{
    Matrix3<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0] = m[0]; 
        
    res[1][0] = m[1][0] * cs + m[2][0] * sn;
    res[1][1] = m[1][1] * cs + m[2][1] * sn;
    res[1][2] = m[1][2] * cs + m[2][2] * sn;
    
    res[2][0] = m[2][0] * cs - m[1][0] * sn;
    res[2][1] = m[2][1] * cs - m[1][1] * sn;
    res[2][2] = m[2][2] * cs - m[1][2] * sn;    
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> PreRotY(T radians, const Matrix3<T>& m)
{
    Matrix3<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs + m[2][0] * sn; 
    res[0][1] = m[0][1] * cs + m[2][1] * sn;
    res[0][2] = m[0][2] * cs + m[2][2] * sn;
    
    res[1] = m[1];
    
    res[2][0] = m[2][0] * cs - m[0][0] * sn;
    res[2][1] = m[2][1] * cs - m[0][1] * sn;
    res[2][2] = m[2][2] * cs - m[0][2] * sn;    
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> PreRotZ(T radians, const Matrix3<T>& m)
{
    Matrix3<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs + m[1][0] * sn; 
    res[0][1] = m[0][1] * cs + m[1][1] * sn;
    res[0][2] = m[0][2] * cs + m[1][2] * sn;    
    
    res[1][0] = m[1][0] * cs - m[0][0] * sn;
    res[1][1] = m[1][1] * cs - m[0][1] * sn;
    res[1][2] = m[1][2] * cs - m[0][2] * sn;
    
    res[2] = m[2];
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> PostScale(const Matrix3<T>& m, T scaleX, T scaleY, T scaleZ)
{
    Matrix3<T> res;
    
    res[0][0] = m[0][0] * scaleX;
    res[0][1] = m[0][1] * scaleY;
    res[0][2] = m[0][2] * scaleZ;
    
    res[1][0] = m[1][0] * scaleX;
    res[1][1] = m[1][1] * scaleY;
    res[1][2] = m[1][2] * scaleZ;
    
    res[2][0] = m[2][0] * scaleX;
    res[2][1] = m[2][1] * scaleY;
    res[2][2] = m[2][2] * scaleZ;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> PostRotX(const Matrix3<T>& m, T radians)
{
    Matrix3<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0]; 
    res[0][1] = m[0][1] * cs - m[0][2] * sn;
    res[0][2] = m[0][2] * cs + m[0][1] * sn;
    
    res[1][0] = m[1][0];
    res[1][1] = m[1][1] * cs - m[1][2] * sn;
    res[1][2] = m[1][2] * cs + m[1][1] * sn;
    
    res[2][0] = m[2][0];
    res[2][1] = m[2][1] * cs - m[2][2] * sn;
    res[2][2] = m[2][2] * cs + m[2][1] * sn;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> PostRotY(const Matrix3<T>& m, T radians)
{
    Matrix3<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs - m[0][2] * sn; 
    res[0][1] = m[0][1];
    res[0][2] = m[0][2] * cs + m[0][0] * sn;
    
    res[1][0] = m[1][0] * cs - m[1][2] * sn;
    res[1][1] = m[1][1];
    res[1][2] = m[1][2] * cs + m[1][0] * sn;
    
    res[2][0] = m[2][0] * cs - m[2][2] * sn;
    res[2][1] = m[2][1];
    res[2][2] = m[2][2] * cs + m[2][0] * sn;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix3<T> PostRotZ(const Matrix3<T>& m, T radians)
{
    Matrix3<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs - m[0][1] * sn; 
    res[0][1] = m[0][1] * cs + m[0][0] * sn;
    res[0][2] = m[0][2];    
    
    res[1][0] = m[1][0] * cs - m[1][1] * sn;
    res[1][1] = m[1][1] * cs + m[1][0] * sn;
    res[1][2] = m[1][2];
    
    res[2][0] = m[2][0] * cs - m[2][1] * sn;
    res[2][1] = m[2][1] * cs + m[2][0] * sn;
    res[2][2] = m[2][2];
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix3<T> operator*(const Matrix3<T>& m, T v)
{
    return Matrix3<T>(m[0] * v, m[1] * v, m[2] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix3<T> operator*(T v, const Matrix3<T>& m)
{
    return Matrix3<T>(m[0] * v, m[1] * v, m[2] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix3<T> operator/(const Matrix3<T>& m, T v)
{
    return Matrix3<T>(m[0] / v, m[1] / v, m[2] / v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector3<T> operator*(const Vector3<T>& v, const Matrix3<T>& m)
{
    return Vector3<T>(v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0], 
        v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1],
        v[0] * m[0][2] + v[1] * m[1][2] + v[2] * m[2][2]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix3<T> operator*(const Matrix3<T>& m0, const Matrix3<T>& m1)
{
    Matrix3<T> ret(m0);
    return ret *= m1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Matrix4<T> Transpose(const Matrix4<T>& m)
{
    return Matrix4<T>(m[0][0], m[1][0], m[2][0], m[3][0],
        m[0][1], m[1][1], m[2][1], m[3][1],
        m[0][2], m[1][2], m[2][2], m[3][2],
        m[0][3], m[1][3], m[2][3], m[3][3]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
bool IsAffine(const Matrix4<T>& m)
{
    return fabsf(LengthSquared(Vector4<T>(m[0][3], m[1][3], m[2][3], m[3][3]) - 
        Vector4<T>(T(0.0), T(0.0), T(0.0), T(1.0)))) < 0.00001f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix4<T> operator*(const Matrix4<T>& m, T v)
{
    return Matrix4<T>(m[0] * v, m[1] * v, m[2] * v, m[3] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix4<T> operator*(T v, const Matrix4<T>& m)
{
    return Matrix4<T>(m[0] * v, m[1] * v, m[2] * v, m[3] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix4<T> operator/(const Matrix4<T>& m, T v)
{
    return Matrix4<T>(m[0] / v, m[1] / v, m[2] / v, m[3] / v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector4<T> operator*(const Vector4<T>& v, const Matrix4<T>& m)
{
    return Vector4<T>(v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0] + v[3] * m[3][0],
        v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1] + v[3] * m[3][1],
        v[0] * m[0][2] + v[1] * m[1][2] + v[2] * m[2][2] + v[3] * m[3][2],
        v[0] * m[0][3] + v[1] * m[1][3] + v[2] * m[2][3] + v[3] * m[3][3]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix4<T> operator*(const Matrix4<T>& m0, const Matrix4<T>& m1)
{
    Matrix4<T> ret(m0);
    return ret *= m1;
}

}
