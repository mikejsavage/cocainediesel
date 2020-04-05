////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_FILE_H__
#define __CORE_FILE_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>
#include <NsCore/NSTLForwards.h>


namespace Noesis
{

class Stream;
template<class T> class Ptr;
typedef NsFixedString<512> FixedPath;

namespace File
{

/// Normalize a pathname (case is preserved)
NS_CORE_KERNEL_API FixedPath Normalize(const char* path);

/// Extract the directory part of a pathname (A/picture.tga -> A)
NS_CORE_KERNEL_API FixedPath DirName(const char* path);

/// Extract the base portion of a pathname (A/picture.tga -> picture.tga)
NS_CORE_KERNEL_API FixedPath BaseName(const char* path);

/// Returns a stream from given filename or nullptr if not found
NS_CORE_KERNEL_API Ptr<Stream> OpenStream(const char* filename);

}
}

#endif
