#ifndef __TRACYMUTEX_HPP__
#define __TRACYMUTEX_HPP__

namespace tracy
{

struct TracyMutex {
	TracyMutex();
	~TracyMutex();

	void lock();
	bool try_lock();
	void unlock();

	alignas(16) char opaque[64];
};

}

#endif
