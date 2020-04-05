////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_MEMORYSTREAM_H__
#define __CORE_MEMORYSTREAM_H__


#include <NsCore/Noesis.h>
#include <NsCore/Stream.h>
#include <NsCore/KernelApi.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Stream implementation using a memory buffer
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_CORE_KERNEL_API MemoryStream: public Stream
{
public:
    /// Constructor
    MemoryStream(const void *buffer, uint32_t size);

    /// From Stream
    //@{
    void SetPosition(uint32_t pos);
    uint32_t GetPosition() const;
    uint32_t GetLength() const;
    uint32_t Read(void* buffer, uint32_t size);
    //@}

private:
    const void* const mBuffer;
    const uint32_t mSize;
    uint32_t mOffset;
};

}

#endif
