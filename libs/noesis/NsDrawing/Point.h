////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #868]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __DRAWING_POINT_H__
#define __DRAWING_POINT_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>
#include <NsDrawing/TypesApi.h>
#include <NsDrawing/Size.h>
#include <NsMath/Vector.h>


namespace Noesis
{

struct Pointi;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Point. Represents an x- and y-coordinate pair in two-dimensional space.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct Point: public Vector2f
{
    /// Default constructor that creates a (0,0) point
    inline Point();

    /// Constructor for x, y
    inline Point(float x, float y);

    /// Construct from Pointi
    inline Point(const Pointi& point);

    /// Constructor from Vector
    inline Point(const Vector2f& v);

    /// Constructor from size
    explicit inline Point(const Size& size);

    /// Copy constructor
    inline Point(const Point& point);

    /// Copy operator
    inline Point& operator=(const Point& point);

    /// Generates a string representation of the point
    /// The string has the following form: "x,y"
    NS_DRAWING_TYPES_API NsString ToString() const;

    /// Tries to parse a Point from a string
    NS_DRAWING_TYPES_API static bool TryParse(const char* str, Point& result);

    /// TODO: Inline because inheriting from Vector does not allow exporting full class
    NS_IMPLEMENT_INLINE_REFLECTION(Point, Vector2f)
    {
        NsMeta<TypeId>("Point");
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Pointi. Represents an x- and y-coordinate pair in integer two-dimensional space.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct Pointi: public Vector2i
{
    /// Default constructor that creates a (0,0) point
    inline Pointi();

    /// Constructor for x, y
    inline Pointi(int x, int y);

    /// Constructor from float Point
    inline Pointi(const Point& p);

    /// Constructor from Vector
    inline Pointi(const Vector2i& v);

    /// Constructor from size
    explicit inline Pointi(const Sizei& size);

    /// Copy constructor
    inline Pointi(const Pointi& point);

    /// Copy operator
    inline Pointi& operator=(const Pointi& point);

    /// Generates a string representation of the point
    /// The string has the following form: "x,y"
    NS_DRAWING_TYPES_API NsString ToString() const;

    /// Tries to parse a Pointi from a string
    NS_DRAWING_TYPES_API static bool TryParse(const char* str, Pointi& result);

    /// TODO: Inline because inheriting from Vector does not allow exporting full class
    NS_IMPLEMENT_INLINE_REFLECTION_(Pointi, Vector2i)
};

}

#include <NsDrawing/Point.inl>

#endif
