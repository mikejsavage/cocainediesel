// include base.h instead of this

extern Allocator * sys_allocator;

struct ArenaAllocator;
struct TempAllocator final : public Allocator {
	TempAllocator() = default;
	TempAllocator( const TempAllocator & other );
	~TempAllocator();

	void operator=( const TempAllocator & ) = delete;

	void * try_allocate( size_t size, size_t alignment, SourceLocation src = CurrentSourceLocation() );
	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, SourceLocation src = CurrentSourceLocation() );
	void deallocate( void * ptr, SourceLocation src = CurrentSourceLocation() );

private:
	ArenaAllocator * arena;
	u8 * old_cursor;

	friend struct ArenaAllocator;
};

struct ArenaAllocator final : public Allocator {
	ArenaAllocator() = default;
	ArenaAllocator( void * mem, size_t size );

	void * try_allocate( size_t size, size_t alignment, SourceLocation src = CurrentSourceLocation() );
	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, SourceLocation src = CurrentSourceLocation() );
	void deallocate( void * ptr, SourceLocation src = CurrentSourceLocation() );

	TempAllocator temp();

	void clear();
	void * get_memory();

	float max_utilisation() const;

private:
	u8 * memory;
	u8 * top;
	u8 * cursor;
	u8 * cursor_max;

	u32 num_temp_allocators;

	void * try_temp_allocate( size_t size, size_t alignment, SourceLocation src = CurrentSourceLocation() );
	void * try_temp_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, SourceLocation src = CurrentSourceLocation() );

	friend struct TempAllocator;
};

template< typename... Rest >
char * Allocator::operator()( SourceLocation src, const char * fmt, const Rest & ... rest ) {
	size_t len = ggformat( NULL, 0, fmt, rest... );
	char * buf = AllocMany< char >( this, len + 1, src );
	ggformat( buf, len + 1, fmt, rest... );
	return buf;
}

template< typename... Rest >
char * Allocator::operator()( const char * fmt, const Rest & ... rest ) {
	return ( *this )( CurrentSourceLocation(), fmt, rest... );
}

template< typename... Rest >
Span< char > Allocator::sv( const char * fmt, const Rest & ... rest ) {
	// still need the + 1 because ggformat always null terminates
	Span< char > buf = AllocSpan< char >( this, ggformat( NULL, 0, fmt, rest... ) + 1 );
	ggformat( buf.ptr, buf.n + 1, fmt, rest... );
	return buf.slice( 0, buf.n - 1 ); // trim '\0'
}

char * CopyString( Allocator * a, const char * str, SourceLocation src = CurrentSourceLocation() );

template< typename T >
Span< T > CloneSpan( Allocator * a, Span< T > span, SourceLocation src = CurrentSourceLocation() ) {
	Span< T > copy = AllocSpan< T >( a, span.n, src );
	memcpy( copy.ptr, span.ptr, span.num_bytes() );
	return copy;
}

template< typename T >
Span< T > CloneSpan( Allocator * a, Span< const T > span, SourceLocation src = CurrentSourceLocation() ) {
	Span< T > copy = AllocSpan< T >( a, span.n, src );
	memcpy( copy.ptr, span.ptr, span.num_bytes() );
	return copy;
}
