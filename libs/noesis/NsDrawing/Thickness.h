////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __DRAWING_THICKNESS_H__
#define __DRAWING_THICKNESS_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsDrawing/TypesApi.h>
#include <NsCore/NSTLForwards.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Thickness. Describes the thickness of a frame around a rectangle.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_DRAWING_TYPES_API Thickness
{
    float left;
    float top;
    float right;
    float bottom;

    Thickness(float thickness = 0);
    Thickness(float lr, float tb);
    Thickness(float l, float t, float r, float b);
    Thickness(const Thickness& thickness);

    /// Validates this instance for the given constraints
    bool IsValid(bool allowNegative, bool allowNegativeInf, bool allowPositiveInf,
        bool allowNaN) const;

    /// Copy operator
    Thickness operator=(const Thickness& thickness);

    /// Comparisson operators
    //@{
    bool operator==(const Thickness& thickness) const;
    bool operator!=(const Thickness& thickness) const;
    //@}

    /// Creates a string representation of this thickness structure
    /// The string has the following form: "left,top,right,bottom" or "left,top" or "left"
    NsString ToString() const;

    /// Returns a hash code
    uint32_t GetHashCode() const;

    /// Tries to parse a Thickness from a string
    static bool TryParse(const char* str, Thickness& result);

private:
    static bool IsValidValue(float value, bool allowNegative, bool allowNegativeInf,
        bool allowPositiveInf, bool allowNaN);

    NS_DECLARE_REFLECTION(Thickness, NoParent)
};

}

#endif
