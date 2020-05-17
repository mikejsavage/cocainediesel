////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __DRAWING_COLOR_H__
#define __DRAWING_COLOR_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/StringFwd.h>
#include <NsDrawing/TypesApi.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// RGBA color in the sRGB (http://en.wikipedia.org/wiki/SRGB) space
////////////////////////////////////////////////////////////////////////////////////////////////////
struct Color
{
    float r;
    float g;
    float b;
    float a;

    /// Default constructor that creates a black color
    Color();

    /// Constructor from float components (0.0 - 1.0)
    Color(float r, float g, float b, float a = 1.0f);

    /// Constructor from integer components (0 - 255)
    Color(int r, int g, int b, int a = 255);

    /// Creates a color from an integer representation as obtained from GetPackedColorBGRA
    static Color FromPackedBGRA(uint32_t color);
    
    /// Creates a color from an integer representation as obtained from GetPackedColorRGBA
    static Color FromPackedRGBA(uint32_t color);

    /// Creates a sRGB color from components given in linear RGB space
    static Color FromLinearRGB(float r, float g, float b, float a = 1.0f);
    static Color FromLinearRGB(int r, int g, int b,  int a = 255);

    /// Returns the components of the color in linear RGB space
    void ToLinearRGB(float& r, float& g, float& b, float& a) const;
    void ToLinearRGB(int& r, int& g, int& b, int& a) const;

    /// Returns the BGRA packed representation
    uint32_t GetPackedColorBGRA() const;

    /// Returns the RGBA packed representation
    uint32_t GetPackedColorRGBA() const;

    /// Logic operators
    bool operator==(const Color& color) const;
    bool operator!=(const Color& color) const;
    
    /// Generates a string representation in the form #AARRGGBB
    NS_DRAWING_TYPES_API String ToString() const;

    /// Creates a color from a string. Valid formats are:
    ///  - #AARRGGBB 
    ///  - #RRGGBB
    ///  - #ARGB | 
    ///  - A predefined color from the list defined in 
    ///     http://msdn.microsoft.com/en-us/library/system.windows.media.colors.aspx
    NS_DRAWING_TYPES_API static bool TryParse(const char* str, Color& output);

    /// Constants
    NS_DRAWING_TYPES_API static const Color Black;
    NS_DRAWING_TYPES_API static const Color Blue;
    NS_DRAWING_TYPES_API static const Color Cyan;
    NS_DRAWING_TYPES_API static const Color DarkGray;
    NS_DRAWING_TYPES_API static const Color Gray;
    NS_DRAWING_TYPES_API static const Color Green;
    NS_DRAWING_TYPES_API static const Color LightGray;
    NS_DRAWING_TYPES_API static const Color Magenta;
    NS_DRAWING_TYPES_API static const Color Orange;
    NS_DRAWING_TYPES_API static const Color Pink;
    NS_DRAWING_TYPES_API static const Color Red;
    NS_DRAWING_TYPES_API static const Color White;
    NS_DRAWING_TYPES_API static const Color Yellow;

    NS_IMPLEMENT_INLINE_REFLECTION(Color, NoParent)
    {
        NsProp("r", &Color::r);
        NsProp("g", &Color::g);
        NsProp("b", &Color::b);
        NsProp("a", &Color::a);
    }
};

/// Returns the Premultiplied alpha representation of the given color
/// The Premultiplied alpha representation of RGBA is (RA, GA, BA, A)
/// Setting sRGB to false performs the premultiplication in linear space
Color PreMultiplyAlpha(const Color& color, bool sRGB = true);

/// Returns a linear interpolation between two colours
/// Setting sRGB to false perfoms the interpolation in linear space
Color Lerp(const Color& c0, const Color& c1, float t, bool sRGB = true);

/// Add two colours
/// Setting sRGB to false perfoms the operation in linear space
Color Add(const Color& c0, const Color& c1, bool sRGB = true);

/// Returns the multiplication of two given colors
Color Modulate(const Color& c0, const Color& c1);

}

#include <NsDrawing/Color.inl>

#endif
