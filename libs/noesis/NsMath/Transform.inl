////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsMath/Utils.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> Inverse(const Transform2<T>& m)
{
    T determinant = m[0][0] * m[1][1] - m[0][1] * m[1][0];

    T xx = m[1][1] / determinant;
    T xy = -m[0][1] / determinant;

    T yx = -m[1][0] / determinant;
    T yy = m[0][0] / determinant;

    T zx = -m[2][0] * xx - m[2][1] * yx;
    T zy = -m[2][0] * xy - m[2][1] * yy;

    return Transform2<T>(xx, xy, yx, yy, zx, zy);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> Inverse(const Transform2<T>& m, T determinant)
{
    NS_ASSERT(!IsZero(determinant));

    T xx = m[1][1] / determinant;
    T xy = -m[0][1] / determinant;
    
    T yx = -m[1][0] / determinant;
    T yy = m[0][0] / determinant;

    T zx = -m[2][0] * xx - m[2][1] * yx;
    T zy = -m[2][0] * xy - m[2][1] * yy;

    return Transform2<T>(xx, xy, yx, yy, zx, zy);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> PreScale(T scaleX, T scaleY, const Transform2<T>& m)
{
    Transform2<T> res;
    
    res[0][0] = m[0][0] * scaleX;
    res[0][1] = m[0][1] * scaleX;
    
    res[1][0] = m[1][0] * scaleY;
    res[1][1] = m[1][1] * scaleY;
    
    res[2] = m[2];
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> PreTrans(T transX, T transY, const Transform2<T>& m)
{
    Transform2<T> res;
    
    res[0] = m[0];
    res[1] = m[1];
    
    res[2][0] = m[2][0] + m[0][0] * transX + m[1][0] * transY;
    res[2][1] = m[2][1] + m[0][1] * transX + m[1][1] * transY;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> PreRot(T radians, const Transform2<T>& m)
{
    Transform2<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs + m[1][0] * sn; 
    res[0][1] = m[0][1] * cs + m[1][1] * sn;   
    
    res[1][0] = m[1][0] * cs - m[0][0] * sn;
    res[1][1] = m[1][1] * cs - m[0][1] * sn;
    
    res[2] = m[2];
        
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> PreSkew(T radiansX, T radiansY, const Transform2<T>& m)
{
    Transform2<T> res;

    res[0][0] = m[0][0] + m[1][0] * radiansY;
    res[0][1] = m[0][1] + m[1][1] * radiansY;

    res[1][0] = m[1][0] + m[0][0] * radiansX;
    res[1][1] = m[1][1] + m[0][1] * radiansX;

    res[2] = m[2];

    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> PostScale(const Transform2<T>& m, T scaleX, T scaleY)
{
    Transform2<T> res;
    
    res[0][0] = m[0][0] * scaleX;
    res[0][1] = m[0][1] * scaleY;
    
    res[1][0] = m[1][0] * scaleX;
    res[1][1] = m[1][1] * scaleY;
    
    res[2][0] = m[2][0] * scaleX;
    res[2][1] = m[2][1] * scaleY;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> PostTrans(const Transform2<T>& m, T transX, T transY)
{
    Transform2<T> res;
    
    res[0] = m[0];
    res[1] = m[1];
    
    res[2][0] = m[2][0] + transX;
    res[2][1] = m[2][1] + transY;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> PostRot(const Transform2<T>& m, T radians)
{
    Transform2<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs - m[0][1] * sn; 
    res[0][1] = m[0][1] * cs + m[0][0] * sn;    
    
    res[1][0] = m[1][0] * cs - m[1][1] * sn;
    res[1][1] = m[1][1] * cs + m[1][0] * sn;
    
    res[2][0] = m[2][0] * cs - m[2][1] * sn;
    res[2][1] = m[2][1] * cs + m[2][0] * sn;    
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform2<T> PostSkew(const Transform2<T>& m, T radiansX, T radiansY)
{
    Transform2<T> res;

    res[0][0] = m[0][0] + m[0][1] * radiansX;
    res[0][1] = m[0][1] + m[0][0] * radiansY;

    res[1][0] = m[1][0] + m[1][1] * radiansX;
    res[1][1] = m[1][1] + m[1][0] * radiansY;
    
    res[2][0] = m[2][0] + m[2][1] * radiansX;
    res[2][1] = m[2][1] + m[2][0] * radiansY;

    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Transform2<T> operator*(const Transform2<T>& m, T v)
{
    return Transform2<T>(m[0] * v, m[1] * v, m[2] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Transform2<T> operator*(T v, const Transform2<T>& m)
{
    return Transform2<T>(m[0] * v, m[1] * v, m[2] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Transform2<T> operator/(const Transform2<T>& m, T v)
{
    return Transform2<T>(m[0] / v, m[1] / v, m[2] / v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector2<T> PointTransform(const Vector2<T>& v, const Transform2<T>& m)
{
    return Vector2<T>(v[0] * m[0][0] + v[1] * m[1][0] + m[2][0],
        v[0] * m[0][1] + v[1] * m[1][1] + m[2][1]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector2<T> VectorTransform(const Vector2<T>& v, const Transform2<T>& m)
{
    return Vector2<T>(v[0] * m[0][0] + v[1] * m[1][0],
        v[0] * m[0][1] + v[1] * m[1][1]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector2<T> operator*(const Vector2<T>& v, const Transform2<T>& m)
{
    return PointTransform(v, m);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector3<T> operator*(const Vector3<T>& v, const Transform2<T>& m)
{
    return Vector3<T>(v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0],
        v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1], v[2]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Transform2<T> operator*(const Transform2<T>& m0, const Transform2<T>& m1)
{
    Transform2<T> ret(m0);
    return ret *= m1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix3<T> operator*(const Transform2<T>& m0, const Matrix3<T>& m1)
{
    Matrix3<T> res;
    
    res[0] = Vector3<T>(
        m0[0][0] * m1[0][0] + m0[0][1] * m1[1][0],
        m0[0][0] * m1[0][1] + m0[0][1] * m1[1][1],
        m0[0][0] * m1[0][2] + m0[0][1] * m1[1][2]);

    res[1] = Vector3<T>(
        m0[1][0] * m1[0][0] + m0[1][1] * m1[1][0],
        m0[1][0] * m1[0][1] + m0[1][1] * m1[1][1],
        m0[1][0] * m1[0][2] + m0[1][1] * m1[1][2]);
        
    res[2] = Vector3<T>(
        m0[2][0] * m1[0][0] + m0[2][1] * m1[1][0] + m1[2][0],
        m0[2][0] * m1[0][1] + m0[2][1] * m1[1][1] + m1[2][1],
        m0[2][0] * m1[0][2] + m0[2][1] * m1[1][2] + m1[2][2]);
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix3<T> operator*(const Matrix3<T>& m0, const Transform2<T>& m1)
{
    Matrix3<T> res;

    res[0] = Vector3<T>(m0[0][0] * m1[0][0] + m0[0][1] * m1[1][0] + m0[0][2] * m1[2][0],
                      m0[0][0] * m1[0][1] + m0[0][1] * m1[1][1] + m0[0][2] * m1[2][1],
                      m0[0][2]);
                    
    res[1] = Vector3<T>(m0[1][0] * m1[0][0] + m0[1][1] * m1[1][0] + m0[1][2] * m1[2][0],
                      m0[1][0] * m1[0][1] + m0[1][1] * m1[1][1] + m0[1][2] * m1[2][1],
                      m0[1][2]);
                       
    res[2] = Vector3<T>(m0[2][0] * m1[0][0] + m0[2][1] * m1[1][0] + m0[2][2] * m1[2][0],
                      m0[2][0] * m1[0][1] + m0[2][1] * m1[1][1] + m0[2][2] * m1[2][1],
                      m0[2][2]);
                    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float ScaleX(const Transform2f& m)
{
    return Length(m[0]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float ScaleY(const Transform2f& m)
{
    return Length(m[1]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float TransX(const Transform2f& m)
{
    return m[2][0];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float TransY(const Transform2f& m)
{
    return m[2][1];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float MaxScale(const Transform2f& m)
{
    return Max(Length(m[0]), Length(m[1]));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsRotating(const Transform2f& m)
{
    // Note that this is not very precise when transform has shearing 
    return !IsZero(m[0][1]) || !IsZero(m[1][0]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> Inverse(const Transform3<T>& m)
{
    Transform3<T> res;
    
    Matrix3<T> rotInv = Inverse(Matrix3<T>(m[0], m[1], m[2]));
    
    res[0] = rotInv[0];
    res[1] = rotInv[1];
    res[2] = rotInv[2];
    res[3] = Vector3<T>(-m[3][0] * rotInv[0][0] - m[3][1] * rotInv[1][0] - m[3][2] * rotInv[2][0],
        -m[3][0] * rotInv[0][1] - m[3][1] * rotInv[1][1] - m[3][2] * rotInv[2][1],
        -m[3][0] * rotInv[0][2] - m[3][1] * rotInv[1][2] - m[3][2] * rotInv[2][2]);
                      
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PreScale(T scaleX, T scaleY, T scaleZ, const Transform3<T>& m)
{
    Transform3<T> res;
    
    res[0][0] = m[0][0] * scaleX;
    res[0][1] = m[0][1] * scaleX;
    res[0][2] = m[0][2] * scaleX;
    
    res[1][0] = m[1][0] * scaleY;
    res[1][1] = m[1][1] * scaleY;
    res[1][2] = m[1][2] * scaleY;
    
    res[2][0] = m[2][0] * scaleZ;
    res[2][1] = m[2][1] * scaleZ;
    res[2][2] = m[2][2] * scaleZ;
    
    res[3] = m[3];
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PreTrans(T transX, T transY, T transZ, const Transform3<T>& m)
{
    Transform3<T> res;
    
    res[0] = m[0];
    res[1] = m[1];
    res[2] = m[2];
    
    res[3][0] = m[3][0] + m[0][0] * transX + m[1][0] * transY + m[2][0] * transZ;
    res[3][1] = m[3][1] + m[0][1] * transX + m[1][1] * transY + m[2][1] * transZ;
    res[3][2] = m[3][2] + m[0][2] * transX + m[1][2] * transY + m[2][2] * transZ;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PreRotX(T radians, const Transform3<T>& m)
{
    Transform3<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0] = m[0]; 
    
    res[1][0] = m[1][0] * cs + m[2][0] * sn;
    res[1][1] = m[1][1] * cs + m[2][1] * sn;
    res[1][2] = m[1][2] * cs + m[2][2] * sn;
    
    res[2][0] = m[2][0] * cs - m[1][0] * sn;
    res[2][1] = m[2][1] * cs - m[1][1] * sn;
    res[2][2] = m[2][2] * cs - m[1][2] * sn;
    
    res[3] = m[3];
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PreRotY(T radians, const Transform3<T>& m)
{
    Transform3<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs + m[2][0] * sn; 
    res[0][1] = m[0][1] * cs + m[2][1] * sn;
    res[0][2] = m[0][2] * cs + m[2][2] * sn;
    
    res[1] = m[1];
    
    res[2][0] = m[2][0] * cs - m[0][0] * sn;
    res[2][1] = m[2][1] * cs - m[0][1] * sn;
    res[2][2] = m[2][2] * cs - m[0][2] * sn;
    
    res[3] = m[3];
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PreRotZ(T radians, const Transform3<T>& m)
{
    Transform3<T> res;
    
    T cs = cosf(radians);
    T sn = sinf(radians);
    
    res[0][0] = m[0][0] * cs + m[1][0] * sn; 
    res[0][1] = m[0][1] * cs + m[1][1] * sn;
    res[0][2] = m[0][2] * cs + m[1][2] * sn;    
    
    res[1][0] = m[1][0] * cs - m[0][0] * sn;
    res[1][1] = m[1][1] * cs - m[0][1] * sn;
    res[1][2] = m[1][2] * cs - m[0][2] * sn;
    
    res[2] = m[2];
    res[3] = m[3];
        
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PostScale(const Transform3<T>& m, T scaleX, T scaleY, T scaleZ)
{
    Transform3<T> res;
    
    res[0][0] = m[0][0] * scaleX;
    res[0][1] = m[0][1] * scaleY;
    res[0][2] = m[0][2] * scaleZ;
    
    res[1][0] = m[1][0] * scaleX;
    res[1][1] = m[1][1] * scaleY;
    res[1][2] = m[1][2] * scaleZ;
    
    res[2][0] = m[2][0] * scaleX;
    res[2][1] = m[2][1] * scaleY;
    res[2][2] = m[2][2] * scaleZ;
    
    res[3][0] = m[3][0] * scaleX;
    res[3][1] = m[3][1] * scaleY;
    res[3][2] = m[3][2] * scaleZ;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PostTrans(const Transform3<T>& m, T transX, T transY, T transZ)
{
    Transform3<T> res;
    
    res[0] = m[0];
    res[1] = m[1];
    res[2] = m[2];
    
    res[3][0] = m[3][0] + transX;
    res[3][1] = m[3][1] + transY;
    res[3][2] = m[3][2] + transZ;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PostRotX(const Transform3<T>& m, T radians)
{
    Transform3<T> res;
    
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
    
    res[3][0] = m[3][0];
    res[3][1] = m[3][1] * cs - m[3][2] * sn;
    res[3][2] = m[3][2] * cs + m[3][1] * sn;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PostRotY(const Transform3<T>& m, T radians)
{
    Transform3<T> res;
    
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
    
    res[3][0] = m[3][0] * cs - m[3][2] * sn;
    res[3][1] = m[3][1];
    res[3][2] = m[3][2] * cs + m[3][0] * sn;
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Transform3<T> PostRotZ(const Transform3<T>& m, T radians)
{
    Transform3<T> res;
    
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
    
    res[3][0] = m[3][0] * cs - m[3][1] * sn;
    res[3][1] = m[3][1] * cs + m[3][0] * sn;
    res[3][2] = m[3][2];    
    
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Transform3<T> operator*(const Transform3<T>& m, T v)
{
    return Transform3<T>(m[0] * v, m[1] * v, m[2] * v, m[3] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Transform3<T> operator*(T v, const Transform3<T>& m)
{
    return Transform3<T>(m[0] * v, m[1] * v, m[2] * v, m[3] * v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Transform3<T> operator/(const Transform3<T>& m, T v)
{
    return Transform3<T>(m[0] / v, m[1] / v, m[2] / v, m[3] / v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector3<T> PointTransform(const Vector3<T>& v, const Transform3<T>& m)
{
    return Vector3<T>(v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0] + m[3][0],
        v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1] + m[3][1],
        v[0] * m[0][2] + v[1] * m[1][2] + v[2] * m[2][2] + m[3][2]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector3<T> VectorTransform(const Vector3<T>& v, const Transform3<T>& m)
{
    return Vector3<T>(v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0],
        v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1],
        v[0] * m[0][2] + v[1] * m[1][2] + v[2] * m[2][2]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector3<T> operator*(const Vector3<T>& v, const Transform3<T>& m)
{
    return PointTransform(v, m);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector4<T> operator*(const Vector4<T>& v, const Transform3<T>& m)
{
    return Vector4<T>(v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0] + v[3] * m[3][0],
        v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1] + v[3] * m[3][1],
        v[0] * m[0][2] + v[1] * m[1][2] + v[2] * m[2][2] + v[3] * m[3][2], v[3]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Transform3<T> operator*(const Transform3<T>& m0, const Transform3<T>& m1)
{
    Transform3<T> ret(m0);
    return ret *= m1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix4<T> operator*(const Transform3<T>& m0, const Matrix4<T>& m1)
{
    Matrix4<T> res;
    
    res[0] = Vector4<T>(
        m0[0][0] * m1[0][0] + m0[0][1] * m1[1][0] + m0[0][2] * m1[2][0],
        m0[0][0] * m1[0][1] + m0[0][1] * m1[1][1] + m0[0][2] * m1[2][1],
        m0[0][0] * m1[0][2] + m0[0][1] * m1[1][2] + m0[0][2] * m1[2][2],
        m0[0][0] * m1[0][3] + m0[0][1] * m1[1][3] + m0[0][2] * m1[2][3]);

    res[1] = Vector4<T>(
        m0[1][0] * m1[0][0] + m0[1][1] * m1[1][0] + m0[1][2] * m1[2][0],
        m0[1][0] * m1[0][1] + m0[1][1] * m1[1][1] + m0[1][2] * m1[2][1],
        m0[1][0] * m1[0][2] + m0[1][1] * m1[1][2] + m0[1][2] * m1[2][2],
        m0[1][0] * m1[0][3] + m0[1][1] * m1[1][3] + m0[1][2] * m1[2][3]);
        
    res[2] = Vector4<T>(
        m0[2][0] * m1[0][0] + m0[2][1] * m1[1][0] + m0[2][2] * m1[2][0],
        m0[2][0] * m1[0][1] + m0[2][1] * m1[1][1] + m0[2][2] * m1[2][1],
        m0[2][0] * m1[0][2] + m0[2][1] * m1[1][2] + m0[2][2] * m1[2][2],
        m0[2][0] * m1[0][3] + m0[2][1] * m1[1][3] + m0[2][2] * m1[2][3]);
        
    res[3] = Vector4<T>(
        m0[3][0] * m1[0][0] + m0[3][1] * m1[1][0] + m0[3][2] * m1[2][0] + m1[3][0],
        m0[3][0] * m1[0][1] + m0[3][1] * m1[1][1] + m0[3][2] * m1[2][1] + m1[3][1],
        m0[3][0] * m1[0][2] + m0[3][1] * m1[1][2] + m0[3][2] * m1[2][2] + m1[3][2],
        m0[3][0] * m1[0][3] + m0[3][1] * m1[1][3] + m0[3][2] * m1[2][3] + m1[3][3]);
        
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Matrix4<T> operator*(const Matrix4<T>& m0, const Transform3<T>& m1)
{
    Matrix4<T> res;

    res[0] = Vector4<T>(
        m0[0][0] * m1[0][0] + m0[0][1] * m1[1][0] + m0[0][2] * m1[2][0] + m0[0][3] * m1[3][0],
        m0[0][0] * m1[0][1] + m0[0][1] * m1[1][1] + m0[0][2] * m1[2][1] + m0[0][3] * m1[3][1],
        m0[0][0] * m1[0][2] + m0[0][1] * m1[1][2] + m0[0][2] * m1[2][2] + m0[0][3] * m1[3][2],
                                                                          m0[0][3]);

    res[1] = Vector4<T>(
        m0[1][0] * m1[0][0] + m0[1][1] * m1[1][0] + m0[1][2] * m1[2][0] + m0[1][3] * m1[3][0],
        m0[1][0] * m1[0][1] + m0[1][1] * m1[1][1] + m0[1][2] * m1[2][1] + m0[1][3] * m1[3][1],
        m0[1][0] * m1[0][2] + m0[1][1] * m1[1][2] + m0[1][2] * m1[2][2] + m0[1][3] * m1[3][2],
                                                                          m0[1][3]);
        
    res[2] = Vector4<T>(
        m0[2][0] * m1[0][0] + m0[2][1] * m1[1][0] + m0[2][2] * m1[2][0] + m0[2][3] * m1[3][0],
        m0[2][0] * m1[0][1] + m0[2][1] * m1[1][1] + m0[2][2] * m1[2][1] + m0[2][3] * m1[3][1],
        m0[2][0] * m1[0][2] + m0[2][1] * m1[1][2] + m0[2][2] * m1[2][2] + m0[2][3] * m1[3][2],
                                                                          m0[2][3]);
        
    res[3] = Vector4<T>(
        m0[3][0] * m1[0][0] + m0[3][1] * m1[1][0] + m0[3][2] * m1[2][0] + m0[3][3] * m1[3][0],
        m0[3][0] * m1[0][1] + m0[3][1] * m1[1][1] + m0[3][2] * m1[2][1] + m0[3][3] * m1[3][1],
        m0[3][0] * m1[0][2] + m0[3][1] * m1[1][2] + m0[3][2] * m1[2][2] + m0[3][3] * m1[3][2],
                                                                          m0[3][3]);

    return res;
}

}
