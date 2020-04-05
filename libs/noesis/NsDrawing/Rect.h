////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #868]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __DRAWING_RECT_H__
#define __DRAWING_RECT_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/NSTLForwards.h>
#include <NsDrawing/TypesApi.h>


namespace Noesis
{

template<class T> class Transform2;
typedef Transform2<float> Transform2f;
template<class T> class Matrix4;
typedef Matrix4<float> Matrix4f;
struct Size;
struct Sizei;
struct Point;
struct Pointi;
struct Recti;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A 2D Rectangle defined by an origin and a size
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_DRAWING_TYPES_API Rect
{
    float x;
    float y;
    float width;
    float height;

    /// Constructor from coordinates for the sides of the rectangle
    /// The width of the new rectangle is right - left
    /// The height of the new rectangle is bottom - top
    inline Rect(float left = 0, float top = 0, float right = 0, float bottom = 0);

    /// Constructor from size. The created rectangle is located at the origin (0,0)
    inline explicit Rect(const Size& size);

    /// Constructor from location and size
    inline Rect(const Point& location, const Size& size);

    /// Constructor from an integer rect
    inline Rect(const Recti& rect);

    /// Copy constructor
    inline Rect(const Rect& rect);

    /// Copy operator
    inline Rect& operator=(const Rect& rect);

    /// Comparison operators
    //@{
    inline bool operator==(const Rect& rect) const;
    inline bool operator!=(const Rect& rect) const;
    //@}

    /// Gets or sets the position of the rectangle's top-left corner
    //@{
    inline Point GetLocation() const;
    inline void SetLocation(const Point& location);
    //@}

    /// Gets or sets the width and height of the rectangle
    //@{
    inline Size GetSize() const;
    inline void SetSize(const Size& size);
    //@}

    /// Accessors to rectangle sides
    //@{
    inline float GetLeft() const;
    inline float GetRight() const;
    inline float GetTop() const;
    inline float GetBottom() const;
    //@}

    /// Accessors to rectangle corners
    //@{
    inline Point GetTopLeft() const;
    inline Point GetTopRight() const;
    inline Point GetBottomLeft() const;
    inline Point GetBottomRight() const;
    //@}

    /// Indicates whether this rectangle is empty (its size is 0)
    inline bool IsEmpty() const;

    /// Determines whether this rectangle contains the specified point or rectangle
    //@{
    inline bool Contains(float x, float y) const;
    inline bool Contains(const Point& point) const;
    inline bool Contains(const Rect& rect) const;
    //@}

    /// Expands or contracts the rectangle by the specified amount
    /// The width of the resulting rectangle is increased or decreased by twice the specified width
    /// offset, because it is applied to both the left and right sides of the rectangle. Likewise,
    /// the height of the rectangle is increased or decreased by twice the specified height
    /// If the specified width or height modifiers shrink the rectangle by more than its current
    /// width or height (giving a negative area) this rectangle becomes the Empty rectangle
    //@{
    inline void Inflate(float width, float height);
    inline void Inflate(const Size& size);
    //@}

    /// Finds the intersection of two rectangles and stores the result as a Rect structure
    Rect Intersect(const Rect& rect) const;

    /// Indicates whether the specified rectangle intersects with this rectangle
    inline bool IntersectsWith(const Rect& rect) const;

    /// Translates the rectangle by the specified amount
    inline void Offset(const Point& offset);

    /// Multiplies the size of the current rectangle by the specified x and y value
    inline void Scale(float scaleX, float scaleY);

    /// Expands a rectangle exactly enough to contain the specified rectangle or point
    //@{
    inline void Expand(const Point& point);
    inline void Expand(const Rect& rect);
    //@}
    
    /// Transforms the rectangle by the given matrix
    //@{
    void Transform(const Transform2f& mtx);
    void Transform(const Matrix4f& mtx);
    //@}

    /// Creates a string representation of this rectangle
    /// The string has the following form: "x,y,width,height"
    NsString ToString() const;

    /// Returns a hash code
    uint32_t GetHashCode() const;

    /// Tries to parse a Rect from a string
    static bool TryParse(const char* str, Rect& result);

    /// Empty rect (with all members set to 0)
    inline static Rect Empty();

    /// Infinite box (with no limits, it ranges from -Infinity to +Infinity)
    inline static Rect Infinite();

    NS_DECLARE_REFLECTION(Rect, NoParent)
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A 2D Rectangle defined by an integer origin and an integer size
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_DRAWING_TYPES_API Recti
{
    int x;
    int y;
    uint32_t width;
    uint32_t height;

    /// Constructor from coordinates for the sides of the rectangle
    /// The width of the new rectangle is right - left
    /// The height of the new rectangle is bottom - top
    inline Recti(int left = 0, int top = 0, int right = 0, int bottom = 0);

    /// Constructor from size. The created rectangle is located at the origin (0,0)
    inline explicit Recti(const Sizei& size);

    /// Constructor from location and size
    inline Recti(const Pointi& location, const Sizei& size);

    /// Copy constructor
    inline Recti(const Recti& rect);

    /// Copy operator
    inline Recti& operator=(const Recti& rect);

    /// Comparison operators
    //@{
    inline bool operator==(const Recti& rect) const;
    inline bool operator!=(const Recti& rect) const;
    //@}

    /// Gets or sets the position of the rectangle's top-left corner
    //@{
    inline Pointi GetLocation() const;
    inline void SetLocation(const Pointi& location);
    //@}

    /// Gets or sets the width and height of the rectangle
    //@{
    inline Sizei GetSize() const;
    inline void SetSize(const Sizei& size);
    //@}

    /// Accessors to rectangle sides
    //@{
    inline int GetLeft() const;
    inline int GetRight() const;
    inline int GetTop() const;
    inline int GetBottom() const;
    //@}

    /// Accessors to rectangle corners
    //@{
    inline Pointi GetTopLeft() const;
    inline Pointi GetTopRight() const;
    inline Pointi GetBottomLeft() const;
    inline Pointi GetBottomRight() const;
    //@}

    /// Indicates whether this rectangle is empty (its size is 0)
    inline bool IsEmpty() const;

    /// Determines whether this rectangle contains the specified point or rectangle
    //@{
    inline bool Contains(int x, int y) const;
    inline bool Contains(const Pointi& point) const;
    inline bool Contains(const Recti& rect) const;
    //@}

    /// Expands or contracts the rectangle by the specified amount
    /// The width of the resulting rectangle is increased or decreased by twice the specified width
    /// offset, because it is applied to both the left and right sides of the rectangle. Likewise,
    /// the height of the rectangle is increased or decreased by twice the specified height
    /// If the specified width or height modifiers shrink the rectangle by more than its current
    /// width or height (giving a negative area) this rectangle becomes the Empty rectangle
    //@{
    inline void Inflate(int width, int height);
    inline void Inflate(const Sizei& size);
    //@}

    /// Finds the intersection of two rectangles and stores the result as a Recti structure
    Recti Intersect(const Recti& rect) const;

    /// Indicates whether the specified rectangle intersects with this rectangle
    inline bool IntersectsWith(const Recti& rect) const;

    /// Translates the rectangle by the specified amount
    inline void Offset(const Pointi& offset);

    /// Multiplies the size of the current rectangle by the specified x and y value
    inline void Scale(int scaleX, int scaleY);

    /// Expands a rectangle exactly enough to contain the specified rectangle or point
    //@{
    inline void Expand(const Pointi& point);
    inline void Expand(const Recti& rect);
    //@}

    /// Creates a string representation of this rectangle
    /// The string has the following form: "x,y,width,height"
    NsString ToString() const;

    /// Returns a hash code
    uint32_t GetHashCode() const;

    /// Tries to parse a Recti from a string
    static bool TryParse(const char* str, Recti& result);

    /// Empty rect (with all members set to 0)
    inline static Recti Empty();

    /// Infinite box (with no limits, it ranges from -Infinity to +Infinity)
    inline static Recti Infinite();

    NS_DECLARE_REFLECTION(Recti, NoParent)
};

}

#include <NsDrawing/Rect.inl>

#endif
