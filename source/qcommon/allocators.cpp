#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/asan.h"
#include "qcommon/string.h"

void * Allocator::allocate( size_t size, size_t alignment, const char * func, const char * file, int line ) {
	void * p = try_allocate( size, alignment, func, file, line );
	if( p == NULL )
		Fatal( "Allocation failed in '%s' (%s:%d)", func, file, line );
	return p;
}

void * Allocator::reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line ) {
	void * new_p = try_reallocate( ptr, current_size, new_size, alignment, func, file, line );
	if( new_p == NULL )
		Fatal( "Reallocation failed in '%s' (%s:%d)", func, file, line );
	return new_p;
}

void * AllocManyHelper( Allocator * a, size_t n, size_t size, size_t alignment, const char * func, const char * file, int line ) {
	if( n != 0 && SIZE_MAX / n < size )
		Fatal( "allocation too large" );
	return a->allocate( n * size, alignment, func, file, line );
}

void * ReallocManyHelper( Allocator * a, void * ptr, size_t current_n, size_t new_n, size_t size, size_t alignment, const char * func, const char * file, int line ) {
	if( SIZE_MAX / new_n < size )
		Fatal( "allocation too large" );
	return a->reallocate( ptr, current_n * size, new_n * size, alignment, func, file, line );
}

/*
 * SystemAllocator
 */

#if PUBLIC_BUILD

struct AllocationTracker {
	void track( void * ptr, const char * func, const char * file, int line, size_t size ) { }
	void untrack( void * ptr, const char * func, const char * file, int line ) { }
};

#else

#include "qcommon/threads.h"
#undef min
#undef max
#include <unordered_map>

struct AllocationTracker {
	NONCOPYABLE( AllocationTracker );

	struct AllocInfo {
		const char * func;
		const char * file;
		int line;
		size_t size;
	};

	std::unordered_map< void *, AllocInfo > allocations;
	Mutex * mutex;

	AllocationTracker() {
		mutex = NewMutex();
	}

	~AllocationTracker() {
		String< 2048 > msg( "Memory leaks:" );

		size_t leaks = 0;
		for( auto & alloc : allocations ) {
			const AllocInfo & info = alloc.second;
			msg.append( "\n{} bytes at {} ({}:{})", info.size, info.func, info.file, info.line );

			leaks++;
			if( leaks == 5 )
				break;
		}

		if( leaks < allocations.size() ) {
			msg.append( "\n...and {} more", allocations.size() - leaks );
		}

		if( leaks > 0 ) {
			Fatal( "%s", msg.c_str() );
		}

		DeleteMutex( mutex );
	}

	void track( void * ptr, const char * func, const char * file, int line, size_t size ) {
		if( ptr == NULL )
			return;
		Lock( mutex );
		allocations[ ptr ] = { func, file, line, size };
		Unlock( mutex );
	}

	void untrack( void * ptr, const char * func, const char * file, int line ) {
		if( ptr == NULL )
			return;
		Lock( mutex );
		if( allocations.erase( ptr ) == 0 )
			Fatal( "Stray free in '%s' (%s:%d)", func, file, line );
		Unlock( mutex );
	};
};

#endif

struct SystemAllocator final : public Allocator {
	AllocationTracker tracker;

	void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line ) {
		/*
		 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/malloc?view=vs-2015
		 * https://www.gnu.org/software/libc/manual/html_node/Aligned-Memory-Blocks.html
		 *
		 * "In Visual C++, the fundamental alignment is the alignment that's required for a
		 * double, or 8 bytes. In code that targets 64-bit platforms, it's 16 bytes."
		 *
		 * "The address of a block returned by malloc or realloc in GNU systems is always a
		 * multiple of eight (or sixteen on 64-bit systems)."
		 */

		assert( alignment <= 16 );
		void * ptr = malloc( size );
		TracyCAlloc( ptr, size );
		tracker.track( ptr, func, file, line, size );
		return ptr;
	}

	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line ) {
		assert( alignment <= 16 );

		TracyCFree( ptr );
		tracker.untrack( ptr, func, file, line );

		void * new_ptr = realloc( ptr, new_size );
		if( new_ptr == NULL ) {
			TracyCAlloc( ptr, new_size );
			tracker.track( ptr, func, file, line, new_size );
			return NULL;
		}

		TracyCAlloc( new_ptr, new_size );
		tracker.track( new_ptr, func, file, line, new_size );

		return new_ptr;
	}

	void deallocate( void * ptr, const char * func, const char * file, int line ) {
		TracyCFree( ptr );
		tracker.untrack( ptr, func, file, line );
		free( ptr );
	}
};

