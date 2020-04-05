////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_FILESTREAM_H__
#define __CORE_FILESTREAM_H__


#include <NsCore/Noesis.h>
#include <NsCore/Stream.h>

#include <stdio.h>


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Stream implementation using a FILE* handle
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_CORE_KERNEL_API FileStream: public Stream
{
public:
    FileStream(FILE* fp);
    ~FileStream();

    /// From Stream
    //@{
    void SetPosition(uint32_t pos);
    uint32_t GetPosition() const;
    uint32_t GetLength() const;
    uint32_t Read(void* buffer, uint32_t size);
    //@}

private:
    FILE* mHandle;
};

}

#endif