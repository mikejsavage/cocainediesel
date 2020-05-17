////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsCore/Math.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color::Color(): r(0.0f), g(0.0f), b(0.0f), a(1.0f)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color::Color(float rr, float gg, float bb, float aa): r(rr), g(gg), b(bb), a(aa)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color::Color(int rr, int gg, int bb, int aa): r(rr / 255.0f), g(gg / 255.0f), b(bb / 255.0f),
    a(aa / 255.0f)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color Color::FromPackedBGRA(uint32_t color)
{
    return Color(((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f, 
        (color & 255) / 255.0f, (color >> 24) / 255.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color Color::FromPackedRGBA(uint32_t color)
{
    return Color((color & 255) / 255.0f, ((color >> 8) & 255) / 255.0f, 
        ((color >> 16) & 255) / 255.0f, (color >> 24) / 255.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline float Gamma(float v)
{
    /// http://www.w3.org/Graphics/Color/sRGB.html
    return v <= 0.00304f ? v * 12.92f : 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline float DeGamma(float v)
{
    /// http://www.w3.org/Graphics/Color/sRGB.html
    return v <= 0.03928f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color Color::FromLinearRGB(float r, float g, float b, float a)
{
    return Color(Gamma(r), Gamma(g), Gamma(b), a);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color Color::FromLinearRGB(int r, int g, int b,  int a)
{
    return Color(Gamma(r / 255.0f), Gamma(g / 255.0f), Gamma(b / 255.0f), a / 255.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void Color::ToLinearRGB(float& red, float& green, float& blue, float& alpha) const
{
    red = DeGamma(r);
    green = DeGamma(g);
    blue = DeGamma(b);
    alpha = a;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void Color::ToLinearRGB(int& r_, int& g_, int& b_, int& a_) const
{
    float red = DeGamma(r);
    float green = DeGamma(g);
    float blue = DeGamma(b);
    float alpha = a;

    r_ = Clip(Trunc(red * 255.0f), 0, 255);
    g_ = Clip(Trunc(green * 255.0f), 0, 255);
    b_ = Clip(Trunc(blue * 255.0f), 0, 255);
    a_ = Clip(Trunc(alpha * 255.0f), 0, 255);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline uint32_t Color::GetPackedColorBGRA() const
{
    uint32_t red = Clip(Trunc(r * 255.0f), 0, 255);
    uint32_t green = Clip(Trunc(g * 255.0f), 0, 255);
    uint32_t blue = Clip(Trunc(b * 255.0f), 0, 255);
    uint32_t alpha = Clip(Trunc(a * 255.0f), 0, 255);

    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline uint32_t Color::GetPackedColorRGBA() const
{
    uint32_t red = Clip(Trunc(r * 255.0f), 0, 255);
    uint32_t green = Clip(Trunc(g * 255.0f), 0, 255);
    uint32_t blue = Clip(Trunc(b * 255.0f), 0, 255);
    uint32_t alpha = Clip(Trunc(a * 255.0f), 0, 255);

    return (alpha << 24) | (blue << 16) | (green << 8) | red;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool Color::operator==(const Color& color) const
{
    return r == color.r && g == color.g && b == color.b && a == color.a;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool Color::operator!=(const Color& color) const
{
    return !(*this == color);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color PreMultiplyAlpha(const Color& color, bool sRGB)
{
    // http://ssp.impulsetrain.com/gamma-premult.html
    const float a_ = sRGB ? color.a : Gamma(color.a);
    return Color(color.r * a_, color.g * a_, color.b * a_, color.a);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color Lerp(const Color& c0, const Color& c1, float t, bool sRGB)
{
    if (sRGB)
    {
        return Color
        (
            Lerp(c0.r, c1.r, t),
            Lerp(c0.g, c1.g, t),
            Lerp(c0.b, c1.b, t),
            Lerp(c0.a, c1.a, t)
        );
    }
    else
    {
        float lr0, lg0, lb0, la0;
        c0.ToLinearRGB(lr0, lg0, lb0, la0);
        
        float lr1, lg1, lb1, la1;
        c1.ToLinearRGB(lr1, lg1, lb1, la1);

        return Color::FromLinearRGB
        (
            Lerp(lr0, lr1, t), 
            Lerp(lg0, lg1, t), 
            Lerp(lb0, lb1, t), 
            Lerp(la0, la1, t)
        );
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color Add(const Color& c0, const Color& c1, bool sRGB)
{
    if (sRGB)
    {
        return Color(c0.r + c1.r, c0.g + c1.g, c0.b + c1.b, c0.a + c1.a);
    }
    else
    {
        float lr0, lg0, lb0, la0;
        c0.ToLinearRGB(lr0, lg0, lb0, la0);
        
        float lr1, lg1, lb1, la1;
        c1.ToLinearRGB(lr1, lg1, lb1, la1);
        
        return Color::FromLinearRGB(lr0 + lr1, lg0 + lg1, lb0 + lb1, la0 + la1);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Color Modulate(const Color& c0, const Color& c1)
{
    return Color(c0.r * c1.r, c0.g * c1.g, c0.b * c1.b, c0.a * c1.a);
}

}
