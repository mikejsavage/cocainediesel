////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #514]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __MATH_AABBOX_H__
#define __MATH_AABBOX_H__


#include <NsCore/Noesis.h>
#include <NsMath/Vector.h>
#include <NsMath/Transform.h>
#include <NsMath/VectorMathApi.h>


namespace Noesis
{

template<class T> class AABBox2;
typedef AABBox2<float> AABBox2f;


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 2D Axis Aligned Bounding Box
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class AABBox2
{
public:
    /// Default constructor. An empty box is created
    inline AABBox2();
    
    /// Copy constructor
    inline AABBox2(const AABBox2& v) = default;
    template<class S> inline AABBox2(const AABBox2<S>& v);
    
    /// Constructor from minX, minY, maxX, maxY
    inline AABBox2(float minX, float minY, float maxX, float maxY);

    /// Constructor from min, max
    inline AABBox2(const Vector2<T>& min, const Vector2<T>& max);
    
    /// Constructor from center and side. A bounding box with size 2*halfSide will be constructed
    inline AABBox2(const Vector2<T>& center, T halfSide);

    /// Assignment
    inline AABBox2& operator=(const AABBox2& v) = default;
    template<class S> inline AABBox2& operator=(const AABBox2<S>& v);

    /// Reset to an empty bounding box
    inline void Reset();

    /// Accessors
    //@{
    /// \return ith corner point
    ///         X   Y
    /// [0] : (min,min)
    /// [1] : (max,min)
    /// [2] : (min,max)
    /// [3] : (max,max)
    inline Vector2<T> operator[](uint32_t i) const;

    inline void SetMin(const Vector2<T>& min);
    inline const Vector2<T>& GetMin() const;
    inline void SetMax(const Vector2<T>& max);
    inline const Vector2<T>& GetMax() const;
    //@}

    /// \return the center position of the box
    inline Vector2<T> GetCenter() const;

    /// \return the diagonal vector of the box
    inline Vector2<T> GetDiagonal() const;

    /// In-Place operators
    //@{
    /// Operator to expand the bounding box to fit a given box
    inline AABBox2& operator+=(const AABBox2& box);
    /// Operator to expand the box to fit the given point
    inline AABBox2& operator+=(const Vector2<T>& point);
    /// Scales the box. The origin of the scale if the center of the box. 
    inline AABBox2& Scale(T s);
    //@}

    ///// Logic operators
    //@{
    inline bool operator==(const AABBox2& box) const;
    inline bool operator!=(const AABBox2& box) const;
    //@}

    /// Check if the bounding box is empty
    inline bool IsEmpty() const;

private:
    template<class TT> friend class AABBox2;
    template<class TT> friend const AABBox2<TT> operator*(const AABBox2<TT>& box, 
        const Transform2<TT>& mtx);
    
    Vector2<T> mMin, mMax;
};

/// Binary operators
//@{
template<class T> const AABBox2<T> operator+(const AABBox2<T>& box0, const AABBox2<T>& box1);
template<class T> const AABBox2<T> operator+(const AABBox2<T>& box, const Vector2<T>& point);
template<class T> const AABBox2<T> operator+(const Vector2<T>& point, const AABBox2<T>& box);

// Transform a AABBox by a matrix returning a AABBox
template<class T> const AABBox2<T> operator*(const AABBox2<T>& box, const Transform2<T>& mtx);

/// Scale operators
template<class T> const AABBox2<T> operator*(const AABBox2<T>& box, T s);
template<class T> const AABBox2<T> operator*(T s, const AABBox2<T>& box);
//@}

/// Function to check if a point is inside a bounding box
template<class T> bool Inside(const AABBox2<T>& box, const Vector2<T>& p);

/// Function to check if two bounding boxes overlap
template<class T> bool BoxesIntersect(const AABBox2<T>& box0, const AABBox2<T>& box1);

/// Returns the intersection of two boxes
template<class T> AABBox2<T> Intersect(const AABBox2<T>& box0, const AABBox2<T>& box1);

}

// Inline include
#include <NsMath/AABBox.inl>

#endif
