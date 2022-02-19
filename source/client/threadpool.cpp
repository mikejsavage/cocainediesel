#include "qcommon/base.h"
#include "qcommon/threads.h"
#include "client/client.h"
#include "client/threadpool.h"

#include "tracy/Tracy.hpp"

struct Job {
	JobCallback callback;
	void * data;
};

struct Worker {
	Thread * thread;
	ArenaAllocator arena;
};

static Job jobs[ 4096 ];
static Mutex * jobs_mutex;
static Semaphore * jobs_sem;
static Semaphore * completion_sem;
static bool shutting_down;
static size_t jobs_head;
static size_t jobs_not_started;
static size_t jobs_done;

static Worker workers[ 32 ];
static u32 num_workers;

static void ThreadPoolWorker( void * data ) {
#if TRACY_ENABLE
	tracy::SetThreadName( "Thread pool worker" );
#endif

	ArenaAllocator * arena = ( ArenaAllocator * ) data;

	while( true ) {
		Wait( jobs_sem );
		Lock( jobs_mutex );

		if( shutting_down ) {
			Unlock( jobs_mutex );
			break;
		}

		if( jobs_not_started == 0 ) {
			Unlock( jobs_mutex );
			continue;
		}

		Job * job = &jobs[ jobs_head % ARRAY_COUNT( jobs ) ];
		jobs_head++;
		jobs_not_started--;

		Unlock( jobs_mutex );

		{
			TempAllocator temp = arena->temp();
			job->callback( &temp, job->data );
		}

		Lock( jobs_mutex );
		jobs_done++;
		Unlock( jobs_mutex );

		Signal( completion_sem );
	}
}

void InitThreadPool() {
	TracyZoneScoped;

	shutting_down = false;
	jobs_head = 0;
	jobs_not_started = 0;
	jobs_done = 0;
	jobs_mutex = NewMutex();
	jobs_sem = NewSemaphore();
	completion_sem = NewSemaphore();

	num_workers = Min2( GetCoreCount() - 1, u32( ARRAY_COUNT( workers ) ) );

	for( u32 i = 0; i < num_workers; i++ ) {
		constexpr size_t arena_size = 1024 * 1024; // 1MB
		void * arena_memory = ALLOC_SIZE( sys_allocator, arena_size, 16 );
		workers[ i ].arena = ArenaAllocator( arena_memory, arena_size );
		workers[ i ].thread = NewThread( ThreadPoolWorker, &workers[ i ].arena );
	}
}

void ShutdownThreadPool() {
	TracyZoneScoped;

	Lock( jobs_mutex );
	shutting_down = true;
	Unlock( jobs_mutex );

	for( u32 i = 0; i < num_workers; i++ ) {
		Signal( jobs_sem );
	}

	for( u32 i = 0; i < num_workers; i++ ) {
		JoinThread( workers[ i ].thread );
		FREE( sys_allocator, workers[ i ].arena.get_memory() );
	}

	DeleteSemaphore( completion_sem );
	DeleteSemaphore( jobs_sem );
	DeleteMutex( jobs_mutex );
}

void ThreadPoolDo( JobCallback callback, void * data ) {
	TracyZoneScoped;

	Lock( jobs_mutex );

	assert( jobs_not_started < ARRAY_COUNT( jobs ) );

	Job * job = &jobs[ ( jobs_head + jobs_not_started ) % ARRAY_COUNT( jobs ) ];
	job->callback = callback;
	job->data = data;

	jobs_not_started++;

	Unlock( jobs_mutex );
	Signal( jobs_sem );
}

void ParallelFor( void * datum, size_t n, size_t stride, JobCallback callback ) {
	TracyZoneScoped;

	Lock( jobs_mutex );

	assert( n <= ARRAY_COUNT( jobs ) - jobs_not_started );

	for( size_t i = 0; i < n; i++ ) {
		Job * job = &jobs[ ( jobs_head + jobs_not_started ) % ARRAY_COUNT( jobs ) ];
		job->callback = callback;
		job->data = ( ( char * ) datum ) + stride * i;

		jobs_not_started++;
	}

	Unlock( jobs_mutex );
	Signal( jobs_sem, checked_cast< int >( n ) );

	ThreadPoolFinish();
}

void ThreadPoolFinish() {
	TracyZoneScoped;

	Lock( jobs_mutex );

	while( true ) {
		if( jobs_not_started == 0 ) {
			while( jobs_done != jobs_head ) {
				Unlock( jobs_mutex );
				Wait( completion_sem );
				Lock( jobs_mutex );
			}
			break;
		}

		Job * job = &jobs[ jobs_head % ARRAY_COUNT( jobs ) ];
		jobs_head++;
		jobs_not_started--;

		Unlock( jobs_mutex );

		{
			TempAllocator temp = cls.frame_arena.temp();
			job->callback( &temp, job->data );
		}

		Lock( jobs_mutex );
		jobs_done++;
	}

	Unlock( jobs_mutex );
}
