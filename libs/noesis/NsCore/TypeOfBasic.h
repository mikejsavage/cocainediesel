////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_TYPEOFBASIC_H__
#define __CORE_TYPEOFBASIC_H__


#ifndef __INCLUDED_FROM_TYPEOF_H__
    #error Do not include this file directly. Use <TypeOf.h> instead
#endif


#include <NsCore/Noesis.h>
#include <NsCore/TypeOfBase.h>


namespace Noesis
{

class Symbol;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// TypeOf specialization for basic types.
////////////////////////////////////////////////////////////////////////////////////////////////////
//@{
template<> class TypeOfHelper<int8_t>: public TypeOfHelperBase<Type, Type, int8_t> { };
template<> class TypeOfHelper<int16_t>: public TypeOfHelperBase<Type, Type, int16_t> { };
template<> class TypeOfHelper<int32_t>: public TypeOfHelperBase<Type, Type, int32_t> { };
template<> class TypeOfHelper<int64_t>: public TypeOfHelperBase<Type, Type, int64_t> { };
template<> class TypeOfHelper<uint8_t>: public TypeOfHelperBase<Type, Type, uint8_t> { };
template<> class TypeOfHelper<uint16_t>: public TypeOfHelperBase<Type, Type, uint16_t> { };
template<> class TypeOfHelper<uint32_t>: public TypeOfHelperBase<Type, Type, uint32_t> { };
template<> class TypeOfHelper<uint64_t>: public TypeOfHelperBase<Type, Type, uint64_t> { };
template<> class TypeOfHelper<float>: public TypeOfHelperBase<Type, Type, float> { };
template<> class TypeOfHelper<double>: public TypeOfHelperBase<Type, Type, double> { };
template<> class TypeOfHelper<char>: public TypeOfHelperBase<Type, Type, char> { };
template<> class TypeOfHelper<bool>: public TypeOfHelperBase<Type, Type, bool> { };
template<> class TypeOfHelper<Symbol>: public TypeOfHelperBase<Type, Type, Symbol> { };
template<> class TypeOfHelper<NsString>: public TypeOfHelperBase<Type, Type, NsString> { };
//@}

}

#endif
