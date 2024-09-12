// include base.h instead of this

struct UnitTest {
	using UnitTestCallback = bool ( * )();

	const char * name;
	SourceLocation src_loc;
	UnitTestCallback callback;
	UnitTest * next;

	static inline UnitTest * tests_head = NULL;
	static inline UnitTest * tests_tail = NULL;

	UnitTest( const char * name_, SourceLocation src_loc_, UnitTestCallback callback_ ) {
		name = name_;
		src_loc = src_loc_;
		callback = callback_;

		if( tests_tail == NULL ) {
			tests_head = this;
			tests_tail = this;
		}
		else {
			tests_tail->next = this;
			tests_tail = this;
		}
		next = NULL;
	}
};

#if PUBLIC_BUILD
#define TEST( name, body )
#else
#define TEST( name, body ) static UnitTest COUNTER_NAME( unittest )( name, CurrentSourceLocation(), []() -> bool { body } )
#endif
