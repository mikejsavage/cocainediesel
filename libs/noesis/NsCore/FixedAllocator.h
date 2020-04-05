////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_FIXEDALLOCATOR_H__
#define __CORE_FIXEDALLOCATOR_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>
#include <NsCore/Vector.h>
#include <EASTL/fixed_vector.h>


namespace Noesis
{

class FixedAllocatorTest;

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A Fixed-Size allocator. Based on "Modern C++ Design" implementation by Andrei Alexandrescu
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_CORE_KERNEL_API FixedAllocator
{
public:
    NS_DISABLE_COPY(FixedAllocator)

    FixedAllocator();
    ~FixedAllocator();

    /// Initialization
    void Initialize(uint32_t blockSize, uint32_t pageSize);

    /// Allocates memory
    void* Allocate();

    /// Frees memory
    void Deallocate(void* p);

    /// Return the size in byes of each object
    uint32_t GetBlockSize() const;

    /// Whenever a chunk is empty, FixedAllocator doesn't free it. It is mantained internally
    /// expecting new allocations in the future. Only one empty chunk is stored each time.
    /// This function free the empty chunk. Call the function to save a few bytes whenever you
    /// know, there won't be more allocations
    /// \return A boolean indicating if a trim was performed
    bool TrimEmptyChunk();

    /// This function optimize the memory usage of the internal containers. Basically is the
    /// swap trick for the internal vector of chunks
    /// \return A boolean indicating if the trim was performed
    bool TrimChunkList();

    /// Check for corruption
    /// \return A boolean indicating if the current state of the FixedAllocator is corrupted
    bool IsCorrupt() const;

private:
    friend class FixedAllocatorTest;

    struct Chunk
    {
        void Init(uint32_t blockSize, uint8_t blocks);
        void* Allocate(uint32_t blockSize);
        void Deallocate(void* p, uint32_t blockSize);
        void Reset(uint32_t blockSize, uint8_t blocks);
        void Release();
        bool IsCorrupt(uint8_t numBlocks, uint32_t blockSize, bool checkIndexes) const;
        bool IsBlockAvailable(void* p, uint8_t numBlocks, uint32_t blockSize) const;
        bool HasBlock(void* p, uint32_t chunkLength) const;
        bool HasAvailable(uint8_t numBlocks) const;
        bool IsFilled() const;

        uint8_t* data;
        uint8_t nextFree;
        uint8_t freeBlocks;
    };

    const Chunk* HasBlock(void* p) const;
    void DoDeallocate(void* p);
    void MakeNewChunk();
    Chunk* VicinityFind(void* p) const;

private:
    typedef eastl::fixed_vector<Chunk, 4, true> Chunks;
    
    static const uint8_t MinObjectsPerChunk = 8;
    static const uint8_t MaxObjectsPerChunk = 0xff;

    uint32_t mBlockSize;
    uint8_t mNumBlocks;

    Chunks mChunks;
    Chunk* mAllocChunk;
    Chunk* mDeallocChunk;
    Chunk* mEmptyChunk;
};

NS_WARNING_POP

}

#endif
