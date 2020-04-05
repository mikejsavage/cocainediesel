////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_VALUEHELPER_H__
#define __CORE_VALUEHELPER_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>
#include <NsCore/String.h>


namespace Noesis
{

/// Returns a string that represents the passed value
template<class T> NsString ToString(const T& value);

/// Returns a hash code for the passed value
template<class T> uint32_t GetHashCode(const T& value);

/// Returns true if both values are equal. We require a specialization for float/double because NaN
/// values cannot be directly compared with fast float compiler setting enabled
template<class T> bool Equals(const T& left, const T& right);
inline bool Equals(float left, float right);
inline bool Equals(double left, double right);

}

// Inline include
#include <NsCore/ValueHelper.inl>


#endif
