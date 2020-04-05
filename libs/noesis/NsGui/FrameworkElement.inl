////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
T* FrameworkElement::GetTemplateChild(const char* name) const
{
    BaseComponent* child = GetTemplateChild(name);
    NS_ASSERT(child == 0 || DynamicCast<T*>(child) != 0);
    return static_cast<T*>(child);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
T* FrameworkElement::FindName(const char* name) const
{
    BaseComponent* resource = FindName(name);
    NS_ASSERT(resource == 0 || DynamicCast<T*>(resource) != 0);
    return static_cast<T*>(resource);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
T* FrameworkElement::FindResource(IResourceKey* key) const
{
    BaseComponent* resource = FindResource(key);
    NS_ASSERT(DynamicCast<T*>(resource) != 0);
    return static_cast<T*>(resource);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
T* FrameworkElement::TryFindResource(IResourceKey* key) const
{
    BaseComponent* resource = TryFindResource(key);
    NS_ASSERT(resource == 0 || DynamicCast<T*>(resource) != 0);
    return static_cast<T*>(resource);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
T* FrameworkElement::FindResource(const char* key) const
{
    BaseComponent* resource = FindResource(key);
    NS_ASSERT(DynamicCast<T*>(resource) != 0);
    return static_cast<T*>(resource);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
T* FrameworkElement::TryFindResource(const char* key) const
{
    BaseComponent* resource = TryFindResource(key);
    NS_ASSERT(resource == 0 || DynamicCast<T*>(resource) != 0);
    return static_cast<T*>(resource);
}

}
