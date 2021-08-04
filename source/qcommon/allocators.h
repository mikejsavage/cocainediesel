// include base.h instead of this

extern Allocator * sys_allocator;

struct ArenaAllocator;
struct TempAllocator final : public Allocator {
	TempAllocator() = default;
	TempAllocator( const TempAllocator & other );
	~TempAllocator();

	void operator=( const TempAllocator & ) = delete;

	void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );
	void deallocate( void * ptr, const char * func, const char * file, int line );

private:
	ArenaAllocator * arena;
	u8 * old_cursor;

	friend struct ArenaAllocator;
};

struct ArenaAllocator final : public Allocator {
	ArenaAllocator() = default;
	ArenaAllocator( void * mem, size_t size );

	void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );
	void deallocate( void * ptr, const char * func, const char * file, int line );

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

	void * try_temp_allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * try_temp_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );

	friend struct TempAllocator;
};

template< typename... Rest >
char * Allocator::operator()( const char * fmt, const Rest & ... rest ) {
	size_t len = ggformat( NULL, 0, fmt, rest... );
	char * buf = ALLOC_MANY( this, char, len + 1 );
	ggformat( buf, len + 1, fmt, rest... );
	return buf;
}

char * CopyString( Allocator * a, const char * str );
