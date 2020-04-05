////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_NSTLFORWARDS_H__
#define __CORE_NSTLFORWARDS_H__


////////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations for all nstl types
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace eastl
{

template<class T> struct deque_helper
{
    enum
    {
        MaxBufferSize = 256,
        DefaultSize = (MaxBufferSize / sizeof(T) > 0) ? (MaxBufferSize / sizeof(T)): 1
    };
};

class NsAllocator;
template<class T> struct hash;
template<class T> struct equal_to;
template<class T> struct less;
template<class T1, class T2> struct pair;
template <size_t N> class bitset;
template<typename T, typename Allocator> class vector;
template<typename T, size_t nodeCount, bool bEnableOverflow, typename Allocator> class fixed_vector;
template<typename T, typename Allocator> class list;
template <typename T, typename Allocator> class basic_string;
template<typename T, int nodeCount, bool bEnableOverflow, typename Allocator> class fixed_string;
template<typename Key, typename T, typename Hash, typename Predicate, typename Allocator,
    bool bCacheHashCode> class hash_map;
template<typename Key, typename T, typename Hash, typename Predicate, typename Allocator,
    bool bCacheHashCode> class hash_multimap;
template<typename Value, typename Hash, typename Predicate, typename Allocator,
    bool bCacheHashCode> class hash_set;
template<typename Value, typename Hash, typename Predicate, typename Allocator,
    bool bCacheHashCode> class hash_multiset;
template<typename Key, typename T, typename Compare, typename Allocator> class map;
template<typename Key, typename T, typename Compare, typename Allocator> class multimap;
template<typename Key, typename Compare, typename Allocator> class set;
template<typename Key, typename Compare, typename Allocator> class multiset;

}

// NsBitSet
template<size_t N> using NsBitset = eastl::bitset<N>;

// NsVector
template<class T, class Allocator = eastl::NsAllocator> using NsVector = eastl::vector<T, Allocator>;

// NsFixedVector
template<class T, size_t nodeCount, bool bEnableOverflow = true,
    typename Allocator = eastl::NsAllocator>
using NsFixedVector = eastl::fixed_vector<T, nodeCount, bEnableOverflow, Allocator>;

// NsList
template<class T, class Allocator = eastl::NsAllocator> using NsList = eastl::list<T, Allocator>;

// NsString
typedef eastl::basic_string<char, eastl::NsAllocator> NsString;

// NsFixedString
template<int N, bool EnableOverflow = true, typename Allocator = eastl::NsAllocator>
using NsFixedString = eastl::fixed_string<char, N, EnableOverflow, Allocator>;

// NsHashMap
template<class Key, class T, class Hash = eastl::hash<Key>,
    typename Predicate = eastl::equal_to<Key>, class Allocator = eastl::NsAllocator,
    bool bCacheHashCode = false>
using NsHashMap = eastl::hash_map<Key, T, Hash, Predicate, Allocator, bCacheHashCode>;

// NsHashMultimap
template<class Key, class T, class Hash = eastl::hash<Key>,
    typename Predicate = eastl::equal_to<Key>, class Allocator = eastl::NsAllocator,
    bool bCacheHashCode = false>
using NsHashMultimap = eastl::hash_multimap<Key, T, Hash, Predicate, Allocator, bCacheHashCode>;

// NsHashSet
template<class Value, class Hash = eastl::hash<Value>,
    typename Predicate = eastl::equal_to<Value>, class Allocator = eastl::NsAllocator,
    bool bCacheHashCode = false>
using NsHashSet = eastl::hash_set<Value, Hash, Predicate, Allocator, bCacheHashCode>;

// NsHashMultiset
template<class Value, class Hash = eastl::hash<Value>,
    typename Predicate = eastl::equal_to<Value>, class Allocator = eastl::NsAllocator,
    bool bCacheHashCode = false>
using NsHashMultiset = eastl::hash_multiset<Value, Hash, Predicate, Allocator, bCacheHashCode>;

// NsMap
template<class Key, class T, class Compare = eastl::less<Key>,
    typename Allocator = eastl::NsAllocator>
using NsMap = eastl::map<Key, T, Compare, Allocator>;

// NsMultimap
template<class Key, class T, class Compare = eastl::less<Key>,
    typename Allocator = eastl::NsAllocator>
using NsMultimap = eastl::multimap<Key, T, Compare, Allocator>;

// NsSet
template<class Key, class Compare = eastl::less<Key>,
    typename Allocator = eastl::NsAllocator>
using NsSet = eastl::set<Key, Compare, Allocator>;

// NsMultiset
template<class Key, class Compare = eastl::less<Key>,
    typename Allocator = eastl::NsAllocator>
using NsMultiset = eastl::multiset<Key, Compare, Allocator>;

// NsDeque
template<class T, class Allocator = eastl::NsAllocator,
    size_t kNodeSize = eastl::deque_helper<T>::DefaultSize>
class NsDeque;

// NsQueue
template<class T, class Container = NsDeque<T>> class NsQueue;

// NsStack
template<class T, class Container = NsDeque<T>> class NsStack;

#endif
