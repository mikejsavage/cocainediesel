////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __MATH_UTILS_H__
#define __MATH_UTILS_H__


#include <NsCore/Noesis.h>

#include <float.h>


namespace Noesis
{

/// Tests if a number is a power of 2
inline bool IsPow2(uint32_t x);

/// Gets nearest power of 2
inline uint32_t NearestPow2(uint32_t x);

/// Gets next power of 2
inline uint32_t NextPow2(uint32_t x);

/// Gets previous power of 2
inline uint32_t PrevPow2(uint32_t x);

/// Returns whether the specified value is close to 1 within the order of epsilon
inline bool IsOne(float val, float epsilon = 10.0f * FLT_EPSILON);

/// Returns whether the specified value is close to 0 within the order of epsilon 
inline bool IsZero(float val, float epsilon = 10.0f * FLT_EPSILON);

/// Returns whether or not two floats are within epsilon of each other
inline bool AreClose(float a, float b, float epsilon = 10.0f * FLT_EPSILON);

/// Returns whether or not the first number is greater than the second number
inline bool GreaterThan(float a, float b, float epsilon = 10.0f * FLT_EPSILON);

/// Returns whether or not the first number is greater than or close to the second number
inline bool GreaterThanOrClose(float a, float b, float epsilon = 10.0f * FLT_EPSILON);

/// Returns whether or not the first number is less than the second number
inline bool LessThan(float a, float b, float epsilon = 10.0f * FLT_EPSILON);

/// Returns whether or not the first number is less than or close to the second number
inline bool LessThanOrClose(float a, float b, float epsilon = 10.0f * FLT_EPSILON);

/// Tests if float is NaN
inline bool IsNaN(float val);

/// Tests if double is NaN
inline bool IsNaN(double val);

/// Tests if float is positive infinity or negative infinity
inline bool IsInfinity(float val);

/// Tests if double is positive infinity or negative infinity
inline bool IsInfinity(double val);

/// Tests if float is positive infinity
inline bool IsPositiveInfinity(float val);

/// Tests if double is positive infinity
inline bool IsPositiveInfinity(double val);

/// Tests if float is negative infinity
inline bool IsNegativeInfinity(float val);

/// Tests if double is negative infinity
inline bool IsNegativeInfinity(double val);

/// Float to Signed Int conversion with truncation towards zero
inline int Trunc(float val);

/// Float to Signed Int conversion rounding to nearest integer
inline int Round(float val);

/// Calculates the floor of a value
inline float Floor(float val);

/// Calculates the ceil of a value
inline float Ceil(float val);

/// Max returns the greater of its two arguments
template<class T> const T& Max(const T& a, const T& b);

/// Min returns the lesser of its two arguments
template<class T> const T& Min(const T& a, const T& b);

/// Clips a value between a minimum and a maximum
template<class T> T Clip(T val, T min, T max);

/// Returns a linear interpolation between x and y. 0 <= t <= 1
inline float Lerp(float x, float y, float t);

}

/// Inline Include
#include <NsMath/Utils.inl>

#endif
