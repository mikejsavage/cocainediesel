////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsCore/StringUtils.h>
#include <NsCore/UTF8.h>


#if defined(NS_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #include <windows.h>
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
    StrCopy(fullPath, sizeof(fullPath), directory);
    StrAppend(fullPath, sizeof(fullPath), "/*");
    StrAppend(fullPath, sizeof(fullPath), extension);

    uint16_t u16str[sizeof(fullPath)];
    uint32_t numChars = UTF8::UTF8To16(fullPath, u16str, sizeof(fullPath));
    NS_ASSERT(numChars <= sizeof(fullPath));

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((LPCWSTR)u16str, FindExInfoBasic, &fd, FindExSearchNameMatch, 0, 0);
    if (h != INVALID_HANDLE_VALUE)
    {
        numChars = UTF8::UTF16To8((uint16_t*)fd.cFileName, findData.filename, sizeof(fullPath));
        NS_ASSERT(numChars <= sizeof(fullPath));
        StrCopy(findData.extension, sizeof(findData.extension), extension);
        findData.handle = h;
        return true;
    }

    return false;

#else
    DIR* dir = opendir(directory);

    if (dir != 0)
    {
        StrCopy(findData.extension, sizeof(findData.extension), extension);
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

#else
    DIR* dir = (DIR*)findData.handle;

    while (true)
    {
        dirent* entry = readdir(dir);

        if (entry != 0)
        {
            if (StrCaseEndsWith(entry->d_name, findData.extension))
            {
                StrCopy(findData.filename, sizeof(findData.filename), entry->d_name);
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

#else
    DIR* dir = (DIR*)findData.handle;
    int r = closedir(dir);
    NS_ASSERT(r == 0);
#endif
}

}
