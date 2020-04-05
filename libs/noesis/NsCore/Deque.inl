////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <NsCore/Error.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace eastl
{

////////////////////////////////////////////////////////////////////////////////////////////////////
// deque_node
// Implemented as a fixed size buffer, mBegin and mEnd are pointers to the next front and back free
// positions. 
// When created, those headers will be set to point to the front, the middle or the back of the 
// buffer, to allow the parent deque container to select the best setup
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
deque_node<T, Allocator, kNodeSize>::deque_node() :
    mBufferBegin(reinterpret_cast<T*>((uintptr_t(mBuffer) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))),
    mBufferEnd(mBufferBegin + kNodeSize - 1),
    mBegin(0),
    mEnd(0),
    mSize(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
deque_node<T, Allocator, kNodeSize>::~deque_node()
{
    FreeAll();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline bool deque_node<T, Allocator, kNodeSize>::can_push_back() const
{
    return (mEnd <= mBufferEnd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline bool deque_node<T, Allocator, kNodeSize>::can_push_front() const
{
    return (mBegin >= mBufferBegin);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque_node<T, Allocator, kNodeSize>::push_back(const T& value)
{
    NS_ASSERT(mBegin != 0);
    NS_ASSERT(can_push_back());

    if (mBegin == mEnd)
    {
        // both headers are pointing at the same direction, we move the begin header to avoid 
        // the next push_front overwriting this address
        --mBegin;
    }

    ::new(mEnd) T(value);
    ++mEnd;

    ++mSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
T& deque_node<T, Allocator, kNodeSize>::push_back()
{
    NS_ASSERT(mBegin != 0);
    NS_ASSERT(can_push_back());

    if (mBegin == mEnd)
    {
        // both headers are pointing at the same direction, we move the begin header to avoid 
        // the next push_front overwriting this address
        --mBegin;
    }

    ::new(mEnd) T();
    ++mEnd;

    ++mSize;

    return back();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque_node<T, Allocator, kNodeSize>::push_front(const T& value)
{
    NS_ASSERT(mBegin != 0);
    NS_ASSERT(can_push_front());

    if (mBegin == mEnd)
    {
        // both headers are pointing at the same direction, we move the end header to avoid 
        // the next push_back overwriting this address
        ++mEnd;
    }

    ::new(mBegin) T(value);
    --mBegin;

    ++mSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
T& deque_node<T, Allocator, kNodeSize>::push_front()
{
    NS_ASSERT(mBegin != 0);
    NS_ASSERT(can_push_front());

    if (mBegin == mEnd)
    {
        // both headers are pointing at the same direction, we move the end header to avoid 
        // the next push_back overwriting this address
        ++mEnd;
    }

    ::new(mBegin) T();
    --mBegin;

    ++mSize;

    return front();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque_node<T, Allocator, kNodeSize>::pop_back()
{
    NS_ASSERT(mBegin != 0);
    NS_ASSERT(mSize != 0);

    if (mSize != 0)
    {
        --mEnd;
        mEnd->~T();

        if (mBegin == mEnd - 1)
        {
            // there are no more elements in the node, both headers should point to the same address
            mBegin = mEnd;
        }
        
        --mSize;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque_node<T, Allocator, kNodeSize>::pop_front()
{
    NS_ASSERT(mBegin != 0);
    NS_ASSERT(mSize != 0);

    if (mSize != 0)
    {
        ++mBegin;
        mBegin->~T();

        if (mEnd == mBegin + 1)
        {
            // there are no more elements in the node, both headers should point to the same address
            mEnd = mBegin;
        }
        
        --mSize;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline T& deque_node<T, Allocator, kNodeSize>::front()
{
    return const_cast<T&>(static_cast<const this_type*>(this)->front());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline const T& deque_node<T, Allocator, kNodeSize>::front() const
{
    NS_ASSERT(mBegin != 0);
    NS_ASSERT(mSize != 0);
    return operator[](0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline T& deque_node<T, Allocator, kNodeSize>::back()
{
    return const_cast<T&>(static_cast<const this_type*>(this)->back());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline const T& deque_node<T, Allocator, kNodeSize>::back() const
{
    NS_ASSERT(mBegin != 0);
    NS_ASSERT(mSize != 0);
    return operator[](mSize-1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline T& deque_node<T, Allocator, kNodeSize>::operator[](size_type n)
{
    return const_cast<T&>(static_cast<const this_type*>(this)->operator[](n));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline const T& deque_node<T, Allocator, kNodeSize>::operator[](size_type n) const
{
    NS_ASSERT(mBegin != 0);
    NS_ASSERT(n < mSize);

    return *(mBegin + n + 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline T& deque_node<T, Allocator, kNodeSize>::at(size_type n)
{
    return const_cast<T&>(static_cast<const this_type*>(this)->at(n));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline const T& deque_node<T, Allocator, kNodeSize>::at(size_type n) const
{
    // we dont't want to throw exceptions
    return operator[](n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline bool deque_node<T, Allocator, kNodeSize>::empty() const
{
    NS_ASSERT(mBegin != 0);
    
    return (mSize == 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque_node<T, Allocator, kNodeSize>::size_type deque_node<T, Allocator, 
    kNodeSize>::size() const
{
    NS_ASSERT(mBegin != 0);
    return mSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline void deque_node<T, Allocator, kNodeSize>::clear()
{   
    FreeAll();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline void deque_node<T, Allocator, kNodeSize>::FreeAll()
{
    while (++mBegin < mEnd)
    {
        mBegin->~T();
    }
    mSize = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque_node<T, Allocator, kNodeSize>::init_headers(HeadPos headPos)
{
    NS_ASSERT(headPos < HeadPos_Count);

    switch (headPos)
    {
    case HeadPos_Start:
        mBegin = mBufferBegin;
        break;

    case HeadPos_Mid:
        mBegin = mBufferBegin + (kNodeSize / 2);
        break;

    case HeadPos_End:
        mBegin = mBufferEnd;
        break;

    default:
        NS_ASSERT_UNREACHABLE;
    }

    mEnd = mBegin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// deque_index
// Implemented as a ring buffer over a standard eastl::vector.
// The elements of the buffer are pointers to deque_nodes
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
deque_index<T, Allocator>::deque_index(Allocator* allocator) :
    mAllocator(allocator),
    mVector(),
    mBegin(0),
    mEnd(0),
    mSize(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
deque_index<T, Allocator>::~deque_index()
{
    for (uint32_t i = 0; i < mVector.size(); ++i)
    {
        DeleteNode(mVector[i]);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
T& deque_index<T, Allocator>::push_back()
{
    size_t vectorSize = mVector.size();

    if (mSize >= vectorSize)
    {
        vectorSize = Grow();            
    }

    if (mSize == 0)
    {
        mVector[mEnd]->init_headers(T::HeadPos_Mid);
    }
    else
    {
        mEnd = (mEnd + 1) % vectorSize;
        mVector[mEnd]->init_headers(T::HeadPos_Start);
    }

    ++mSize;

    return *mVector[mEnd];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
T& deque_index<T, Allocator>::push_front()
{
    size_type vectorSize = mVector.size();

    if (mSize >= vectorSize)
    {
        vectorSize = Grow();
    }

    if (mSize == 0)
    {
        mVector[mEnd]->init_headers(T::HeadPos_Mid);
    }
    else
    {
        mBegin = (mBegin + vectorSize - 1) % vectorSize;
        mVector[mBegin]->init_headers(T::HeadPos_End);
    }

    ++mSize;

    return *mVector[mBegin];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
void deque_index<T, Allocator>::pop_back()
{
    NS_ASSERT(mSize != 0);

    if (mSize != 0)
    {
        --mSize;

        if (mSize != 0)
        {
            size_type vectorSize = mVector.size();
            mEnd = (mEnd + vectorSize - 1) % vectorSize;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
void deque_index<T, Allocator>::pop_front()
{
    NS_ASSERT(mSize != 0);

    if (mSize != 0)
    {
        --mSize;

        if (mSize != 0)
        {
            size_type vectorSize = mVector.size();
            mBegin = (mBegin + 1) % vectorSize;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline T& deque_index<T, Allocator>::operator[](size_type n)
{
    return const_cast<T&>(static_cast<const this_type*>(this)->operator[](n));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline const T& deque_index<T, Allocator>::operator[](size_type n) const
{
    NS_ASSERT(mSize != 0);
    NS_ASSERT(n < mSize);

    size_type vectorSize = mVector.size();
    size_type index = (n + mBegin) % vectorSize;
    return *mVector[index];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline T& deque_index<T, Allocator>::front()
{
    return const_cast<T&>(static_cast<const this_type*>(this)->front());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline const T& deque_index<T, Allocator>::front() const
{
    NS_ASSERT(mSize > 0);
    return operator[](0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline T& deque_index<T, Allocator>::back()
{
    return const_cast<T&>(static_cast<const this_type*>(this)->back());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline const T& deque_index<T, Allocator>::back() const
{
    NS_ASSERT(mSize > 0);
    return operator[](mSize - 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
void deque_index<T, Allocator>::clear()
{
    if (mSize != 0)
    {
        for (size_type i = mBegin; i <= mEnd; ++i)
        {
            operator[](i).clear();
        }
    }

    mBegin = 0;
    mEnd = 0;
    mSize = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
void deque_index<T, Allocator>::grow(size_type n)
{
    size_type currentSize = mVector.size();

    if (n > currentSize)
    {
        size_type offset = n - currentSize;
        typename TVector::iterator it = mVector.begin() + mBegin;
        mVector.insert(it, offset, 0);

        for (size_type i = mBegin; i < mBegin + offset; ++i)
        {
            if (mVector[i] == 0)
            {
                CreateNode(mVector[i]);
            }
        }

        currentSize = mVector.size();

        // move the headers using the % to handle the empty buffer case
        if (mEnd >= mBegin)
        {
            mEnd = (mEnd + offset) % currentSize;
        }

        mBegin = (mBegin + offset) % currentSize;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
void deque_index<T, Allocator>::swap(this_type& other)
{
    eastl::swap_(mAllocator, other.mAllocator);
    eastl::swap_(mVector, other.mVector);
    eastl::swap_(mBegin, other.mBegin);
    eastl::swap_(mEnd, other.mEnd);
    eastl::swap_(mSize, other.mSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline bool deque_index<T, Allocator>::empty() const
{
    return mSize == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline typename deque_index<T, Allocator>::size_type deque_index<T, Allocator>::size() const
{
    return mSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline typename deque_index<T, Allocator>::size_type deque_index<T, Allocator>::capacity() const
{
    return mVector.size();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline void deque_index<T, Allocator>::CreateNode(T*& node)
{
    size_type size = sizeof(T);
    node = static_cast<T*>(mAllocator->allocate(size));
    ::new(node) T();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
inline void deque_index<T, Allocator>::DeleteNode(T*& node)
{
    node->~T();
    size_type size = sizeof(T);
    mAllocator->deallocate(node, size);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
typename deque_index<T, Allocator>::size_type deque_index<T, Allocator>::Grow()
{
    size_type currentSize = mVector.size();
    size_type newSize = (currentSize == 0) ? 1 : currentSize * 2;

    grow(newSize);

    return newSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// deque_iterator
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>::deque_iterator() :
    mParent(0),    
    mOffset(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>::deque_iterator(
    deque_type* parent,
    size_type offset) :
    mParent(parent),    
    mOffset(offset)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>::deque_iterator(
    const this_type_non_const& other) :
    mParent(other.mParent),
    mOffset(other.mOffset)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>&
deque_iterator<T, Allocator, kNodeSize, bConst>::operator=(const this_type_non_const& other)
{
    mParent = other.mParent;
    mOffset = other.mOffset;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline bool deque_iterator<T, Allocator, kNodeSize, bConst>::operator==(const this_type& other)
{
    return (mParent == other.mParent) && (mOffset == other.mOffset);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline bool deque_iterator<T, Allocator, kNodeSize, bConst>::operator!=(const this_type& other)
{
    return (mParent != other.mParent) || (mOffset != other.mOffset);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>&
deque_iterator<T, Allocator, kNodeSize, bConst>::operator++()
{
    ++mOffset;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>
deque_iterator<T, Allocator, kNodeSize, bConst>::operator++(int)
{
    return (++this_type(*this));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>&
deque_iterator<T, Allocator, kNodeSize, bConst>::operator--()
{
    --mOffset;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>
deque_iterator<T, Allocator, kNodeSize, bConst>::operator--(int)
{
    return (--this_type(*this));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>&
deque_iterator<T, Allocator, kNodeSize, bConst>::operator+=(difference_type n)
{
    mOffset += size_type(n);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>
deque_iterator<T, Allocator, kNodeSize, bConst>::operator+(difference_type n)
{
    return (this_type(*this) += n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>&
deque_iterator<T, Allocator, kNodeSize, bConst>::operator-=(difference_type n)
{
    mOffset -= size_type(n);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>
deque_iterator<T, Allocator, kNodeSize, bConst>::operator-(difference_type n)
{
    return (this_type(*this) -= n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline typename deque_iterator<T, Allocator, kNodeSize, bConst>::difference_type
deque_iterator<T, Allocator, kNodeSize, bConst>::operator-(const this_type& other)
{
    return mOffset - other.mOffset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline typename deque_iterator<T, Allocator, kNodeSize, bConst>::reference
deque_iterator<T, Allocator, kNodeSize, bConst>::operator*() const
{
    return (*mParent)[mOffset];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline typename deque_iterator<T, Allocator, kNodeSize, bConst>::pointer
deque_iterator<T, Allocator, kNodeSize, bConst>::operator->() const
{
    return &((*mParent)[mOffset]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline bool deque_iterator<T, Allocator, kNodeSize, bConst>::operator<(const this_type& other) const
{
    NS_ASSERT(mParent == other.mParent);
    return mOffset < other.mOffset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline bool deque_iterator<T, Allocator, kNodeSize, bConst>::operator<=(const this_type& other) const
{
    NS_ASSERT(mParent == other.mParent);
    return mOffset <= other.mOffset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline bool deque_iterator<T, Allocator, kNodeSize, bConst>::operator>(const this_type& other) const
{
    NS_ASSERT(mParent == other.mParent);
    return mOffset > other.mOffset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline bool deque_iterator<T, Allocator, kNodeSize, bConst>::operator>=(const this_type& other) const
{
    NS_ASSERT(mParent == other.mParent);
    return mOffset >= other.mOffset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline typename deque_iterator<T, Allocator, kNodeSize, bConst>::reference
deque_iterator<T, Allocator, kNodeSize, bConst>::operator[](difference_type n)
{
    return (*mParent)[mOffset + n];    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>
operator+(
    typename deque_iterator<T, Allocator, kNodeSize, bConst>::difference_type n,
    const deque_iterator<T, Allocator, kNodeSize, bConst>& other)
{
    return (other + n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
inline deque_iterator<T, Allocator, kNodeSize, bConst>
operator-(
    typename deque_iterator<T, Allocator, kNodeSize, bConst>::difference_type n,
    const deque_iterator<T, Allocator, kNodeSize, bConst>& other)
{
    return (other - n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// deque
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline deque<T, Allocator, kNodeSize>::deque() :
    mAllocator(),
    mIndex(&mAllocator),
    mSize(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline deque<T, Allocator, kNodeSize>::deque(const allocator_type& allocator) :
    mAllocator(allocator),
    mIndex(&mAllocator),
    mSize(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline deque<T, Allocator, kNodeSize>::deque(size_type n, const allocator_type& allocator) :
    mAllocator(allocator),
    mIndex(&mAllocator),
    mSize(0)
{
    resize(n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline deque<T, Allocator, kNodeSize>::deque(
    size_type n, 
    const value_type& value, 
    const allocator_type& allocator) :
    mAllocator(allocator),
    mIndex(&mAllocator),
    mSize(0)
{
    resize(n, value);
}
    
////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline deque<T, Allocator, kNodeSize>::deque(const this_type& other) :
    mAllocator(other.mAllocator),
    mIndex(&mAllocator),
    mSize(0)
{
    assign(other.begin(), other.end());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
template <typename InputIterator>
inline deque<T, Allocator, kNodeSize>::deque(InputIterator first, InputIterator last) :
    mAllocator(),
    mIndex(&mAllocator),
    mSize(0)
{
    assign(first, last);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
deque<T, Allocator, kNodeSize>::~deque()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline deque<T, Allocator, kNodeSize>& 
deque<T, Allocator, kNodeSize>::operator=(const this_type& other)
{
    assign(other.begin(), other.end());
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque<T, Allocator, kNodeSize>::swap(this_type& other)
{
    eastl::swap_(mAllocator, other.mAllocator);
    mIndex.swap(other.mIndex);
    eastl::swap_(mSize, other.mSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline void deque<T, Allocator, kNodeSize>::assign(size_type n, const value_type& value)
{
    clear();
    resize(n, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
template <typename InputIterator>
inline void deque<T, Allocator, kNodeSize>::assign(InputIterator first, InputIterator last)
{
    clear();

    difference_type diff = last - first;

    reserve(size_type(diff)); 

    for (InputIterator it = first; it != last; ++it)
    {
        push_back(*it);
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline void deque<T, Allocator, kNodeSize>::push_back(const value_type& value)
{
    GrowBackIfNeeded();

    mIndex.back().push_back(value);        

    ++mSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::reference
deque<T, Allocator, kNodeSize>::push_back()
{
    GrowBackIfNeeded();

    mIndex.back().push_back();        

    ++mSize;    

    return back();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline void deque<T, Allocator, kNodeSize>::push_front(const value_type& value)
{
    GrowFrontIfNeeded();

    mIndex.front().push_front(value);        

    ++mSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::reference
deque<T, Allocator, kNodeSize>::push_front()
{
    GrowFrontIfNeeded();

    mIndex.front().push_front();        

    ++mSize;

    return front();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque<T, Allocator, kNodeSize>::pop_back()
{
    NS_ASSERT(mSize > 0);

    if (mSize > 0)
    {
        mIndex.back().pop_back();

        --mSize;

        if (mIndex.back().empty())
        {
            mIndex.pop_back();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque<T, Allocator, kNodeSize>::pop_front()
{
    NS_ASSERT(mSize > 0);

    if (mSize > 0)
    {
        mIndex.front().pop_front();

        --mSize;

        if (mIndex.front().empty())
        {
            mIndex.pop_front();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::reference 
deque<T, Allocator, kNodeSize>::operator[](size_type n)
{
    return const_cast<reference>(static_cast<const this_type*>(this)->operator[](n));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
typename deque<T, Allocator, kNodeSize>::const_reference 
deque<T, Allocator, kNodeSize>::operator[](size_type n) const
{
    NS_ASSERT(mSize > 0);
    NS_ASSERT(n < mSize);

    size_type fistNodeCount = mIndex.front().size();

    if (n < fistNodeCount)
    {
        return mIndex.front()[n];
    }
    else
    {
        n -= fistNodeCount;
        size_type nodeIndex = (n / kNodeSize) + 1;
        n %= kNodeSize;

        return mIndex[nodeIndex][n];
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::reference 
deque<T, Allocator, kNodeSize>::at(size_t n)
{
    return const_cast<reference>(static_cast<const this_type*>(this)->at(n));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::const_reference 
deque<T, Allocator, kNodeSize>::at(size_t n) const
{
    // we dont't want to throw exceptions
    return operator[](n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::reference
deque<T, Allocator, kNodeSize>::front()
{
    return const_cast<reference>(static_cast<const this_type*>(this)->front());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::const_reference
deque<T, Allocator, kNodeSize>::front() const
{
    return operator[](0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::reference
deque<T, Allocator, kNodeSize>::back()
{
    return const_cast<reference>(static_cast<const this_type*>(this)->back());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::const_reference
deque<T, Allocator, kNodeSize>::back() const
{
    return operator[](mSize-1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::iterator
deque<T, Allocator, kNodeSize>::begin()
{
    return iterator(this, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::const_iterator
deque<T, Allocator, kNodeSize>::begin() const
{
    return const_iterator(const_cast<this_type*>(this), 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::iterator
deque<T, Allocator, kNodeSize>::end()
{
    return iterator(this, mSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::const_iterator
deque<T, Allocator, kNodeSize>::end() const
{
    return const_iterator(const_cast<this_type*>(this), mSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::reverse_iterator
deque<T, Allocator, kNodeSize>::rbegin()
{
    return reverse_iterator(iterator(this, mSize));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::const_reverse_iterator
deque<T, Allocator, kNodeSize>::rbegin() const
{
    return const_reverse_iterator(iterator(const_cast<this_type*>(this), mSize));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::reverse_iterator
deque<T, Allocator, kNodeSize>::rend()
{
    return reverse_iterator(iterator(this, 0));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::const_reverse_iterator
deque<T, Allocator, kNodeSize>::rend() const
{
    return const_reverse_iterator(iterator(const_cast<this_type*>(this), 0));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline bool deque<T, Allocator, kNodeSize>::empty() const
{
    return mSize == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::size_type 
deque<T, Allocator, kNodeSize>::size() const
{
    return mSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::size_type 
deque<T, Allocator, kNodeSize>::capacity() const
{
    return size_type(mIndex.capacity() * kNodeSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque<T, Allocator, kNodeSize>::resize(size_type n)
{
    resize(n, value_type());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque<T, Allocator, kNodeSize>::resize(size_type n, const value_type& value)
{
    if (n > mSize)
    {
        size_type diff = n - mSize;

        reserve(mSize + diff); 

        for (size_type i = 0; i < diff; ++i)
        {
            push_back(value);
        }
    }
    else
    {
        size_type diff = mSize - n;
        for (size_type i = 0; i < diff; ++i)
        {
            pop_back();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline void deque<T, Allocator, kNodeSize>::reserve(size_type n)
{
    size_type currentCapacity = capacity();

    if (n > currentCapacity)
    {
        size_type nodesNeeded = (n / kNodeSize) + 1;
        mIndex.grow(nodesNeeded);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline void deque<T, Allocator, kNodeSize>::clear()
{
    mIndex.clear();
    mSize = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename deque<T, Allocator, kNodeSize>::allocator_type&
deque<T, Allocator, kNodeSize>::get_allocator()
{
    return mAllocator;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque<T, Allocator, kNodeSize>::GrowBackIfNeeded()
{
    if (mIndex.empty() || !mIndex.back().can_push_back())
    {
        mIndex.push_back();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
void deque<T, Allocator, kNodeSize>::GrowFrontIfNeeded()
{
    if (mIndex.empty() || !mIndex.front().can_push_front())
    {
        mIndex.push_front();
    }
}

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// NsDeque
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline NsDeque<T, Allocator, kNodeSize>::NsDeque():
    BaseType()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline NsDeque<T, Allocator, kNodeSize>::NsDeque(const allocator_type& allocator) :
    BaseType(allocator)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline NsDeque<T, Allocator, kNodeSize>::NsDeque(size_type n, const allocator_type& allocator) :
    BaseType(n, allocator)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline NsDeque<T, Allocator, kNodeSize>::NsDeque(
    size_type n, 
    const value_type& value, 
    const allocator_type& allocator) :
    BaseType(n, value, allocator)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
NsDeque<T, Allocator, kNodeSize>::NsDeque(const ThisType& other) :
    BaseType(other)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
template <typename InputIterator>
inline NsDeque<T, Allocator, kNodeSize>::NsDeque(InputIterator first, InputIterator last) :
    BaseType(first, last)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
inline typename NsDeque<T, Allocator, kNodeSize>::ThisType& 
NsDeque<T, Allocator, kNodeSize>::operator=(const ThisType& other)
{
    return static_cast<ThisType&>(BaseType::operator=(other));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Visual Studio Debugging
//
// To improve the VS2005 debugging visualization of deque containers, add this to the autoexp.dat 
// file in the [Visualizer] section. Do NOT insert carriage returns or spaces.
//
// ;------------------------------------------------------------------------------
// ; NsDeque
// ; INDEX SIZE: ($c.mIndex.mVector.mpEnd - $c.mIndex.mVector.mpBegin)
// ; NODE OFFSET: ((((($i - $c.mIndex.mVector.mpBegin[$c.mIndex.mBegin]->mSize) / $T2) + 1) + $c.mIndex.mBegin) % ($c.mIndex.mVector.mpEnd - $c.mIndex.mVector.mpBegin))
// ; ELEMENT OFFSET: (($i - $c.mIndex.mVector.mpBegin[$c.mIndex.mBegin]->mSize) % $T2)
// ;------------------------------------------------------------------------------
// NsDeque<*,*,*>|eastl::deque<*,*,*>{
//     preview 
//     (#(
//         "size=", [$c.mSize, u], ", capacity=", [(($c.mIndex.mVector.mpEnd - $c.mIndex.mVector.mpBegin) * $T2), u], ", free=", [((($c.mIndex.mVector.mpEnd - $c.mIndex.mVector.mpBegin) * $T2) - $c.mSize), u]
//     ))
//     children
//     (#(
//         [_raw debug]: [$c,!],
//         #array 
//         (
//             expr:
//             #(
//                 #if ($i < $c.mIndex.mVector.mpBegin[$c.mIndex.mBegin]->mSize)
//                 ( 
//                     $c.mIndex.mVector.mpBegin[$c.mIndex.mBegin]->mBufferBegin[$i+1]
//                 ) 
//                 #else 
//                 (
//                     $c.mIndex.mVector.mpBegin[((((($i - $c.mIndex.mVector.mpBegin[$c.mIndex.mBegin]->mSize) / $T2) + 1) + $c.mIndex.mBegin) % ($c.mIndex.mVector.mpEnd - $c.mIndex.mVector.mpBegin))]->mBufferBegin[(($i - $c.mIndex.mVector.mpBegin[$c.mIndex.mBegin]->mSize) % $T2)]
//                 )
//             ),
//             size: $c.mSize
//         )
//     ))
// }
////////////////////////////////////////////////////////////////////////////////////////////////////
