#include "qcommon/types.h"
#include "qcommon/qcommon.h"

#if RELEASE_BUILD

struct AllocationTracker {
	NONCOPYABLE( AllocationTracker );
	void track( void * ptr, const char * func, const char * file, int line ) { }
	void untrack( void * ptr ) { };
};

#else

#include "qcommon/qthreads.h"
#undef min
#undef max
#include <unordered_map>

struct AllocationTracker {
	NONCOPYABLE( AllocationTracker );

	struct AllocInfo {
		const char * func;
		const char * file;
		int line;
	};

	std::unordered_map< void *, AllocInfo > allocations;
	qmutex_t * mutex;

	AllocationTracker() {
		mutex = QMutex_Create();
	}

	~AllocationTracker() {
		for( auto & alloc : allocations ) {
			const AllocInfo & info = alloc.second;
			Com_Printf( "Leaked allocation in '%s' (%s:%d)\n", info.func, info.file, info.line );
		}
		assert( allocations.empty() );
		QMutex_Destroy( &mutex );
	}

	void track( void * ptr, const char * func, const char * file, int line ) {
		QMutex_Lock( mutex );
		allocations[ ptr ] = { func, file, line };
		QMutex_Unlock( mutex );
	}

	void untrack( void * ptr ) {
		QMutex_Lock( mutex );
		if( allocations.erase( ptr ) == 0 )
			Sys_Error( "Stray free" );
		QMutex_Unlock( mutex );
	};
};

#endif

void * Allocator::allocate( size_t size, size_t alignment, const char * func, const char * file, int line ) {
	void * p = try_allocate( size, alignment, func, file, line );
	if( p == NULL )
		Sys_Error( "Allocation failed in '%s' (%s:%d)", func, file, line );
	return p;
}

void * Allocator::reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment ) {
	void * new_p = try_reallocate( ptr, current_size, new_size, alignment );
	if( new_p == NULL )
		Sys_Error( "Reallocation failed" );
	return new_p;
}

struct SystemAllocator final : public Allocator {
	SystemAllocator() { }
	NONCOPYABLE( SystemAllocator );

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
		tracker.track( ptr, func, file, line );
		return ptr;
	}

	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment ) {
		assert( alignment <= 16 );
		return realloc( ptr, new_size );
	}

	void deallocate( void * ptr ) {
		free( ptr );
		tracker.untrack( ptr );
	}
};
