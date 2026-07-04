// include base.h instead of this

struct Test {
	using TestCallback = bool ( * )();

	const char * name;
	SourceLocation src_loc;
	TestCallback callback;
	Test * next;

	static inline Test * tests_head = NULL;
	static inline Test * tests_tail = NULL;

	Test( const char * name_, SourceLocation src_loc_, TestCallback callback_ ) {
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
#define TEST( name ) [[maybe_unused]] static bool COUNTER_NAME( runtest )()
#else
#define TEST( name ) \
	static bool LINE_NAME( runtest )(); \
	static Test LINE_NAME( test )( name, CurrentSourceLocation(), LINE_NAME( runtest ) ); \
	static bool LINE_NAME( runtest )()
#endif
