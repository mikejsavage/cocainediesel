////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_BASEOBJECT_H__
#define __CORE_BASEOBJECT_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/NSTLForwards.h>


namespace Noesis
{

class TypeClass;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Base class for all Noesis Engine Polymorphic objects.
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_CORE_KERNEL_API BaseObject
{
public:
    NS_DISABLE_COPY(BaseObject)

    BaseObject();
    virtual ~BaseObject() = 0;

    /// NoesisEngine object memory is managed by kernel
    //@{
    static void *operator new(size_t size);
    static void *operator new[](size_t size);
    static void operator delete(void* ptr);
    static void operator delete[](void* ptr);
    static void *operator new(size_t size, void* placementPtr);
    static void *operator new[](size_t size, void* placementPtr);
    static void operator delete(void* ptr, void* placementPtr);
    static void operator delete[](void* ptr, void* placementPtr);
    //@}

    /// Determines whether the specified object instances are considered equal
    static bool Equals(const BaseObject* left, const BaseObject* right);

    /// Gets the class type information
    virtual const TypeClass* GetClassType() const;

    /// Returns a string that represents the current object
    virtual NsString ToString() const;

    /// Determines whether the specified object is equal to the current object
    virtual bool Equals(const BaseObject* object) const;

    /// Returns a hash code for the current object
    virtual uint32_t GetHashCode() const;

    NS_DECLARE_STATIC_REFLECTION(BaseObject, NoParent)
};

#ifdef NS_TRACK_COMPONENTS
NS_CORE_KERNEL_API void TrackObjectAlloc(const char* id);
NS_CORE_KERNEL_API void TrackObjectDealloc(const char* id);
#endif

}

#endif
