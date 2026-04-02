// include base.h instead of this

struct UnitTest {
	using UnitTestCallback = bool ( * )();

	const char * name;
	SourceLocation src_loc;
	UnitTestCallback callback;
	UnitTest * next;

	UnitTest() = default;
	static UnitTest dummy;
	static inline const UnitTest * tests_head = &dummy;
	static inline UnitTest * tests_tail = &dummy;

	UnitTest( const char * name_, SourceLocation src_loc_, UnitTestCallback callback_ ) {
		name = name_;
		src_loc = src_loc_;
		callback = callback_;
		next = NULL;

		tests_tail->next = this;
		tests_tail = this;
	}
};

inline UnitTest UnitTest::dummy = { };

#if PUBLIC_BUILD
#define TEST( name ) [[maybe_unused]] static bool COUNTER_NAME( rununittest )()
#else
#define TEST( name ) \
	static bool LINE_NAME( rununittest )(); \
	static UnitTest LINE_NAME( unittest )( name, CurrentSourceLocation(), LINE_NAME( rununittest ) ); \
	static bool LINE_NAME( rununittest )()
#endif
