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
                        printf( "Leaked allocation in '%s' (%s:%d)\n", info.func, info.file, info.line );
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
                allocations.erase( ptr );
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

void * Allocator::reallocate( void * p, size_t current_size, size_t new_size ) {
	void * new_p = try_reallocate( p, current_size, new_size );
	if( new_p == NULL )
		Sys_Error( "Reallocation failed" );
	return new_p;
}

struct SystemAllocator final : public Allocator {
        SystemAllocator() { }
        NONCOPYABLE( SystemAllocator );

        AllocationTracker tracker;

        void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line ) {
                assert( alignment <= 16 );
                void * p = malloc( size );
                tracker.track( p, func, file, line );
                return p;
        }

        void * try_reallocate( void * ptr, size_t current_size, size_t new_size ) {
                return realloc( ptr, new_size );
        }

        void deallocate( void * ptr ) {
                free( ptr );
                tracker.untrack( ptr );
        }
};
