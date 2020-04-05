////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_DEQUE_H__
#define __CORE_DEQUE_H__


#include <NsCore/Noesis.h>
#include <NsCore/NSTLForwards.h>
#include <NsCore/Vector.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
namespace eastl
{

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
class deque_node
{
public:
    static_assert(kNodeSize > 0, "Deque kNodeSize must be greater than 0");
    typedef deque_node<T, Allocator, kNodeSize> this_type;
    typedef eastl_size_t size_type;

public:
    enum HeadPos
    {
        HeadPos_Start = 0,
        HeadPos_Mid,
        HeadPos_End,

        HeadPos_Count,
    };

public:
    deque_node();
    ~deque_node();

    bool can_push_back() const;
    bool can_push_front() const;

    void push_back(const T& value);
    T&   push_back();

    void push_front(const T& value);
    T&   push_front();

    void pop_back();
    void pop_front();

    T&       front();
    const T& front() const;

    T&       back();
    const T& back() const;

    T&       operator[](size_type n);
    const T& operator[](size_type n) const;

    T&       at(size_type n);
    const T& at(size_type n) const;

    bool empty() const;

    size_type size() const;

    void clear();

    void init_headers(HeadPos headPos);

private:
    void FreeAll();

private:
    static const size_t ALIGNMENT = 8;
    static const size_t BUFFER_SIZE = kNodeSize * sizeof(T) + (ALIGNMENT - 1);

private:
    Allocator mAllocator;
    char      mBuffer[BUFFER_SIZE];
    T*        mBufferBegin;
    T*        mBufferEnd;
    T*        mBegin;
    T*        mEnd;
    size_type mSize;

private:
    // disallow copy and assign
    deque_node(const this_type&);
    this_type& operator=(const this_type&);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator>
class deque_index
{
public: 
    typedef deque_index<T, Allocator> this_type;
    typedef eastl_size_t size_type;

public:
    deque_index(Allocator* allocator);
    ~deque_index();

    T& push_back();
    T& push_front();

    void pop_back();
    void pop_front();

    T&       operator[](size_type n);
    const T& operator[](size_type n) const;

    T& front();
    const T& front() const;

    T& back();
    const T& back() const;

    bool empty() const;
    size_type size() const;
    size_type capacity() const;

    void clear();

    void grow(size_type n);

    void swap(this_type& other);

private:
    typedef eastl::vector<T*, Allocator> TVector;

private:
    void CreateNode(T*& node);
    void DeleteNode(T*& node);
    size_type Grow();

private:
    Allocator* mAllocator;

    TVector mVector;
    
    size_type mBegin;
    size_type mEnd;
    size_type mSize;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
class deque;

////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
class deque_iterator
{
public:
    typedef deque_iterator<T, Allocator, kNodeSize, bConst> this_type;
    typedef deque<T, Allocator, kNodeSize>                  deque_type;
    typedef deque_iterator<T, Allocator, kNodeSize, false>  this_type_non_const;
    typedef deque_iterator<T, Allocator, kNodeSize, true>   this_type_const;

public:
    typedef T                                                                  value_type;
    typedef typename type_select<bConst, const value_type*, value_type*>::type pointer;
    typedef typename type_select<bConst, const value_type&, value_type&>::type reference;
    typedef EASTL_ITC_NS::random_access_iterator_tag                           iterator_category;
    typedef eastl_size_t                                                       size_type;
    typedef ptrdiff_t                                                          difference_type;
    
public:
    deque_iterator();
    deque_iterator(deque_type* parent, size_type offset);
    deque_iterator(const this_type_non_const& other);

    this_type& operator=(const this_type_non_const& other);

    bool operator==(const this_type& other);
    bool operator!=(const this_type& other);

    this_type& operator++();
    this_type  operator++(int);

    this_type& operator--();
    this_type  operator--(int);

    this_type& operator+=(difference_type n);
    this_type  operator+(difference_type n);

    this_type& operator-=(difference_type n);
    this_type  operator-(difference_type n);

    difference_type operator-(const this_type& other);

    reference operator*() const;
    pointer   operator->() const;

    bool operator<(const this_type& other) const; 
    bool operator<=(const this_type& other) const; 
    
    bool operator>(const this_type& other) const; 
    bool operator>=(const this_type& other) const; 

    reference operator[](difference_type n);

private:
    deque_type* mParent;
    size_type   mOffset;

