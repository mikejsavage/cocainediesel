////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsCore/Noesis.h>
#include <NsCore/CompilerTools.h>
#include <NsCore/TypeEnumHelper.h>


namespace Noesis
{

class Type;
class TypeClass;
class TypeEnum;
template<class T> struct TypeEnumFiller;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeOfHelper for classes and enums.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class TypeOfHelper
{
private:
    struct Class
    {
        typedef TypeClass ReturnType;
        static const ReturnType* Get() { return T::StaticGetClassType((TypeTag<T>*)nullptr); }
    };

    struct Enum
    {
        typedef TypeEnum ReturnType;
        static const ReturnType* Get() { return TypeEnumFiller<T>::GetType(); }
    };

    typedef If<IsEnum<T>::Result, Enum, Class> ClassEnumCheck;

public:
    typedef typename ClassEnumCheck::ReturnType ReturnType;

    static const ReturnType* Get()
    {
        return ClassEnumCheck::Get();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Base class used to implement TypeOf
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class ReturnT, class CreateT, class T, class S = T>
class TypeOfHelperBase
{
public:
    typedef ReturnT ReturnType;

    static const ReturnT* Get()
    {
        static const ReturnT* type;

        if (type == 0)
        {
            type = static_cast<const ReturnT*>(TypeCreate::Create(NS_TYPEID(S), Create,
                TypeOfHelper<T>::Fill));
        }

        return type;
    }

private:
    static Type* Create(const TypeInfo& typeInfo)
    {
        return new CreateT(typeInfo);
    }

    static void Fill(Type*)
    {
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
const typename TypeOfHelper<T>::ReturnType* TypeOf()
{
    return TypeOfHelper<T>::Get();
}

typedef void NoParent;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeOf specialization for NoParent.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<> class TypeOfHelper<NoParent>
{
public:
    typedef Type ReturnType;

    static const ReturnType* Get()
    {
        return 0;
    }
};

}
