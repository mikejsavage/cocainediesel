////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_BINDINGLISTENER_H__
#define __GUI_BINDINGLISTENER_H__


#include <NsCore/Noesis.h>
#include <NsCore/BaseComponent.h>
#include <NsCore/Ptr.h>
#include <NsCore/Delegate.h>
#include <NsCore/Vector.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsGui/CoreApi.h>


namespace Noesis
{

class TypeProperty;
class Type;
class DependencyObject;
class DependencyProperty;
class FrameworkElement;
class CollectionView;
class BaseBinding;
NS_INTERFACE IResourceKey;
struct PathElement;
struct DependencyPropertyChangedEventArgs;
struct AncestorNameScopeChangedArgs;
struct PropertyChangedEventArgs;
struct NotifyCollectionChangedEventArgs;
struct NotifyDictionaryChangedEventArgs;
struct EventArgs;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// BindingListener shared data
////////////////////////////////////////////////////////////////////////////////////////////////////
struct BindingListenerData
{
    FrameworkElement* target;
    FrameworkElement* nameScope;
    bool skipTargetName;
    uint8_t priority;

    BindingListenerData(FrameworkElement* t, FrameworkElement* ns, bool sk, uint8_t p);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Used by DataTriggers and MultiDataTriggers to listen to Binding changes
////////////////////////////////////////////////////////////////////////////////////////////////////
class BindingListener
{
public:
    BindingListener();
    virtual ~BindingListener() = 0;

    /// Tries to resolve binding and subscribes to binding changes
    void Register();

    /// Unsubscribes from binding changes
    void Unregister();

    /// Indicates if current binding value matches the trigger condition
    bool Matches() const;

protected:
    virtual const BindingListenerData* GetData() const = 0;
    virtual BaseComponent* GetValue() const = 0;
    virtual BaseBinding* GetBinding() const = 0;
    virtual void Invalidate(FrameworkElement* target, FrameworkElement* nameScope,
        bool skipTargetName, bool fireEnterActions, uint8_t priority) const = 0;

private:
    void Initialize();
    void Shutdown();

    void AddPathElement(const PathElement& element, void* context);

    struct WeakPathElement;
    void RegisterNotification(const WeakPathElement& element);
    void UnregisterNotification(const WeakPathElement& element);

    void SetPathUnresolved();
    bool IsPathResolved() const;

    bool TryConvertTriggerValue();

    bool MatchesOnReset();
    void Matches(bool& matchesOldValue, bool& matchesNewValue, bool reevaluate,
        const Ptr<BaseComponent>& newValue = 0);

    Ptr<BaseComponent> GetValue(const WeakPathElement& element) const;
    Ptr<BaseComponent> GetValue(const WeakPathElement& element,
        const Type*& valueType) const;
    Ptr<BaseComponent> GetSourceValue(const WeakPathElement& element,
        const Type*& valueType) const;
    Ptr<BaseComponent> GetConvertedValue(const Ptr<BaseComponent>& value) const;
    bool UpdateSourceValue(const Ptr<BaseComponent>& newValue, const Type* type);

    void OnAncestorChanged(FrameworkElement* ancestor);
    void OnNameScopeChanged(FrameworkElement* sender,
        const AncestorNameScopeChangedArgs& args);
    void OnTargetContextChanged(BaseComponent* sender,
        const DependencyPropertyChangedEventArgs& args);

    void InvalidateSource();
    void Invalidate(bool matchesOldValue, bool matchesNewValue);

    void OnObjectPropertyChanged(BaseComponent* sender,
        const DependencyPropertyChangedEventArgs& args);
    void OnNotifyPropertyChanged(BaseComponent* sender,
        const PropertyChangedEventArgs& args);

    void OnCollectionChanged(BaseComponent* sender,
        const NotifyCollectionChangedEventArgs& args);
    void OnCollectionReset(BaseComponent* sender,
        const NotifyCollectionChangedEventArgs& args);

    void OnDictionaryChanged(BaseComponent* sender,
        const NotifyDictionaryChangedEventArgs& args);
    void OnDictionaryReset(BaseComponent* sender,
        const NotifyDictionaryChangedEventArgs& args);

    void OnCurrentChanged(BaseComponent* sender, const EventArgs& args);

    void OnTargetDestroyed(DependencyObject* sender);
    void OnSourceDestroyed(DependencyObject* sender);

private:
    BaseComponent* mSource;

    struct WeakPathElement
    {
        BaseComponent* source;
        CollectionView* collection;
        const TypeProperty* property;
        const DependencyProperty* dp;
        int index;
        Ptr<IResourceKey> key;
    };

    // Resolved path elements
    NsVector<WeakPathElement> mPathElements;

    // Last source value
    Ptr<BaseComponent> mSourceValue;
    const Type* mSourceType;

    // Trigger Value converted to the type of the last path property or object
    Ptr<BaseComponent> mTriggerValue;
};

}


#endif
