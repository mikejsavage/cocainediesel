#ifndef __TRACYLOCK_HPP__
#define __TRACYLOCK_HPP__

#include <atomic>
#include <limits>

#include "../common/TracySystem.hpp"
#include "../common/TracyAlign.hpp"
#include "TracyProfiler.hpp"

namespace tracy
{

template<class T>
class Lockable
{
public:
    tracy_force_inline Lockable( const SourceLocationData* srcloc )
        : m_id( GetLockCounter().fetch_add( 1, std::memory_order_relaxed ) )
#ifdef TRACY_ON_DEMAND
        , m_lockCount( 0 )
        , m_active( false )
#endif
    {
        assert( m_id != std::numeric_limits<uint32_t>::max() );

        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin( magic );
        MemWrite( &item->hdr.type, QueueType::LockAnnounce );
        MemWrite( &item->lockAnnounce.id, m_id );
        MemWrite( &item->lockAnnounce.time, Profiler::GetTime() );
        MemWrite( &item->lockAnnounce.lckloc, (uint64_t)srcloc );
        MemWrite( &item->lockAnnounce.type, LockType::Lockable );

#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif

        tail.store( magic + 1, std::memory_order_release );
    }

    Lockable( const Lockable& ) = delete;
    Lockable& operator=( const Lockable& ) = delete;

    ~Lockable()
    {
        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin( magic );
        MemWrite( &item->hdr.type, QueueType::LockTerminate );
        MemWrite( &item->lockTerminate.id, m_id );
        MemWrite( &item->lockTerminate.time, Profiler::GetTime() );
        MemWrite( &item->lockTerminate.type, LockType::Lockable );

#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif

        tail.store( magic + 1, std::memory_order_release );
    }

    tracy_force_inline void lock()
    {
#ifdef TRACY_ON_DEMAND
        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue )
        {
            m_lockable.lock();
            return;
        }
#endif
        {
            Magic magic;
            auto token = GetToken();
            auto& tail = token->get_tail_index();
            auto item = token->enqueue_begin( magic );
            MemWrite( &item->hdr.type, QueueType::LockWait );
            MemWrite( &item->lockWait.id, m_id );
            MemWrite( &item->lockWait.time, Profiler::GetTime() );
            MemWrite( &item->lockWait.type, LockType::Lockable );
            tail.store( magic + 1, std::memory_order_release );
        }

        m_lockable.lock();

        {
            Magic magic;
            auto token = GetToken();
            auto& tail = token->get_tail_index();
            auto item = token->enqueue_begin( magic );
            MemWrite( &item->hdr.type, QueueType::LockObtain );
            MemWrite( &item->lockObtain.id, m_id );
            MemWrite( &item->lockObtain.time, Profiler::GetTime() );
            tail.store( magic + 1, std::memory_order_release );
        }
    }

    tracy_force_inline void unlock()
    {
        m_lockable.unlock();

#ifdef TRACY_ON_DEMAND
        m_lockCount.fetch_sub( 1, std::memory_order_relaxed );
        if( !m_active.load( std::memory_order_relaxed ) ) return;
        if( !GetProfiler().IsConnected() )
        {
            m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin( magic );
        MemWrite( &item->hdr.type, QueueType::LockRelease );
        MemWrite( &item->lockRelease.id, m_id );
        MemWrite( &item->lockRelease.time, Profiler::GetTime() );
        tail.store( magic + 1, std::memory_order_release );
    }

    tracy_force_inline bool try_lock()
    {
        const auto ret = m_lockable.try_lock();

#ifdef TRACY_ON_DEMAND
        if( !ret ) return ret;

        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue ) return ret;
#endif

        if( ret )
        {
            Magic magic;
            auto token = GetToken();
            auto& tail = token->get_tail_index();
            auto item = token->enqueue_begin( magic );
            MemWrite( &item->hdr.type, QueueType::LockObtain );
            MemWrite( &item->lockObtain.id, m_id );
            MemWrite( &item->lockObtain.time, Profiler::GetTime() );
            tail.store( magic + 1, std::memory_order_release );
        }

        return ret;
    }

