////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
// [CR #518]
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CORE_SYMBOL_H__
#define __CORE_SYMBOL_H__


#include <NsCore/Noesis.h>
#include <NsCore/KernelApi.h>
#include <NsCore/NSTLForwards.h>
#include <EASTL/functional.h>


// These functions are to be used from the debugger Watch Window to convert symbols to strings
//  and vice versa while debugging.
// Use the following syntax in Microsoft Visual Studio:
//
//  {,,Core.Kernel.dll}NsSymbolToString(1146)
//  {,,Core.Kernel.dll}NsStringToSymbol("DiskFileSystem")
//@{
extern "C" NS_CORE_KERNEL_API const char* NsSymbolToString(uint32_t id);
extern "C" NS_CORE_KERNEL_API uint32_t NsStringToSymbol(const char* str);
//@}


namespace Noesis
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper macros for using symbols. The macro NS_DECLARE_SYMBOL implements a static symbol and
/// the NSS macro reuse that symbol.
///
/// NS_DECLARE_SYMBOL(name)
/// 
/// void Test()
/// {
///     NsSymbol sym = NSS(name);
/// }
///
////////////////////////////////////////////////////////////////////////////////////////////////////
#define NS_DECLARE_SYMBOL(id) \
    namespace \
    { \
        Noesis::Symbol __sSymbol##id; \
    }

#define NSS(id) ((__sSymbol##id) == 0 ? ((__sSymbol##id.SetId( \
    Noesis::Symbol::IdOfStaticString(#id)))): (__sSymbol##id))

/// Enumeration used when a symbol is created from a string
enum CreationMode
{
    // This mode indicates that a new symbol is created if it doesn't exist previously
    CreationMode_Add,
    // This mode indicates that if the symbol is not found it is not created and a null symbol is
    //  returned
    CreationMode_Dont_Add
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A symbol represents a shared string that is stored in a symbol table. Each string stored in that
/// table is represented uniquely by an index. A symbol stores that index. That way the size of a
/// symbol is sizeof(uint32_t) and comparison operators can be implemented very efficiently.
///
/// Symbols are used in Noesis to represent strings defined at compile time.
///
/// Symbols are case-insensitive.
////////////////////////////////////////////////////////////////////////////////////////////////////
class NS_CORE_KERNEL_API Symbol
{
public:
    /// Constructor
    inline Symbol();
    
    /// Constructor
    explicit Symbol(const char* str, CreationMode creationMode = CreationMode_Add);

    /// Constructor
    explicit Symbol(const NsString& str, CreationMode creationMode = CreationMode_Add);

    /// Constructor
    explicit Symbol(uint32_t index);

    /// Gets symbol string
    const char* GetStr() const;

    /// Gets symbol identifier
    inline uint32_t GetId() const;
    
    /// Sets symbol identifier
    inline Symbol& SetId(uint32_t id);

    /// Checks if this symbol is the empty string
    inline bool IsNull() const;

    /// Comparison operators
    //@{
    inline bool operator==(const Symbol& symbol) const;
    inline bool operator!=(const Symbol& symbol) const;
    inline bool operator<(const Symbol& symbol) const;
    inline bool operator>(const Symbol& symbol) const;
    inline bool operator<=(const Symbol& symbol) const;
    inline bool operator>=(const Symbol& symbol) const;
    //@}
    
    /// conversion to int
    inline operator uint32_t() const;

    /// Gets the null symbol (representing an empty string)
    inline static Symbol Null();
    
    /// Returns the identifier associated to the static string str (creates a new one if it doesn't 
    /// exist). Function used by the NSS macro
    static uint32_t IdOfStaticString(const char* str);

private:
    uint32_t mIndex;
};

/// Dump information to the console about the symbols currently generated
NS_CORE_KERNEL_API void DumpSymbols();

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Hash function
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace eastl
{
template<> struct hash<Noesis::Symbol>
{
    size_t operator()(const Noesis::Symbol& s) const
    {
        return s.GetId();
    }
};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Global access of symbol type.
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef Noesis::Symbol NsSymbol;

// Inline include
#include <NsCore/Symbol.inl>

#endif
