////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __APP_THEMEPROVIDERS_H__
#define __APP_THEMEPROVIDERS_H__


#include <NsCore/Noesis.h>
#include <NsApp/ThemeApi.h>


namespace Noesis
{
class XamlProvider;
class FontProvider;
class TextureProvider;
}

namespace NoesisApp
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Installs providers that expose resources for Noesis theme. After this function is invoked:
/// - A default font, 'PT_Root_UI' is available in 'Normal' and 'Bold' weights
/// - The following styles are available to pass to Noesis::GUI::LoadApplicationResources:
///  + 'Theme/NoesisTheme.DarkRed.xaml'
///  + 'Theme/NoesisTheme.LightRed.xaml'
///  + 'Theme/NoesisTheme.DarkGreen.xaml'
///  + 'Theme/NoesisTheme.LightGreen.xaml'
///  + 'Theme/NoesisTheme.DarkBlue.xaml'
///  + 'Theme/NoesisTheme.LightBlue.xaml'
///  + 'Theme/NoesisTheme.DarkOrange.xaml'
///  + 'Theme/NoesisTheme.LightOrange.xaml'
///  + 'Theme/NoesisTheme.DarkEmerald.xaml'
///  + 'Theme/NoesisTheme.LightEmerald.xaml'
///  + 'Theme/NoesisTheme.DarkPurple.xaml'
///  + 'Theme/NoesisTheme.LightPurple.xaml'
///  + 'Theme/NoesisTheme.DarkCrimson.xaml'
///  + 'Theme/NoesisTheme.LightCrimson.xaml'
///  + 'Theme/NoesisTheme.DarkLime.xaml'
///  + 'Theme/NoesisTheme.LightLime.xaml'
///  + 'Theme/NoesisTheme.DarkAqua.xaml'
///  + 'Theme/NoesisTheme.LightAqua.xaml'
///
/// Remember, this function only install the providers. Call 'LoadApplicationResources' after this
////////////////////////////////////////////////////////////////////////////////////////////////////
NS_APP_THEME_API void SetThemeProviders(Noesis::XamlProvider* xamlFallback = 0,
    Noesis::FontProvider* fontFallback = 0, Noesis::TextureProvider* textureFallback = 0);

}

#endif
