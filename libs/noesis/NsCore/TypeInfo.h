////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_TYPEINFO_H__
#define __CORE_TYPEINFO_H__


#include <NsCore/Noesis.h>


#if defined(NS_COMPILER_GCC)

    #include <NsCore/KernelApi.h>
    extern NS_CORE_KERNEL_API const uint32_t NsPrettyFunctionOffset;
    
    // Do not use char here because the output of __PRETTY_FUNCTION__ varies dependening of
    // the namespace who calls NsTypeName()
    template<class T> const char* NsTypeName(T* = 0)
    {
        return __PRETTY_FUNCTION__ + NsPrettyFunctionOffset;
    }

#else

#ifdef NS_COMPILER_MSVC
    // https://connect.microsoft.com/VisualStudio/feedback/details/1600701
    #include <exception>
#endif

    #include <typeinfo.h>

    template<class T> const char* NsTypeName()
    {
        // raw_name() better than name() because it is smaller and we are going to use it to compare
        return typeid(T).raw_name();
    }

#endif

#define NS_TYPEID(T) Noesis::TypeInfo(static_cast<const char*>(NsTypeName<T>()))

namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeInfo, offers a first-class, comparable wrapper for types created with NS_TYPEID
////////////////////////////////////////////////////////////////////////////////////////////////////
class TypeInfo
{
public:
    inline TypeInfo();
    inline TypeInfo(const char* id);

    inline bool operator==(const TypeInfo& typeInfo) const;
    inline bool operator!=(const TypeInfo& typeInfo) const;

    inline bool operator<(const TypeInfo& typeInfo) const;
    inline bool operator<=(const TypeInfo& typeInfo) const;
    inline bool operator>(const TypeInfo& typeInfo) const;
    inline bool operator>=(const TypeInfo& typeInfo) const;

    inline const char* GetId() const;

private:
    TypeInfo& operator=(const TypeInfo&);

private:
    const char* const mId;
};

}

// Inline include
#include <NsCore/TypeInfo.inl>

#endif
