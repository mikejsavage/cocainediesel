////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #785]
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsCore/String.h>
#include <NsCore/DynamicCast.h>
#include <NsCore/TypeOfForward.h>
#include <NsCore/ValueHelper.h>
#include <NsCore/IdOf.h>
#include <NsCore/ReflectionImplementEmpty.h>
#include <NsCore/TypeId.h>


namespace Noesis
{
namespace Boxing
{
NS_CORE_KERNEL_API const Ptr<BoxedValue>& TrueBoxed();
NS_CORE_KERNEL_API const Ptr<BoxedValue>& FalseBoxed();
NS_CORE_KERNEL_API void* BoxingAllocate(size_t size);
NS_CORE_KERNEL_API void BoxingDeallocate(void* ptr, size_t size);
void Init();
void Shutdown();
void InitThread();
void ShutdownThread();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class Boxed: public BoxedValue
{
public:
    Boxed() {}
    Boxed(typename Param<T>::Type value): mValue(value) {}

    /// From BaseObject
    //@{
    NsString ToString() const override
    {
        return Noesis::ToString(mValue);
    }

    bool Equals(const BaseObject* object) const override
    {
        if (this == object)
        {
            return true;
        }

        const Boxed<T>* value = DynamicCast<const Boxed<T>*>(object);
        if (value != 0)
        {
            return Noesis::Equals(value->mValue, mValue);
        }

        return false;
    }

    uint32_t GetHashCode() const override
    {
        return Noesis::GetHashCode(mValue);
    }
    //@}

    /// From BoxedValue
    //@{
    const Type* GetValueType() const override
    {
        return TypeOf<T>();
    }

    const void* GetValuePtr() const override
    {
        return &mValue;
    }
    //@}

    static void* operator new(size_t size)
    {
        return Boxing::BoxingAllocate(size);
    }
    
    static void operator delete(void* ptr, size_t size)
    {
        Boxing::BoxingDeallocate(ptr, size);
    }

public:
    T mValue;

private:
    NS_IMPLEMENT_INLINE_REFLECTION_(Boxed, BoxedValue)
};

namespace Boxing
{

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class Boxer
{
public:
    typedef const T& UnboxType;

    static Ptr<BoxedValue> Box(typename Param<T>::Type value)
    {
        return *new Boxed<T>(value);
    }

    static bool CanUnbox(BaseComponent* object)
    {
        return object && object->GetClassType() == TypeOf<Boxed<T>>();
    }

    static UnboxType Unbox(BaseComponent* object)
    {
        Boxed<T>* boxed = static_cast<Boxed<T>*>(object);
        return boxed->mValue;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> EnableIf<IsBestByCopy<T>::Result, Ptr<BoxedValue>> Box(T value)
{
    return Boxer<T>::Box(value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> EnableIf<!IsBestByCopy<T>::Result, Ptr<BoxedValue>> Box(const T& value)
{
    return Boxer<T>::Box(value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<BoxedValue> Box(bool value)
{
    return value ? TrueBoxed() : FalseBoxed();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<BoxedValue> Box(const char* text)
{
    Ptr<Boxed<NsString>> boxed = *new Boxed<NsString>;
    boxed->mValue = text;
    return boxed;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<BoxedValue> Box(char* text)
{
    Ptr<Boxed<NsString>> boxed = *new Boxed<NsString>;
    boxed->mValue = text;
    return boxed;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> bool CanUnbox(BaseComponent* object)
{
    return Boxer<T>::CanUnbox(object);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> typename Boxer<T>::UnboxType Unbox(BaseComponent* object)
{
    return Boxer<T>::Unbox(object);
}

}
}
