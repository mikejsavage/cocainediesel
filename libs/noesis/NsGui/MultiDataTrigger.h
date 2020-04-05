////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __GUI_MULTIDATATRIGGER_H__
#define __GUI_MULTIDATATRIGGER_H__


#include <NsCore/Noesis.h>
#include <NsCore/Set.h>
#include <NsCore/FixedVector.h>
#include <NsGui/CoreApi.h>
#include <NsGui/BaseTrigger.h>
#include <NsGui/BindingListener.h>


namespace Noesis
{

class BaseSetter;
class Condition;

template<class T> class UICollection;
typedef Noesis::UICollection<Noesis::BaseSetter> BaseSetterCollection;
typedef Noesis::UICollection<Noesis::Condition> ConditionCollection;

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Represents a trigger that applies property values or performs actions when the bound data meets
/// a set of conditions.
///
/// A MultiDataTrigger object is similar to a MultiTrigger, except that the conditions of a
/// MultiDataTrigger are based on property values of bound data instead of those of a UIElement.
/// In a MultiDataTrigger, a condition is met when the property value of the data item matches the
/// specified Value. You can then use setters or the *EnterActions* and *ExitActions* properties to
/// apply changes or start actions when all of the conditions are met.
///
/// http://msdn.microsoft.com/en-us/library/system.windows.multidatatrigger.aspx
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_GUI_CORE_API MultiDataTrigger: public BaseTrigger
{
public:
    MultiDataTrigger();
    ~MultiDataTrigger();

    /// Gets a collection of Condition objects. Changes to property values are applied when all of
    /// the conditions in the collection are met.
    ConditionCollection* GetConditions() const;

    /// Gets a collection of Setter objects, which describe the property values to apply when all
    /// of the conditions of the MultiTrigger are met.
    BaseSetterCollection* GetSetters() const;

    /// From BaseTrigger
    //@{
    void RegisterBindings(FrameworkElement* target, FrameworkElement* nameScope,
        bool skipTargetName, uint8_t priority) final;
    void UnregisterBindings(FrameworkElement* target) final;
    BaseComponent* FindValue(FrameworkElement* target, FrameworkElement* nameScope,
        DependencyObject* object, const DependencyProperty* dp, bool skipSourceName,
        bool skipTargetName) final;
    void Invalidate(FrameworkElement* target, FrameworkElement* nameScope, bool skipSourceName,
        bool skipTargetName, uint8_t priority) final;
    void Seal() override;
    //@}

    ConditionCollection* InternalGetConditions() const; // can return null
    BaseSetterCollection* InternalGetSetters() const; // can return null

private:
    void EnsureConditions() const;
    void EnsureSetters() const;

    void CheckConditions(ConditionCollection* conditions) const;

    void ForceInvalidate(FrameworkElement* target, FrameworkElement* nameScope,
        bool skipTargetName, bool fireEnterActions, uint8_t priority);

    bool Matches(FrameworkElement* target) const;

private:
    mutable Ptr<ConditionCollection> mConditions;
    mutable Ptr<BaseSetterCollection> mSetters;

    class Listener
    {
    public:
        Listener(MultiDataTrigger* dt, FrameworkElement* t, FrameworkElement* ns, bool sk,
            uint8_t p);

        struct Comparer;
        bool operator<(const Listener& other) const;
        bool operator==(const Listener& other) const;

        void Register();
        void Unregister();

        bool Matches() const;

    private:
        MultiDataTrigger* trigger;
        BindingListenerData data;

        class ConditionListener: public BindingListener
        {
        public:
            ConditionListener(Listener* listener, Condition* c);

        protected:
            const BindingListenerData* GetData() const;
            BaseComponent* GetValue() const;
            BaseBinding* GetBinding() const;
            void Invalidate(FrameworkElement* target, FrameworkElement* nameScope,
                bool skipTargetName, bool fireEnterActions, uint8_t priority) const;

        private:
            Listener* listener;
            Condition* condition;
        };

        NsFixedVector<ConditionListener, 4> conditions;
    };

    typedef NsSet<Listener> Listeners;
    Listeners mListeners;

    NS_DECLARE_REFLECTION(MultiDataTrigger, BaseTrigger)
};

NS_WARNING_POP

}


#endif
