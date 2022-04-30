#pragma once

#include "qcommon/types.h"
#include "gameshared/q_math.h"

// TODO: timsort for StableSort?
// TODO: heapsort for Introsort recursion limit

template< typename T >
bool DefaultLessThan( const T & a, const T & b ) {
	return a < b;
}

// TODO: cheat and use memmove lol
template< typename T, typename F >
void InsertionSort( T * begin, T * end, F less_than ) {
	if( begin == end )
		return;

	T * cursor = begin + 1;
	while( cursor < end ) {
		T elem = *cursor;

		T * i = cursor;
		while( i != begin ) {
			if( !less_than( elem, *( i - 1 ) ) )
				break;
			*i = *( i - 1 );
			i--;
		}

		*i = elem;
		cursor++;
	}
}

template< typename T, typename F >
T IntrosortMedian( const T & a, const T & b, const T & c, F less_than ) {
	if( less_than( a, b ) ) {
		if( less_than( b, c ) ) return b;
		if( less_than( a, c ) ) return c;
		return a;
	}

	if( less_than( a, c ) ) return a;
	if( less_than( b, c ) ) return c;
	return b;
}

template< typename T, typename F >
T * IntrosortPartition( T * begin, T * end, F less_than ) {
	T pivot = IntrosortMedian( *begin, *( end - 1 ), *( begin + ( end - begin ) / 2 ), less_than );

	end--;

	while( true ) {
		while( less_than( *begin, pivot ) ) begin++;
		while( less_than( pivot, *end ) ) end--;
		if( begin >= end )
			break;

		Swap2( begin, end );
		begin++;
		end--;
	}

	return begin;
}

template< typename T, typename F >
void Introsort( T * begin, T * end, F less_than, u32 recursion_depth ) {
	constexpr size_t use_insertion_sort_below = 28;

	size_t n = end - begin;
	if( n <= 1 )
		return;

	if( n <= use_insertion_sort_below ) {
		InsertionSort( begin, end, less_than );
	}
	// else if( recursion_depth == 0 ) {
	// }
	else {
		T * pivot = IntrosortPartition( begin, end, less_than );
		Introsort( begin, pivot, less_than, recursion_depth - 1 );
		Introsort( pivot, end, less_than, recursion_depth - 1 );
	}
}

template< typename T, typename F >
void MergeSortMerge( T * begin, T * mid, T * end, T * scratch, F less_than ) {
	T * left_cursor = begin;
	T * right_cursor = mid;
	T * scratch_cursor = scratch;

	while( left_cursor < mid && right_cursor < end ) {
		if( less_than( *left_cursor, *right_cursor ) ) {
			*scratch_cursor = *left_cursor;
			left_cursor++;
		}
		else {
			*scratch_cursor = *right_cursor;
			right_cursor++;
		}
		scratch_cursor++;
	}

	while( left_cursor < mid ) {
		*scratch_cursor = *left_cursor;
		left_cursor++;
		scratch_cursor++;
	}

	while( right_cursor < mid ) {
		*scratch_cursor = *right_cursor;
		right_cursor++;
		scratch_cursor++;
	}

	memcpy( begin, scratch, ( scratch_cursor - scratch ) * sizeof( T ) );
}

template< typename T, typename F >
void MergeSort( T * begin, T * end, T * scratch, F less_than ) {
	if( ( end - begin ) <= 1 )
		return;

	T * mid = begin + ( end - begin ) / 2;
	MergeSort( begin, mid, scratch, less_than );
	MergeSort( mid, end, scratch, less_than );
	MergeSortMerge( begin, mid, end, scratch, less_than );
}

template< typename T, typename F = decltype( DefaultLessThan< T > ) >
void Sort( T * begin, T * end, F less_than = DefaultLessThan< T > ) {
	u32 recursion_limit = 2 * Log2( end - begin );
	Introsort( begin, end, less_than, recursion_limit );
}

template< typename T, typename F = decltype( DefaultLessThan< T > ) >
void StableSort( Allocator * a, T * begin, T * end, F less_than = DefaultLessThan< T > ) {
	T * scratch = ALLOC_MANY( a, T, end - begin );
	MergeSort( begin, end, scratch, less_than );
}
