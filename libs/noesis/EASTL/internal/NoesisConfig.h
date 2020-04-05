////////////////////////////////////////////////////////////////////////////////////////////////////
// Noesis Engine - http://www.noesisengine.com
// Copyright (c) 2009-2010 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __NOESISCONFIG_H__
#define __NOESISCONFIG_H__


#include <NsCore/Noesis.h>
#include <NsCore/Error.h>
#include <NsCore/Memory.h>


#ifndef EASTL_API
    #if defined(NS_CORE_KERNEL_PRIVATE)
        #define EASTL_API
    #elif defined(NS_CORE_KERNEL_EXPORTS)
        #define EASTL_API NS_DLL_EXPORT
    #else
        #define EASTL_API NS_DLL_IMPORT
    #endif
#endif

#define EASTL_ASSERT(expr) NS_ASSERT(expr)
#define EASTL_FAIL_MSG(message) NS_ASSERT(false)
#define EASTL_NAME_ENABLED 0
#define EASTL_EXCEPTIONS_ENABLED 0
#define EASTL_RTTI_ENABLED 0
#define EASTL_DEBUGPARAMS_LEVEL 0
#define EASTL_USER_DEFINED_ALLOCATOR
#define EASTL_SIZE_T unsigned int
#define EASTL_SSIZE_T signed int
#define EASTL_MINMAX_ENABLED 0
#define EASTL_STD_ITERATOR_CATEGORY_ENABLED 0

namespace eastl
{

////////////////////////////////////////////////////////////////////////////////////////////////////
class NsAllocator
{
public:
    explicit NsAllocator(const char* = 0) {}
    NsAllocator(const NsAllocator&, const char*) {}
    void* allocate(size_t n, int = 0) { return Noesis::Alloc(n); }
    void* allocate(size_t n, size_t, size_t, int = 0) { return Noesis::Alloc(n); }
    void deallocate(void* p, size_t) { Noesis::Dealloc(p); }
    const char* get_name() const { return ""; }
    void set_name(const char*) {}
    bool operator==(const NsAllocator&) { return true; }
    bool operator!=(const NsAllocator&) { return false; }
};

}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_CORE_KERNEL_API eastl::NsAllocator* EASTLDefaultAllocator();

#define EASTLAllocatorType eastl::NsAllocator
#define EASTLAllocatorDefault EASTLDefaultAllocator


#endif