    tracy_force_inline void Mark( const SourceLocationData* srcloc )
    {
#ifdef TRACY_ON_DEMAND
        const auto active = m_active.load( std::memory_order_relaxed );
        if( !active ) return;
        const auto connected = GetProfiler().IsConnected();
        if( !connected )
        {
            if( active ) m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin( magic );
        MemWrite( &item->hdr.type, QueueType::LockMark );
        MemWrite( &item->lockMark.id, m_id );
        MemWrite( &item->lockMark.srcloc, (uint64_t)srcloc );
        tail.store( magic + 1, std::memory_order_release );
    }

private:
    T m_lockable;
    uint32_t m_id;

#ifdef TRACY_ON_DEMAND
    std::atomic<uint32_t> m_lockCount;
    std::atomic<bool> m_active;
#endif
};


template<class T>
class SharedLockable
{
public:
    tracy_force_inline SharedLockable( const SourceLocationData* srcloc )
        : m_id( GetLockCounter().fetch_add( 1, std::memory_order_relaxed ) )
#ifdef TRACY_ON_DEMAND
        , m_lockCount( 0 )
        , m_active( false )
#endif
    {
        assert( m_id != std::numeric_limits<uint32_t>::max() );

        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin( magic );
        MemWrite( &item->hdr.type, QueueType::LockAnnounce );
        MemWrite( &item->lockAnnounce.id, m_id );
        MemWrite( &item->lockAnnounce.time, Profiler::GetTime() );
        MemWrite( &item->lockAnnounce.lckloc, (uint64_t)srcloc );
        MemWrite( &item->lockAnnounce.type, LockType::SharedLockable );

#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif

        tail.store( magic + 1, std::memory_order_release );
    }

    SharedLockable( const SharedLockable& ) = delete;
    SharedLockable& operator=( const SharedLockable& ) = delete;

    ~SharedLockable()
    {
        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin( magic );
        MemWrite( &item->hdr.type, QueueType::LockTerminate );
        MemWrite( &item->lockTerminate.id, m_id );
        MemWrite( &item->lockTerminate.time, Profiler::GetTime() );
        MemWrite( &item->lockTerminate.type, LockType::SharedLockable );

#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem( *item );
#endif

        tail.store( magic + 1, std::memory_order_release );
    }

    tracy_force_inline void lock()
    {
#ifdef TRACY_ON_DEMAND
        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue )
        {
            m_lockable.lock();
            return;
        }
#endif
        {
            Magic magic;
            auto token = GetToken();
            auto& tail = token->get_tail_index();
            auto item = token->enqueue_begin( magic );
            MemWrite( &item->hdr.type, QueueType::LockWait );
            MemWrite( &item->lockWait.id, m_id );
            MemWrite( &item->lockWait.time, Profiler::GetTime() );
            MemWrite( &item->lockWait.type, LockType::SharedLockable );
            tail.store( magic + 1, std::memory_order_release );
        }

        m_lockable.lock();

        {
            Magic magic;
            auto token = GetToken();
            auto& tail = token->get_tail_index();
            auto item = token->enqueue_begin( magic );
            MemWrite( &item->hdr.type, QueueType::LockObtain );
            MemWrite( &item->lockObtain.id, m_id );
            MemWrite( &item->lockObtain.time, Profiler::GetTime() );
            tail.store( magic + 1, std::memory_order_release );
        }
    }

    tracy_force_inline void unlock()
    {
        m_lockable.unlock();

#ifdef TRACY_ON_DEMAND
        m_lockCount.fetch_sub( 1, std::memory_order_relaxed );
        if( !m_active.load( std::memory_order_relaxed ) ) return;
        if( !GetProfiler().IsConnected() )
        {
            m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin( magic );
        MemWrite( &item->hdr.type, QueueType::LockRelease );
        MemWrite( &item->lockRelease.id, m_id );
        MemWrite( &item->lockRelease.time, Profiler::GetTime() );
        tail.store( magic + 1, std::memory_order_release );
    }

    tracy_force_inline bool try_lock()
    {
        const auto ret = m_lockable.try_lock();

#ifdef TRACY_ON_DEMAND
        if( !ret ) return ret;

        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue ) return ret;
#endif

        if( ret )
        {
            Magic magic;
            auto token = GetToken();
            auto& tail = token->get_tail_index();
            auto item = token->enqueue_begin( magic );
            MemWrite( &item->hdr.type, QueueType::LockObtain );
            MemWrite( &item->lockObtain.id, m_id );
            MemWrite( &item->lockObtain.time, Profiler::GetTime() );
            tail.store( magic + 1, std::memory_order_release );
        }

        return ret;
    }