    friend class deque_iterator<T, Allocator, kNodeSize, true>;
};

template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
deque_iterator<T, Allocator, kNodeSize, bConst>
operator+(
    typename deque_iterator<T, Allocator, kNodeSize, bConst>::difference_type n,
    const deque_iterator<T, Allocator, kNodeSize, bConst>& other);

template<typename T, typename Allocator, size_t kNodeSize, bool bConst>
deque_iterator<T, Allocator, kNodeSize, bConst>
operator-(
    typename deque_iterator<T, Allocator, kNodeSize, bConst>::difference_type n,
    const deque_iterator<T, Allocator, kNodeSize, bConst>& other);


////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
class deque
{
public:
    typedef deque<T, Allocator, kNodeSize> this_type;

public:
    typedef T         value_type;
    typedef T*        pointer;
    typedef const T*  const_pointer;
    typedef T&        reference;
    typedef const T&  const_reference;
    typedef eastl_size_t    size_type;
    typedef Allocator allocator_type;
    
    typedef deque_iterator<T, Allocator, kNodeSize, false> iterator;
    typedef deque_iterator<T, Allocator, kNodeSize, true>  const_iterator;
    typedef eastl::reverse_iterator<iterator>              reverse_iterator;
    typedef eastl::reverse_iterator<const_iterator>        const_reverse_iterator;

    typedef typename iterator::difference_type difference_type;

public:
    // used in reserve() to allow the user to select the container growth direction
    enum ReserveDir
    {
        ReserveDir_Both,
        ReserveDir_Front,
        ReserveDir_Back,

        ReserveDir_Count,
    };

public:
    deque();
    explicit deque(const allocator_type& allocator);
    explicit deque(size_type n, const allocator_type& allocator);
    deque(size_type n, const value_type& value, const allocator_type& allocator);
    deque(const this_type& other);

    template <typename InputIterator>
    deque(InputIterator first, InputIterator last);

    ~deque();

    this_type& operator=(const this_type& other);

    void swap(this_type& other);

    void assign(size_type n, const value_type& value);

    template <typename InputIterator>
    void assign(InputIterator first, InputIterator last);

    void      push_back(const value_type& value);
    reference push_back();

    void      push_front(const value_type& value);
    reference push_front();

    void pop_back();
    void pop_front();

    reference       operator[](size_type n);
    const_reference operator[](size_type n) const;

    reference       at(size_t n);
    const_reference at(size_t n) const;

    reference       front();
    const_reference front() const;

    reference       back();
    const_reference back() const;

    iterator       begin();
    const_iterator begin() const;

    iterator       end();
    const_iterator end() const;

    reverse_iterator       rbegin();
    const_reverse_iterator rbegin() const;

    reverse_iterator       rend();
    const_reverse_iterator rend() const;

    bool empty() const;
    size_type size() const;
    size_type capacity() const;

    void resize(size_type n);
    void resize(size_type n, const value_type& value);

    void reserve(size_type n);

    void clear(); 

    allocator_type& get_allocator();

private:
    typedef deque_node<T, Allocator, kNodeSize> TDequeNode;
    typedef deque_index<TDequeNode, Allocator> TDequeIndex;

private:
    void GrowBackIfNeeded();
    void GrowFrontIfNeeded();

private:

    Allocator   mAllocator;
    TDequeIndex mIndex;
    size_type   mSize;
};

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// NsDeque, same interface as std::deque with the following improvements:
//  - Extra template parameter to manually set the size of each block
////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename Allocator, size_t kNodeSize>
class NsDeque: public eastl::deque<T, Allocator, kNodeSize>
{
private:
    typedef NsDeque<T, Allocator, kNodeSize> ThisType;
    typedef eastl::deque<T, Allocator, kNodeSize> BaseType;

public:
    typedef typename BaseType::value_type value_type;
    typedef typename BaseType::allocator_type allocator_type;
    typedef typename BaseType::size_type size_type;

public:
    NsDeque();
    explicit NsDeque(const allocator_type& allocator);
    explicit NsDeque(size_type n, const allocator_type& allocator);
    NsDeque(size_type n, const value_type& value, const allocator_type& allocator);
    NsDeque(const ThisType& other);

    template <typename InputIterator>
    NsDeque(InputIterator first, InputIterator last);

    ThisType& operator=(const ThisType& other);
};

#include "Deque.inl"

#endif
