#include <new>

#include "TracyMutex.hpp"

#if defined _MSC_VER

#include <shared_mutex>
using MutexImpl = std::shared_mutex;

#elif defined __CYGWIN__

#include "tracy_benaphore.h"
using MutexImpl = NonRecursiveBenaphore;

#else

#include <mutex>
using MutexImpl = std::mutex;

#endif

namespace tracy
{

static_assert( sizeof( MutexImpl ) <= sizeof( TracyMutex ), "TracyMutex isn't big enough" );
static_assert( alignof( MutexImpl ) <= alignof( TracyMutex ), "TracyMutex isn't aligned enough" );

MutexImpl* GetImpl( char* opaque ) {
	return (MutexImpl*)opaque;
}

TracyMutex::TracyMutex() {
	new ( GetImpl( opaque ) ) MutexImpl();
}

TracyMutex::~TracyMutex() {
	GetImpl( opaque )->~MutexImpl();
}

void TracyMutex::lock() {
	GetImpl( opaque )->lock();
}

bool TracyMutex::try_lock() {
	return GetImpl( opaque )->try_lock();
}

void TracyMutex::unlock() {
	GetImpl( opaque )->unlock();
}

}
