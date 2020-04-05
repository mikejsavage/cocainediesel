////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_FIXEDSTRING_H__
#define __CORE_FIXEDSTRING_H__


#include <NsCore/Noesis.h>
#include <NsCore/NSTLForwards.h>
#include <EASTL/functional.h>
#include <EASTL/fixed_string.h>


namespace eastl
{

template<int N, bool EnableOverflow, typename Allocator>
struct hash<NsFixedString<N, EnableOverflow, Allocator> >
{
    size_t operator()(const NsFixedString<N, EnableOverflow, Allocator>& str) const
    {
        return eastl::string_hash<NsFixedString<N, EnableOverflow, Allocator>>()(str);
    }
};

}


#endif 
