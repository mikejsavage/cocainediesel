////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #868]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __DRAWING_SIZE_H__
#define __DRAWING_SIZE_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsDrawing/TypesApi.h>
#include <NsCore/NSTLForwards.h>


namespace Noesis
{

struct Size;
struct Sizei;
struct Point;
struct Pointi;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Size. Describes the size of an object.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_DRAWING_TYPES_API Size
{
    float width;
    float height;

    /// Constructor
    inline Size(float w = 0.0f, float h = 0.0f);

    /// Constructor from integer Sizei
    inline Size(const Sizei& size);

    /// Constructor from a Point
    explicit Size(const Point& point);

    /// Copy constructor
    inline Size(const Size& size);

    /// Copy operator
    inline Size& operator=(const Size& size);

    /// Comparison operators
    //@{
    inline bool operator==(const Size& size) const;
    inline bool operator!=(const Size& size) const;
    //@}

    /// Math operators
    //@{
    inline Size operator+(const Size& size) const;
    inline Size operator-(const Size& size) const;
    inline Size operator*(float k) const;
    inline Size operator/(float k) const;
    inline Size& operator+=(const Size& size);
    inline Size& operator-=(const Size& size);
    inline Size& operator*=(float k);
    inline Size& operator/=(float k);
    //@}

    /// Expands width and height with the specified size
    inline void Expand(const Size& size);

    /// Scales width and height by a factor
    inline void Scale(float scaleX, float scaleY);

    /// Generates a string representation of the size
    /// The string has the following form: "width,height"
    NsString ToString() const;

    /// Returns a hash code
    uint32_t GetHashCode() const;

    /// Tries to parse a Size from a string
    static bool TryParse(const char* str, Size& result);

    /// Empty size (width and height of 0)
    inline static Size Zero();

    /// Infinite size (defined as the higher positive integer)
    inline static Size Infinite();

    NS_DECLARE_REFLECTION(Size, NoParent)
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Sizei. Describes the integer size of an object.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_DRAWING_TYPES_API Sizei
{
    uint32_t width;
    uint32_t height;

    /// Constructor
    inline Sizei(uint32_t w = 0, uint32_t h = 0);

    /// Constructor from a float size
    inline Sizei(const Size& size);

    /// Constructor from a Pointi
    explicit Sizei(const Pointi& point);

    /// Copy constructor
    inline Sizei(const Sizei& size);

    /// Copy operator
    inline Sizei& operator=(const Sizei& size);

    /// Comparison operators
    //@{
    inline bool operator==(const Sizei& size) const;
    inline bool operator!=(const Sizei& size) const;
    //@}

    /// Math operators
    //@{
    inline Sizei operator+(const Sizei& size) const;
    inline Sizei operator-(const Sizei& size) const;
    inline Sizei operator*(uint32_t k) const;
    inline Sizei operator/(uint32_t k) const;
    inline Sizei& operator+=(const Sizei& size);
    inline Sizei& operator-=(const Sizei& size);
    inline Sizei& operator*=(uint32_t k);
    inline Sizei& operator/=(uint32_t k);
    //@}

    /// Expands width and height with the specified size
    inline void Expand(const Sizei& size);

    /// Scales width and height by a factor
    inline void Scale(uint32_t scaleX, uint32_t scaleY);

    /// Generates a string representation of the size
    /// The string has the following form: "width,height"
    NsString ToString() const;

    /// Returns a hash code
    uint32_t GetHashCode() const;

    /// Tries to parse a Sizei from a string
    static bool TryParse(const char* str, Sizei& result);

    /// Empty size (width and height of 0)
    inline static Sizei Zero();

    /// Infinite size (defined as the higher positive integer)
    inline static Sizei Infinite();

    NS_DECLARE_REFLECTION(Sizei, NoParent)
};

}

#include <NsDrawing/Size.inl>

#endif
