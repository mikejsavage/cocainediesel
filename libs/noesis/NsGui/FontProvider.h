////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __DRAWING_FONTPROVIDER_H__
#define __DRAWING_FONTPROVIDER_H__


#include <NsCore/Noesis.h>
#include <NsCore/BaseComponent.h>
#include <NsCore/Ptr.h>
#include <NsGui/FontProperties.h>


namespace Noesis
{

class Stream;

struct FontSource
{
    // Font file (.ttf .otf .ttc)
    Ptr<Stream> file;
    
    // Index of the face in the font file, starting with value 0
    uint32_t faceIndex;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Base class for implementing providers of fonts
////////////////////////////////////////////////////////////////////////////////////////////////////
class FontProvider: public BaseComponent
{
public:
    /// Finds the font in the given folder that best matches the specified properties
    /// BaseUri is the directory where the search is performed or nullptr for system fonts
    /// Returns a null stream if no matching found
    virtual FontSource MatchFont(const char* baseUri, const char* familyName, FontWeight weight,
        FontStretch stretch, FontStyle style) = 0;

    /// Returns true if the requested font family exists in given directory
    /// BaseUri is the directory where the search is performed or nullptr for system fonts
    virtual bool FamilyExists(const char* baseUri, const char* familyName) = 0;
};

}

#endif
