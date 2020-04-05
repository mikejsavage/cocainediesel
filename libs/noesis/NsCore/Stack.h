////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_STACK_H__
#define __CORE_STACK_H__


#include <NsCore/Noesis.h>
#include <NsCore/NSTLForwards.h>
#include <NsCore/Deque.h>


namespace eastl
{

////////////////////////////////////////////////////////////////////////////////////////////////////
// stack
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, typename Container>
class stack
{
public:
    typedef stack<T, Container> this_type;

public:
    typedef Container                                container_type;
    typedef typename container_type::value_type      value_type;
    typedef typename container_type::size_type       size_type;
    typedef typename container_type::reference       reference;
    typedef typename container_type::const_reference const_reference;

public:
    stack();
    stack(const this_type& other);

    this_type& operator=(const this_type& other);
    
    void push(const value_type& value);

    void pop();

    reference       top();
    const_reference top() const;

    bool empty() const;
    size_type size() const;
    size_type capacity() const;

    container_type& get_container();

private:
    container_type mContainer;
};

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// NsStack, same interface as std::stack
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, typename Container>
class NsStack: public eastl::stack<T, Container>
{
private:
    typedef eastl::stack<T, Container> BaseType;
    typedef NsStack<T, Container> ThisType;

public:
    NsStack();
    NsStack(const ThisType& other);

    ThisType& operator=(const ThisType& other);
};

#include "Stack.inl"


#endif
