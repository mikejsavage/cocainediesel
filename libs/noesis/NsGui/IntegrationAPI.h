
////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_INTEGRATIONAPI_H__
#define __GUI_INTEGRATIONAPI_H__


#include <NsCore/Noesis.h>
#include <NsCore/Log.h>
#include <NsCore/Error.h>
#include <NsCore/Ptr.h>
#include <NsCore/NSTLForwards.h>
#include <NsGui/CoreApi.h>
#include <NsGui/Enums.h>


namespace Noesis
{

NS_INTERFACE IView;
class BaseComponent;
class UIElement;
class FrameworkElement;
class ResourceDictionary;
class XamlProvider;
class TextureProvider;
class FontProvider;
class Stream;
struct MemoryCallbacks;

namespace GUI
{

/// Initialization passing error handler and optional logging handler and memory allocator
/// This must be the first Noesis function invoked and Shutdown the last one
/// For now, only a pair of Init() and Shutdown() is supported per application execution
/// If passed error handler is null a default one that just redirect to log is used
NS_GUI_CORE_API void Init(ErrorHandler errorHandler, LogHandler logHandler = 0,
    const MemoryCallbacks* memoryCallbacks = 0);

/// Sets the provider in charge of loading XAML resources
NS_GUI_CORE_API void SetXamlProvider(XamlProvider* provider);

/// Sets the provider in charge of loading texture resources
NS_GUI_CORE_API void SetTextureProvider(TextureProvider* provider);

/// Sets the provider in charge of loading font resources
NS_GUI_CORE_API void SetFontProvider(FontProvider* provider);

/// Sets a collection of application-scope resources, such as styles and brushes. Provides a
/// simple way to support a consistent theme across your application
NS_GUI_CORE_API void SetApplicationResources(ResourceDictionary* resources);

/// Callback invoked each time an element requests opening or closing the on-screen keyboard
typedef void (*SoftwareKeyboardCallback)(void* user, UIElement* focused, bool open);
NS_GUI_CORE_API void SetSoftwareKeyboardCallback(void* user, SoftwareKeyboardCallback callback);

/// Callback invoked each time a view needs to update the mouse cursor icon
typedef void (*UpdateCursorCallback)(void* user, IView* view, Cursor cursor);
NS_GUI_CORE_API void SetCursorCallback(void* user, UpdateCursorCallback callback);

/// Callback for opening URL in a browser
typedef void (*OpenUrlCallback)(void* user, const char* url);
NS_GUI_CORE_API void SetOpenUrlCallback(void* user, OpenUrlCallback callback);
NS_GUI_CORE_API void OpenUrl(const char* url);

/// Callback for playing audio
typedef void (*PlaySoundCallback)(void* user, const char* filename, float volume);
NS_GUI_CORE_API void SetPlaySoundCallback(void* user, PlaySoundCallback callback);
NS_GUI_CORE_API void PlaySound(const char* filename, float volume);

/// Finds dependencies to other XAMLS and resources (fonts, textures, sounds...)
typedef void (*XamlDependencyCallback)(void* user, const char* uri, XamlDependencyType type);
NS_GUI_CORE_API void GetXamlDependencies(Stream* xaml, const char* folder, void* user,
    XamlDependencyCallback callback);

/// Loads a XAML file that is located at the specified uniform resource identifier
NS_GUI_CORE_API Ptr<BaseComponent> LoadXaml(const char* filename);
template<class T> Ptr<T> LoadXaml(const char* filename);

/// Parses a well-formed XAML fragment and creates a corresponding object tree
NS_GUI_CORE_API Ptr<BaseComponent> ParseXaml(const char* xamlText);
template<class T> Ptr<T> ParseXaml(const char* xamlText);

/// Loads a XAML resource, like an audio, at the given uniform resource identifier
NS_GUI_CORE_API Ptr<Stream> LoadXamlResource(const char* filename);

/// Loads a XAML file passing an object of the same type as the root element
NS_GUI_CORE_API void LoadComponent(BaseComponent* component, const char* filename);

/// Creates a view with the given root element
NS_GUI_CORE_API Ptr<IView> CreateView(FrameworkElement* content);

/// Frees allocated resources and shutdowns. Release all noesis objects before calling this
NS_GUI_CORE_API void Shutdown();

}

}

#include <NsGui/IntegrationAPI.inl>


#endif