/*
 * ArenaAllocator
 */

TempAllocator::TempAllocator( const TempAllocator & other ) {
	arena = other.arena;
	old_cursor = other.old_cursor;
	arena->num_temp_allocators++;
}

TempAllocator::~TempAllocator() {
	arena->cursor = old_cursor;
	arena->num_temp_allocators--;
	ASAN_POISON_MEMORY_REGION( arena->cursor, arena->top - arena->cursor );
}

void * TempAllocator::try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line ) {
	return arena->try_temp_allocate( size, alignment, func, file, line );
}

void * TempAllocator::try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line ) {
	return arena->try_temp_reallocate( ptr, current_size, new_size, alignment, func, file, line );
}

void TempAllocator::deallocate( void * ptr, const char * func, const char * file, int line ) { }

ArenaAllocator::ArenaAllocator( void * mem, size_t size ) {
	ASAN_POISON_MEMORY_REGION( mem, size );
	memory = ( u8 * ) mem;
	top = memory + size;
	cursor = memory;
	cursor_max = cursor;
	num_temp_allocators = 0;
}

void * ArenaAllocator::try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line ) {
	assert( num_temp_allocators == 0 );
	return try_temp_allocate( size, alignment, func, file, line );
}

void * ArenaAllocator::try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line ) {
	assert( num_temp_allocators == 0 );
	return try_temp_reallocate( ptr, current_size, new_size, alignment, func, file, line );
}

void * ArenaAllocator::try_temp_allocate( size_t size, size_t alignment, const char * func, const char * file, int line ) {
	assert( IsPowerOf2( alignment ) );
	u8 * aligned = ( u8 * ) ( size_t( cursor + alignment - 1 ) & ~( alignment - 1 ) );
	if( aligned + size > top )
		return NULL;
	ASAN_UNPOISON_MEMORY_REGION( aligned, size );
	cursor = aligned + size;
	cursor_max = Max2( cursor, cursor_max );
	return aligned;
}

void * ArenaAllocator::try_temp_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line ) {
	if( ptr == NULL )
		return try_temp_allocate( new_size, alignment, func, file, line );

	if( ptr == cursor - current_size && size_t( ptr ) % alignment == 0 ) {
		assert( size_t( ptr ) % alignment == 0 );
		u8 * new_cursor = cursor - current_size + new_size;
		if( new_cursor > top )
			return NULL;

		ASAN_UNPOISON_MEMORY_REGION( ptr, new_size );
		if( new_cursor < cursor ) {
			ASAN_POISON_MEMORY_REGION( new_cursor, cursor - new_cursor );
		}

		cursor = new_cursor;
		cursor_max = Max2( cursor, cursor_max );
		return ptr;
	}

	void * mem = try_temp_allocate( new_size, alignment, func, file, line );
	if( mem == NULL )
		return NULL;
	memcpy( mem, ptr, current_size );
	return mem;
}

void ArenaAllocator::deallocate( void * ptr, const char * func, const char * file, int line ) { }

TempAllocator ArenaAllocator::temp() {
	num_temp_allocators++;

	TempAllocator t;
	t.arena = this;
	t.old_cursor = cursor;
	return t;
}

void ArenaAllocator::clear() {
	assert( num_temp_allocators == 0 );
	ASAN_POISON_MEMORY_REGION( memory, top - memory );
	cursor = memory;
	cursor_max = cursor;
}

void * ArenaAllocator::get_memory() {
	return memory;
}

float ArenaAllocator::max_utilisation() const {
	return float( cursor_max - cursor ) / float( top - cursor );
}

static SystemAllocator sys_allocator_;
Allocator * sys_allocator = &sys_allocator_;
