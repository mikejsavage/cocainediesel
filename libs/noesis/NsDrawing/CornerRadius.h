////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __DRAWING_CORNERRADIUS_H__
#define __DRAWING_CORNERRADIUS_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsDrawing/TypesApi.h>
#include <NsCore/String.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// CornerRadius. Represents the radii of rectangle corners.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct NS_DRAWING_TYPES_API CornerRadius
{
    float topLeft;
    float topRight;
    float bottomRight;
    float bottomLeft;

    CornerRadius(float radius = 0.0f);
    CornerRadius(float tl, float tr, float br, float bl);
    CornerRadius(const CornerRadius& cornerRadius);

    /// Validates this instance for the given constraints
    bool IsValid(bool allowNegative, bool allowNegativeInf, bool allowPositiveInf,
        bool allowNaN) const;

    /// Copy operator
    CornerRadius operator=(const CornerRadius& cornerRadius);

    /// Comparisson operators
    //@{
    bool operator==(const CornerRadius& cornerRadius) const;
    bool operator!=(const CornerRadius& cornerRadius) const;
    //@}

    /// Creates a string representation of this CornerRadius structure
    /// The string has the following form: "topLeft,topRight,bottomRight,bottomLeft" or "radius"
    NsString ToString() const;

    /// Returns a hash code
    uint32_t GetHashCode() const;

    /// Tries to parse a CornerRadius from a string
    static bool TryParse(const char* str, CornerRadius& radius);

private:
    static bool IsValidValue(float value, bool allowNegative, bool allowNegativeInf,
        bool allowPositiveInf, bool allowNaN);

    NS_DECLARE_REFLECTION(CornerRadius, NoParent)
};

}

#endif