    tracy_force_inline void lock_shared()
    {
#ifdef TRACY_ON_DEMAND
        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue )
        {
            m_lockable.lock_shared();
            return;
        }
#endif
        {
            Magic magic;
            auto token = GetToken();
            auto& tail = token->get_tail_index();
            auto item = token->enqueue_begin( magic );
            MemWrite( &item->hdr.type, QueueType::LockSharedWait );
            MemWrite( &item->lockWait.id, m_id );
            MemWrite( &item->lockWait.time, Profiler::GetTime() );
            MemWrite( &item->lockWait.type, LockType::SharedLockable );
            tail.store( magic + 1, std::memory_order_release );
        }

        m_lockable.lock_shared();

        {
            Magic magic;
            auto token = GetToken();
            auto& tail = token->get_tail_index();
            auto item = token->enqueue_begin( magic );
            MemWrite( &item->hdr.type, QueueType::LockSharedObtain );
            MemWrite( &item->lockObtain.id, m_id );
            MemWrite( &item->lockObtain.time, Profiler::GetTime() );
            tail.store( magic + 1, std::memory_order_release );
        }
    }

    tracy_force_inline void unlock_shared()
    {
        m_lockable.unlock_shared();

#ifdef TRACY_ON_DEMAND
        m_lockCount.fetch_sub( 1, std::memory_order_relaxed );
        if( !m_active.load( std::memory_order_relaxed ) ) return;
        if( !GetProfiler().IsConnected() )
        {
            m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin( magic );
        MemWrite( &item->hdr.type, QueueType::LockSharedRelease );
        MemWrite( &item->lockRelease.id, m_id );
        MemWrite( &item->lockRelease.time, Profiler::GetTime() );
        tail.store( magic + 1, std::memory_order_release );
    }

    tracy_force_inline bool try_lock_shared()
    {
        const auto ret = m_lockable.try_lock_shared();

#ifdef TRACY_ON_DEMAND
        if( !ret ) return ret;

        bool queue = false;
        const auto locks = m_lockCount.fetch_add( 1, std::memory_order_relaxed );
        const auto active = m_active.load( std::memory_order_relaxed );
        if( locks == 0 || active )
        {
            const bool connected = GetProfiler().IsConnected();
            if( active != connected ) m_active.store( connected, std::memory_order_relaxed );
            if( connected ) queue = true;
        }
        if( !queue ) return ret;
#endif

        if( ret )
        {
            Magic magic;
            auto token = GetToken();
            auto& tail = token->get_tail_index();
            auto item = token->enqueue_begin( magic );
            MemWrite( &item->hdr.type, QueueType::LockSharedObtain );
            MemWrite( &item->lockObtain.id, m_id );
            MemWrite( &item->lockObtain.time, Profiler::GetTime() );
            tail.store( magic + 1, std::memory_order_release );
        }

        return ret;
    }

    tracy_force_inline void Mark( const SourceLocationData* srcloc )
    {
#ifdef TRACY_ON_DEMAND
        const auto active = m_active.load( std::memory_order_relaxed );
        if( !active ) return;
        const auto connected = GetProfiler().IsConnected();
        if( !connected )
        {
            if( active ) m_active.store( false, std::memory_order_relaxed );
            return;
        }
#endif

        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin( magic );
        MemWrite( &item->hdr.type, QueueType::LockMark );
        MemWrite( &item->lockMark.id, m_id );
        MemWrite( &item->lockMark.srcloc, (uint64_t)srcloc );
        tail.store( magic + 1, std::memory_order_release );
    }

private:
    T m_lockable;
    uint32_t m_id;

#ifdef TRACY_ON_DEMAND
    std::atomic<uint32_t> m_lockCount;
    std::atomic<bool> m_active;
#endif
};


};

#endif
