////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_NSTLPOOLALLOCATOR_H__
#define __CORE_NSTLPOOLALLOCATOR_H__


#include <NsCore/Noesis.h>
#include <NsCore/FixedAllocator.h>


namespace eastl
{
////////////////////////////////////////////////////////////////////////////////////////////////////
/// nstl allocator implemented using a fixed allocator.
///  
/// Example:
///
///     typedef NsSet<int32_t, eastl::less<int32_t>, eastl::PoolAllocator> MySet;
///     MySet set;
///     set.get_allocator().Init(sizeof(MySet::node_type), 128);
///
////////////////////////////////////////////////////////////////////////////////////////////////////
class PoolAllocator
{
public:
    explicit inline PoolAllocator(const char* name = "");
    inline PoolAllocator(const PoolAllocator& allocator);

    inline PoolAllocator& operator=(const PoolAllocator& allocator);
    inline void Init(uint32_t nodeSize, uint32_t nodeCount);

    inline void* allocate(size_t n, int flags = 0);
    inline void* allocate(size_t n, size_t alignment, size_t offset, int flags = 0);
    inline void deallocate(void* p, size_t n);

private:
    Noesis::FixedAllocator mFixedAllocator;
};

inline bool operator==(const PoolAllocator& a, const PoolAllocator& b);
inline bool operator!=(const PoolAllocator& a, const PoolAllocator& b);

}

#include "NSTLPoolAllocator.inl"

#endif
