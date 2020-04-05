////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_CACHEDFONTPROVIDER_H__
#define __GUI_CACHEDFONTPROVIDER_H__


#include <NsCore/Noesis.h>
#include <NsCore/Vector.h>
#include <NsCore/Map.h>
#include <NsGui/FontProvider.h>
#include <NsGui/CachedFontProviderApi.h>

#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>


namespace Noesis
{

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper base class for implementing font providers
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CACHEDFONTPROVIDER_API CachedFontProvider: public FontProvider
{
public:
    CachedFontProvider();

    /// Disables or enables operating systems fonts. By default they are enabled
    void SetUseSystemFonts(bool value);

protected:
    /// Registers a font filename in the given folder. Each time this function is invoked, the
    /// given filename is opened and scanned (through OpenFont). It is recommened deferring
    /// this call as much as possible (for example, until ScanFolder is invoked)
    void RegisterFont(const char* folder, const char* filename);

    /// First time a font is requested from a folder, this function is invoked to give inheritors
    /// the opportunity to register faces found in that folder
    virtual void ScanFolder(const char* folder);

    /// Returns a stream to a previously registered filename
    virtual Ptr<Stream> OpenFont(const char* folder, const char* filename) const = 0;

    /// From FontProvider
    //@{
    FontSource MatchFont(const char* baseUri, const char* familyName, FontWeight weight,
        FontStretch stretch, FontStyle style) override;
    bool FamilyExists(const char* baseUri, const char* familyName) override;
    //@}

private:
    void RegisterFace(const char* folder, const char* filename, uint32_t index, const char* family,
        FontWeight weight, FontStretch stretch, FontStyle style);

private:
    typedef eastl::fixed_string<char, 128> Filename;

    struct Face
    {
        Filename filename;
        uint32_t faceIndex;

        FontWeight weight;
        FontStretch stretch;
        FontStyle style;
    };

    struct ILess
    {
        bool operator()(const Filename& a, const Filename& b) const
        {
            return a.comparei(b) < 0;
        }
    };

    typedef NsVector<Face> Family;
    typedef NsMap<Filename, Family, ILess> Families;
    typedef NsMap<Filename, Families, ILess> Folders;
    Folders mFolders;

    bool mUseSystemFonts;

    Folders::iterator GetFolder(const char* folder);
    Ptr<Stream> InternalOpenFont(const char* folder, const char* filename) const;
    void ScanSystemFonts(const char* root = "");

    FontSource FindBestMatch(const char* baseUri, const Family& faces, FontWeight weight,
        FontStretch stretch, FontStyle style) const;
};

NS_WARNING_POP

}

#endif
