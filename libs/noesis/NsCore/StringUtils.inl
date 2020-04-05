////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsCore/Error.h>

#include <string.h>
#include <stdio.h>


namespace Noesis
{
namespace String
{

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsNullOrEmpty(const char* str)
{
    return str == 0 || *str == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Length(const char* str)
{
    NS_ASSERT(str != 0);
    return static_cast<uint32_t>(strlen(str));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int Compare(const char* str1, const char* str2, IgnoreCase ignoreCase)
{
    NS_ASSERT(str1 != 0);
    NS_ASSERT(str2 != 0);

    if (ignoreCase == IgnoreCase_True)
    {
#ifdef NS_PLATFORM_WINDOWS
        return  _stricmp(str1, str2);
#else
        return strcasecmp(str1, str2);
#endif
    }
    else
    {
        return strcmp(str1, str2);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Equals(const char* str1, const char* str2, IgnoreCase ignoreCase)
{
    return Compare(str1, str2, ignoreCase) == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int CompareFirst(const char* str1, const char* str2, uint32_t n, IgnoreCase ignoreCase)
{
    NS_ASSERT(str1 != 0);
    NS_ASSERT(str2 != 0);

    if (ignoreCase == IgnoreCase_True)
    {
#ifdef NS_PLATFORM_WINDOWS
        return _strnicmp(str1, str2, n);
#else
        return strncasecmp(str1, str2, n);
#endif
    }
    else
    {
        return strncmp(str1, str2, n);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool StartsWith(const char* str, const char* value, IgnoreCase ignoreCase)
{
    return CompareFirst(str, value, Length(value), ignoreCase) == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool EndsWith(const char* str, const char* value, IgnoreCase ignoreCase)
{
    NS_ASSERT(str != 0);
    NS_ASSERT(value != 0);

    uint32_t len = Length(str);
    uint32_t valueLen = Length(value);

    if (len >= valueLen)
    {
        return Compare(str + len - valueLen, value, ignoreCase) == 0;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int FindFirst(const char* str, const char* value, uint32_t offset, IgnoreCase ignoreCase)
{
    NS_ASSERT(str != 0);
    NS_ASSERT(value != 0);

    uint32_t valueLen = Length(value);

    if (valueLen == 0)
    {
        return static_cast<int>(offset);
    }

    uint32_t len = Length(str);

    if (offset + valueLen <= len)
    {
        for (uint32_t i = offset; i < len; ++i)
        {
            if (CompareFirst(str + i, value, valueLen, ignoreCase) == 0)
            {
                return static_cast<int>(i);
            }
        }
    }

    return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int FindLast(const char* str, const char* value, uint32_t offset, IgnoreCase ignoreCase)
{
    NS_ASSERT(str != 0);
    NS_ASSERT(value != 0);

    uint32_t valueLen = Length(value);

    if (valueLen == 0)
    {
        return static_cast<int>(offset);
    }

    uint32_t len = Length(str);

    if (offset + valueLen <= len)
    {
        for (uint32_t i = offset + valueLen; i <= len; ++i)
        {
            if (CompareFirst(str + len - i, value, valueLen, ignoreCase) == 0)
            {
                return static_cast<int>(len - i);
            }
        }
    }

    return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
char* Copy(char* dst, uint32_t capacity, const char* src, uint32_t count)
{
    NS_ASSERT(dst != 0);
    NS_ASSERT(src != 0);
    NS_ASSERT(capacity != 0);

    uint32_t i = 0;

    while (i < capacity - 1 && i < count && src[i] != 0)
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;

    return dst;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
char* Append(char* dst, uint32_t capacity, const char* src, uint32_t count)
{
    NS_ASSERT(dst != 0);
    NS_ASSERT(src != 0);
    NS_ASSERT(capacity != 0);

    uint32_t len = Length(dst);
    uint32_t i = 0;

    while (len + i < capacity - 1 && i < count && src[i] != 0)
    {
        dst[len + i] = src[i];
        i++;
    }

    dst[len + i] = 0;

    return dst;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
char* Replace(char* str, char oldValue, char newValue)
{
    NS_ASSERT(str != 0);

    while (*str != 0)
    {
        if (*str == oldValue)
        {
            *str = newValue;
        }

        ++str;
    }

    return str;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Hash(const char* str)
{
    char c;
    uint32_t result = 2166136261U;

    while ((c = *str++) != 0)
    {
        result = (result * 16777619) ^ c;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t CaseHash(const char* str)
{
    char c;
    uint32_t result = 2166136261U;

    while ((c = *str++) != 0)
    {
        result = (result * 16777619) ^ (c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c);
    }

    return result;
}

}
}
