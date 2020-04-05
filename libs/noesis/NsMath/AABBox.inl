////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #514]
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsCore/Error.h>
#include <NsMath/Utils.h>
#include <EASTL/algorithm.h>

#include <float.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
AABBox2<T>::AABBox2(): mMin(FLT_MAX, FLT_MAX), mMax(-FLT_MAX, -FLT_MAX)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
template<class S> AABBox2<T>::AABBox2(const AABBox2<S>& v): mMin(v.mMin), mMax(v.mMax)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
AABBox2<T>::AABBox2(float minX, float minY, float maxX, float maxY): 
    mMin(minX, minY), mMax(maxX, maxY)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
AABBox2<T>::AABBox2(const Vector2<T>& min, const Vector2<T>& max): mMin(min), mMax(max)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
AABBox2<T>::AABBox2(const Vector2<T>& center, T halfSide)
{
    NS_ASSERT(halfSide > T(0.0));

    Vector2<T> halfSideVector(halfSide, halfSide);
    mMin = center - halfSideVector;
    mMax = center + halfSideVector;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
template<class S> AABBox2<T>& AABBox2<T>::operator=(const AABBox2<S>& v)
{
    mMin = v.mMin;
    mMax = v.mMax;
    
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
void AABBox2<T>::Reset()
{
    mMin = Vector2<T>(FLT_MAX, FLT_MAX);
    mMax = Vector2<T>(-FLT_MAX, -FLT_MAX);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Vector2<T> AABBox2<T>::operator[](uint32_t i) const
{
    NS_ASSERT(i < 4);
    return Vector2<T>(
        i & 1 ? mMax[0] : mMin[0],
        i & 2 ? mMax[1] : mMin[1]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
void AABBox2<T>::SetMin(const Vector2<T>& min)
{
    mMin = min;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector2<T>& AABBox2<T>::GetMin() const
{
    return mMin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
void AABBox2<T>::SetMax(const Vector2<T>& max)
{
    mMax = max;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const Vector2<T>& AABBox2<T>::GetMax() const
{
    return mMax;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Vector2<T> AABBox2<T>::GetCenter() const
{
    return T(0.5f) * (mMin + mMax);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
Vector2<T> AABBox2<T>::GetDiagonal() const
{
    return mMax - mMin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
AABBox2<T>& AABBox2<T>::operator+=(const AABBox2<T>& box)
{
    mMin[0] = Min(box.mMin[0], mMin[0]);
    mMin[1] = Min(box.mMin[1], mMin[1]);

    mMax[0] = Max(box.mMax[0], mMax[0]);
    mMax[1] = Max(box.mMax[1], mMax[1]);

    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
AABBox2<T>& AABBox2<T>::operator+=(const Vector2<T>& point)
{
    mMin[0] = Min(point[0], mMin[0]);
    mMin[1] = Min(point[1], mMin[1]);

    mMax[0] = Max(point[0], mMax[0]);
    mMax[1] = Max(point[1], mMax[1]);

    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
AABBox2<T>& AABBox2<T>::Scale(T s)
{
    Vector2<T> center = GetCenter();

    SetMin((mMin - center) * s + center);
    SetMax((mMax - center) * s + center);
    
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
bool AABBox2<T>::operator==(const AABBox2<T>& box) const
{
    return mMin == box.mMin && mMax == box.mMax;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
bool AABBox2<T>::operator!=(const AABBox2<T>& box) const
{
    return !(*this == box);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
bool AABBox2<T>::IsEmpty() const
{
    return mMax[0] < mMin[0] || mMax[1] < mMin[1];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const AABBox2<T> operator+(const AABBox2<T>& box0, const AABBox2<T>& box1)
{
    AABBox2<T> ret(box0);
    return ret += box1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const AABBox2<T> operator+(const AABBox2<T>& box, const Vector2<T>& point)
{
    AABBox2<T> ret(box);
    return ret += point;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const AABBox2<T> operator+(const Vector2<T>& point, const AABBox2<T>& box)
{
    AABBox2<T> ret(box);
    return ret += point;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const AABBox2<T> operator*(const AABBox2<T>& box, const Transform2<T>& mtx)
{
    if (box.IsEmpty()) return box;
    
    AABBox2<T> ret(mtx[2], mtx[2]);
   
    for (uint32_t i = 0; i < 2; i++)
    {
        for (uint32_t j = 0; j < 2; j++)
        {
            T a = mtx[j][i] * box.mMin[j];
            T b = mtx[j][i] * box.mMax[j];

            ret.mMin[i] += a < b ? a : b;
            ret.mMax[i] += a < b ? b : a;
        }
    }
    
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const AABBox2<T> operator*(const AABBox2<T>& box, T s)
{
    AABBox2<T> ret(box);
    ret.Scale(s);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const AABBox2<T> operator*(T s, const AABBox2<T>& box)
{
    AABBox2<T> ret(box);
    ret.Scale(s);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
bool Inside(const AABBox2<T>& box, const Vector2<T>& p)
{
    return (p[0] > box.GetMin()[0] && p[1] > box.GetMin()[1] &&
        p[0] < box.GetMax()[0] && p[1] < box.GetMax()[1]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
bool BoxesIntersect(const AABBox2<T>& box0, const AABBox2<T>& box1)
{
    return (box0.GetMin()[0] < box1.GetMax()[0] && box0.GetMin()[1] < box1.GetMax()[1] &&
        box0.GetMax()[0] > box1.GetMin()[0] && box0.GetMax()[1] > box1.GetMin()[1]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
AABBox2<T> Intersect(const AABBox2<T>& b0, const AABBox2<T>& b1)
{
    float minX = eastl::max_alt(b0.GetMin()[0], b1.GetMin()[0]);
    float maxX = eastl::min_alt(b0.GetMax()[0], b1.GetMax()[0]);

    float minY = eastl::max_alt(b0.GetMin()[1], b1.GetMin()[1]);
    float maxY = eastl::min_alt(b0.GetMax()[1], b1.GetMax()[1]);

    return AABBox2<T>(minX, minY, maxX, maxY);
}

}
