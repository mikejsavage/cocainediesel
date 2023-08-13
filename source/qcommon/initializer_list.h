// include types.h instead of this

#if COMPILER_GCC

// GCC refuses to compile if the implementation doesn't exactly match the STL
#include <initializer_list>

#else

#define _INITIALIZER_LIST_ // MSVC
#define _LIBCPP_INITIALIZER_LIST // clang

namespace std {
	template< typename T >
	class initializer_list {
	public:

#if _MSC_VER
		constexpr initializer_list( const T * first_, const T * one_after_end_ ) : first( first_ ), one_after_end( one_after_end_ ) { }
#else
		constexpr initializer_list( const T * first_, size_t n ) : first( first_ ), one_after_end( first_ + n ) { }
#endif

		const T * begin() const { return first; }
		const T * end() const { return one_after_end; }
		size_t size() const { return one_after_end - first; }

	private:
		const T * first;
		const T * one_after_end;
	};
}

#endif
