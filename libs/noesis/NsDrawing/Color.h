////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __DRAWING_COLOR_H__
#define __DRAWING_COLOR_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/NSTLForwards.h>
#include <NsDrawing/TypesApi.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// RGBA color in the sRGB (http://en.wikipedia.org/wiki/SRGB) space. Components are interpreted
/// in the 0.0 - 1.0 range as floats or in the range 0 - 255 as integers. An alpha value of 1.0 or
/// 255 represents an opaque color and an alpha value of 0.0 or 0 means that the color is fully 
/// transparent.
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_DRAWING_TYPES_API Color
{
public:
    /// Default constructor that creates a black color
    inline Color();
    
    // Constructor from float components (0.0 - 1.0)
    inline Color(float r, float g, float b, float a = 1.0f);
    
    // Constructor from integer components (0 - 255)
    inline Color(int r, int g, int b, int a = 255);

    // Creates a color from an integer representation as obtained from GetPackedColorBGRA
    static Color FromPackedBGRA(uint32_t color);
    
    // Creates a color from an integer representation as obtained from GetPackedColorRGBA
    static Color FromPackedRGBA(uint32_t color);

    // Creates a sRGB color from components given in linear RGB space
    static Color FromLinearRGB(float r, float g, float b, float a = 1.0f);
    static Color FromLinearRGB(int r, int g, int b,  int a = 255);

    // Returns the components of the color in linear RGB space
    void ToLinearRGB(float& r, float& g, float& b, float& a) const;
    void ToLinearRGB(int& r, int& g, int& b, int& a) const;
    
    // Setters
    //@{
    inline void SetRed(float red);
    inline void SetRed(int red);
    inline void SetGreen(float green);
    inline void SetGreen(int green);
    inline void SetBlue(float blue);
    inline void SetBlue(int blue);
    inline void SetAlpha(float alpha);
    inline void SetAlpha(int alpha);
    inline void SetPackedColorBGRA(uint32_t color);
    inline void SetPackedColorRGBA(uint32_t color);
    //@}

    // Getters
    //@{
    inline int GetRedI() const;
    inline int GetGreenI() const;
    inline int GetBlueI() const;
    inline int GetAlphaI() const;
    inline float GetRedF() const;
    inline float GetGreenF() const;
    inline float GetBlueF() const;
    inline float GetAlphaF() const;
    inline uint32_t GetPackedColorBGRA() const;
    inline uint32_t GetPackedColorRGBA() const;
    //@}
    
    /// Logic operators
    //@{
    inline bool operator==(const Color& color) const;
    inline bool operator!=(const Color& color) const;
    //@}
    
    /// Generates a string representation in the form #AARRGGBB
    NsString ToString() const;

    /// Returns a hash code
    uint32_t GetHashCode() const;

    /// Creates a color from a string. Valid formats are:
    ///  - #AARRGGBB 
    ///  - #RRGGBB
    ///  - #ARGB | 
    ///  - A predefined color from the list defined in 
    ///     http://msdn.microsoft.com/en-us/library/system.windows.media.colors.aspx
    //@{
    static bool TryParse(const char* str, Color& output);
    //@}

    /// Constant Colors
    //@{
    static const Color Black;
    static const Color Blue;
    static const Color Cyan;
    static const Color DarkGray;
    static const Color Gray;
    static const Color Green;
    static const Color LightGray;
    static const Color Magenta;
    static const Color Orange;
    static const Color Pink;
    static const Color Red;
    static const Color White;
    static const Color Yellow;
    //@}
    
private:
    float mColor[4];

    NS_DECLARE_REFLECTION(Color, NoParent)
};

/// Returns the Premultiplied alpha representation of the given color
/// The Premultiplied alpha representation of RGBA is (RA, GA, BA, A)
/// Setting sRGB to false performs the premultiplication in linear space
NS_DRAWING_TYPES_API Color PreMultiplyAlpha(const Color& color, bool sRGB = true);

/// Returns a linear interpolation between two colours
/// Setting sRGB to false perfoms the interpolation in linear space
NS_DRAWING_TYPES_API Color Lerp(const Color& c0, const Color& c1, float t, bool sRGB = true);

/// Add two colours
/// Setting sRGB to false perfoms the operation in linear space
NS_DRAWING_TYPES_API Color Add(const Color& c0, const Color& c1, bool sRGB = true);

/// Returns the multiplication of two given colors
NS_DRAWING_TYPES_API Color Modulate(const Color& c0, const Color& c1);

}

// Inline include
#include <NsDrawing/Color.inl>

#endif
