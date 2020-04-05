////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_TYPEENUM_H__
#define __CORE_TYPEENUM_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>
#include <NsCore/Symbol.h>
#include <NsCore/TypeMeta.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/Vector.h>


namespace Noesis
{

class BoxedValue;

NS_WARNING_PUSH
NS_MSVC_WARNING_DISABLE(4251 4275)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeEnum. Defines reflection info for enumerations.
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_CORE_KERNEL_API TypeEnum: public TypeMeta
{
public:
    /// Constructor
    TypeEnum(const TypeInfo& typeInfo);

    /// Destructor
    ~TypeEnum();

    /// Defines enum value information
    struct ValueInfo
    {
        NsSymbol id;
        int value;

        ValueInfo(NsSymbol i, int v): id(i), value(v) { }
    };

    /// Gets number of enumeration values
    uint32_t GetNumValues() const;

    /// Gets enum value info
    const ValueInfo* GetValueInfo(uint32_t index) const;

    /// Indicates if an enum defines the specified value
    //@{
    bool HasValue(NsSymbol id) const;
    bool HasValue(int value) const;
    //@}

    /// Gets value of the specified enumeration identifier
    /// An error occurrs if the identifier is not defined in the enumeration
    //@{
    int GetValue(NsSymbol id) const;
    NsSymbol GetValue(int value) const;
    //@}
    
    /// Gets a boxed value of the specified enumeration identifier
    /// An error occurrs if the identifier is not defined in the enumeration
    virtual Ptr<BoxedValue> GetValueObject(NsSymbol id) const = 0;

    /// Enumeration type creation
    void AddValue(NsSymbol id, int value);

private:
    const ValueInfo* TryGetValueInfo(NsSymbol id) const;
    const ValueInfo* TryGetValueInfo(int value) const;

private:
    typedef NsVector<ValueInfo*> ValueVector;
    ValueVector mValues;

    NS_DECLARE_REFLECTION(TypeEnum, TypeMeta)
};

NS_WARNING_POP

}

#endif
