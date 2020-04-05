////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsCore/StringUtils.h>
#include <NsCore/UTF8.h>


#if defined(NS_PLATFORM_WINDOWS)
    #include <windows.h>
// <ps4>
#elif defined(NS_PLATFORM_PS4)
    #include <kernel.h>
    #include <sceerror.h>

    struct Dir
    {
        int fd;
        int blk_size;
        char *buf;
        int bytes;
        int pos;
    };
// </ps4>
#else
    #include <sys/stat.h>
    #include <dirent.h>
    #include <errno.h>
#endif


namespace Noesis
{
////////////////////////////////////////////////////////////////////////////////////////////////////
bool FindFirst(const char* directory, const char* extension, FindData& findData)
{
#if defined(NS_PLATFORM_WINDOWS)
    char fullPath[sizeof(findData.filename)];
    String::Copy(fullPath, sizeof(fullPath), directory);
    String::Append(fullPath, sizeof(fullPath), "/*");
    String::Append(fullPath, sizeof(fullPath), extension);

    uint16_t u16str[sizeof(fullPath)];
    uint32_t numChars = UTF8::UTF8To16(fullPath, u16str, sizeof(fullPath));
    NS_ASSERT(numChars <= sizeof(fullPath));

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((LPCWSTR)u16str, FindExInfoBasic, &fd, FindExSearchNameMatch, 0, 0);
    if (h != INVALID_HANDLE_VALUE)
    {
        numChars = UTF8::UTF16To8((uint16_t*)fd.cFileName, findData.filename, sizeof(fullPath));
        NS_ASSERT(numChars <= sizeof(fullPath));
        String::Copy(findData.extension, sizeof(findData.extension), extension);
        findData.handle = h;
        return true;
    }

    return false;

// <ps4>
#elif defined(NS_PLATFORM_PS4)
    SceKernelMode mode = SCE_KERNEL_S_IRU | SCE_KERNEL_S_IFDIR;
    int fd = sceKernelOpen(directory, SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, mode);
    if (fd >= 0)
    {
        SceKernelStat sb = {0};
        int status = sceKernelFstat(fd, &sb);
        NS_ASSERT(status == SCE_OK);

        Dir* dir = (Dir*)Alloc(sizeof(Dir));
        dir->fd = fd;
        dir->blk_size = sb.st_blksize;
        dir->buf = (char*)Alloc(sb.st_blksize);
        dir->bytes = 0;
        dir->pos = 0;

        String::Copy(findData.extension, sizeof(findData.extension), extension);
        findData.handle = dir;

        if (FindNext(findData))
        {
            return true;
        }
        else
        {
            FindClose(findData);
            return false;
        }
    }

    return false;
// </ps4>

#else
    DIR* dir = opendir(directory);

    if (dir != 0)
    {
        String::Copy(findData.extension, sizeof(findData.extension), extension);
        findData.handle = dir;

        if (FindNext(findData))
        {
            return true;
        }
        else
        {
            FindClose(findData);
            return false;
        }
    }

    return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool FindNext(FindData& findData)
{
#if defined(NS_PLATFORM_WINDOWS)
    WIN32_FIND_DATAW fd;
    BOOL res = FindNextFileW(findData.handle, &fd);
    
    if (res)
    {
        const int MaxFilename = sizeof(findData.filename);
        uint32_t n = UTF8::UTF16To8((uint16_t*)fd.cFileName, findData.filename, MaxFilename);
        NS_ASSERT(n <= MaxFilename);
        return true;
    }

    return false;

// <ps4>
#elif defined(NS_PLATFORM_PS4)
    Dir* dir  = (Dir*)findData.handle;

    while (true)
    {
        if (dir->bytes == 0)
        {
            dir->pos = 0;
            dir->bytes = sceKernelGetdents(dir->fd, dir->buf, dir->blk_size);
            if (dir->bytes == 0)
            {
                return false;
            }
        }

        while (dir->pos < dir->bytes)
        {
            SceKernelDirent *dirent = (SceKernelDirent *)&(dir->buf[dir->pos]);
            dir->pos += dirent->d_reclen;

            if (dirent->d_fileno != 0)
            {
                if (String::EndsWith(dirent->d_name, findData.extension, IgnoreCase_True))
                {
                    String::Copy(findData.filename, sizeof(findData.filename), dirent->d_name);
                    return true;
                }
            }
        }

        dir->bytes = 0;
    }

    return false;
// </ps4>

#else
    DIR* dir = (DIR*)findData.handle;

    while (true)
    {
        dirent* entry = readdir(dir);

        if (entry != 0)
        {
            if (String::EndsWith(entry->d_name, findData.extension, IgnoreCase_True))
            {
                String::Copy(findData.filename, sizeof(findData.filename), entry->d_name);
                return true;
            }
        }
        else
        {
            return false;
        }
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void FindClose(FindData& findData)
{
#if defined(NS_PLATFORM_WINDOWS)
    BOOL r = ::FindClose(findData.handle);
    NS_ASSERT(r != 0);

// <ps4>
#elif defined(NS_PLATFORM_PS4)
    Dir* dir  = (Dir*)findData.handle;
    int error = sceKernelClose(dir->fd);
    NS_ASSERT(error == SCE_OK);
    Dealloc(dir->buf);
    Dealloc(dir);
// </ps4>

#else
    DIR* dir = (DIR*)findData.handle;
    int r = closedir(dir);
    NS_ASSERT(r == 0);
#endif
}

}
