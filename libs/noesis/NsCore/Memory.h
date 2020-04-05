////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_MEMORY_H__
#define __CORE_MEMORY_H__


#include <stdint.h>
#include <NsCore/KernelApi.h>


namespace Noesis
{

template<class T> struct Alignment
{
    enum { Mask = sizeof(T) - 1 };
    static uint32_t Align(uint32_t size) { return (size + Mask) & ~Mask; }
    static bool IsAligned(uint32_t size) { return (size & Mask) == 0; }
    static bool IsAlignedPtr(const void* ptr) { return (uintptr_t(ptr) & Mask) == 0; }
};

struct MemoryCallbacks
{
    void* user;
    void* (*alloc)(void* user, size_t size);
    void* (*realloc)(void* user, void* ptr, size_t size);
    void (*dealloc)(void* user, void* ptr);
    size_t (*allocSize)(void* user, void* ptr);
};


NS_CORE_KERNEL_API void SetMemoryCallbacks(const MemoryCallbacks& callbacks);

NS_CORE_KERNEL_API void* Alloc(size_t size);
NS_CORE_KERNEL_API void* Realloc(void* ptr, size_t size);
NS_CORE_KERNEL_API void Dealloc(void* ptr);

NS_CORE_KERNEL_API uint32_t GetAllocatedMemory();
NS_CORE_KERNEL_API uint32_t GetAllocatedMemoryAccum();
NS_CORE_KERNEL_API uint32_t GetAllocationsCount();

}

#endif
