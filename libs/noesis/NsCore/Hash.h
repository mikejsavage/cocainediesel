////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_HASH_H__
#define __CORE_HASH_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// An incremental hash function implementation
///
/// Usage:
///
///     IncrementalHasher hasher;
///     hasher.Begin();
///     hasher.Add(v0);
///     hasher.Add(v1);
///     hasher.Add(v2);
///     uint32_t hash = hasher.End();
////////////////////////////////////////////////////////////////////////////////////////////////////
class IncrementalHasher
{
public:
    NS_FORCE_INLINE void Begin();

    NS_FORCE_INLINE void Add(const void* data, uint32_t numBytes);
    template<typename T> NS_FORCE_INLINE void Add(const T& value);

    NS_FORCE_INLINE uint32_t End();

private:
    uint32_t mHash;
    uint32_t mLen;
};

/// Helper function for hashing a memory buffer
NS_CORE_KERNEL_API uint32_t Hash(const void* data, uint32_t numBytes);

}

#include <NsCore/Hash.inl>

#endif
