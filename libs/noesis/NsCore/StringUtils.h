////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_STRINGUTILS_H__
#define __CORE_STRINGUTILS_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>

#include <stdarg.h>


namespace Noesis
{

enum IgnoreCase
{
    IgnoreCase_False = 0,
    IgnoreCase_True = 1
};

namespace String
{

/// Indicates whether the specified string is null or an empty string
inline bool IsNullOrEmpty(const char* str);

/// Returns the length of the C string str
inline uint32_t Length(const char* str);

/// Compare two strings and returns their relative position in the sort order
inline int Compare(const char* str1, const char* str2, IgnoreCase ignoreCase = IgnoreCase_False);

/// Determines whether two strings have the same value
inline bool Equals(const char* str1, const char* str2, IgnoreCase ignoreCase = IgnoreCase_False);

/// Compare two strings up to n characters and returns their relative position in the sort order
inline int CompareFirst(const char* str1, const char* str2, uint32_t n, IgnoreCase ignoreCase =
    IgnoreCase_False);

/// Determines whether the beginning of a string matches the specified string
inline bool StartsWith(const char* str, const char* value, IgnoreCase ignoreCase =
    IgnoreCase_False);

/// Determines whether the end of a string matches the specified string
inline bool EndsWith(const char* str, const char* value, IgnoreCase ignoreCase = IgnoreCase_False);

/// Reports the index of the first occurrence of the specified string
inline int FindFirst(const char* str, const char* value, uint32_t offset = 0,
    IgnoreCase ignoreCase = IgnoreCase_False);

/// Reports the index position of the last occurrence of a specified string
inline int FindLast(const char* str, const char* value, uint32_t offset = 0,
    IgnoreCase ignoreCase = IgnoreCase_False);

/// Copy characters of one string to another
inline char* Copy(char* dst, uint32_t capacity, const char* src, uint32_t count = 0xffff);

/// Appends characters of a string
inline char* Append(char* dst, uint32_t capacity, const char* src, uint32_t count = 0xffff);

/// Replaces all occurrences of a specified characeter
inline char* Replace(char* str, char oldValue, char newValue);

/// Calculates the hash value of a string
inline uint32_t Hash(const char* str);

/// Calculates the case-insensitive hash value of a string
inline uint32_t CaseHash(const char* str);

/// Parses the string, interpreting its content as a float and returns its value
NS_CORE_KERNEL_API float ToFloat(const char* str, uint32_t* charParsed);

/// Parses the string, interpreting its content as a double and returns its value
NS_CORE_KERNEL_API double ToDouble(const char* str, uint32_t* charParsed);

/// Parses the string, interpreting its content as an integer and returns its value
NS_CORE_KERNEL_API int ToInteger(const char* str, uint32_t* charParsed);

}
}

/// Inline Include
#include <NsCore/StringUtils.inl>

#endif
