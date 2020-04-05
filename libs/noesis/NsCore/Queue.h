////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_QUEUE_H__
#define __CORE_QUEUE_H__


#include <NsCore/Noesis.h>
#include <NsCore/NSTLForwards.h>
#include <NsCore/Deque.h>


namespace eastl
{

////////////////////////////////////////////////////////////////////////////////////////////////////
// queue
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, typename Container>
class queue
{
public:
    typedef queue<T, Container> this_type;

public:
    typedef Container                                container_type;
    typedef typename container_type::value_type      value_type;
    typedef typename container_type::size_type       size_type;
    typedef typename container_type::reference       reference;
    typedef typename container_type::const_reference const_reference;

public:
    queue();
    queue(const this_type& other);

    this_type& operator=(const this_type& other);
    
    void push(const value_type& value);

    void pop();

    reference       front();
    const_reference front() const;

    reference       back();
    const_reference back() const;

    bool empty() const;
    size_type size() const;
    size_type capacity() const;

    container_type& get_container();

private:
    container_type mContainer;
};

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// NsQueue, same interface as std::queue
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, typename Container>
class NsQueue: public eastl::queue<T, Container>
{
private:
    typedef eastl::queue<T, Container> BaseType;
    typedef NsQueue<T, Container> ThisType;

public:
    NsQueue();
    NsQueue(const ThisType& other);

    ThisType& operator=(const ThisType& other);
};

#include "Queue.inl"


#endif
