#if defined _MSC_VER || defined __CYGWIN__ || defined _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# ifndef NOMINMAX
#  define NOMINMAX
# endif
#endif
#ifdef _WIN32
#  include <windows.h>
#else
#  include <pthread.h>
#  include <string.h>
#  include <unistd.h>
#endif

#ifdef __linux__
#   ifndef __ANDROID__
#       include <syscall.h>
#   endif
#   include <fcntl.h>
#endif

#ifdef __MINGW32__
#  define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <stdio.h>

#include "TracySystem.hpp"

#ifdef TRACY_COLLECT_THREAD_NAMES
#  include <atomic>
#  include "TracyAlloc.hpp"
#endif

namespace tracy
{

#ifdef TRACY_COLLECT_THREAD_NAMES
struct ThreadNameData
{
    uint64_t id;
    const char* name;
    ThreadNameData* next;
};
TRACY_API std::atomic<ThreadNameData*>& GetThreadNameData();
TRACY_API void InitRPMallocThread();
#endif

void SetThreadName( std::thread& thread, const char* name )
{
    SetThreadName( thread.native_handle(), name );
}

void SetThreadName( std::thread::native_handle_type handle, const char* name )
{
#if defined _WIN32 && !defined PTW32_VERSION && !defined __WINPTHREADS_VERSION
#  if defined NTDDI_WIN10_RS2 && NTDDI_VERSION >= NTDDI_WIN10_RS2
    wchar_t buf[256];
    mbstowcs( buf, name, 256 );
    SetThreadDescription( static_cast<HANDLE>( handle ), buf );
#  else
    const DWORD MS_VC_EXCEPTION=0x406D1388;
#    pragma pack( push, 8 )
    struct THREADNAME_INFO
    {
        DWORD dwType;
        LPCSTR szName;
        DWORD dwThreadID;
        DWORD dwFlags;
    };
#    pragma pack(pop)

    DWORD ThreadId = GetThreadId( static_cast<HANDLE>( handle ) );
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = ThreadId;
    info.dwFlags = 0;

    __try
    {
        RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
#  endif
#elif defined _GNU_SOURCE && !defined __EMSCRIPTEN__
    {
        const auto sz = strlen( name );
        if( sz <= 15 )
        {
            pthread_setname_np( handle, name );
        }
        else
        {
            char buf[16];
            memcpy( buf, name, 15 );
            buf[15] = '\0';
            pthread_setname_np( handle, buf );
        }
    }
#endif
#ifdef TRACY_COLLECT_THREAD_NAMES
    {
        InitRPMallocThread();
        const auto sz = strlen( name );
        char* buf = (char*)tracy_malloc( sz+1 );
        memcpy( buf, name, sz );
        buf[sz+1] = '\0';
        auto data = (ThreadNameData*)tracy_malloc( sizeof( ThreadNameData ) );
#  ifdef _WIN32
#    if defined PTW32_VERSION
        data->id = pthread_getw32threadid_np( static_cast<pthread_t>( handle ) );
#    elif defined __WINPTHREADS_VERSION
        data->id = GetThreadId( pthread_gethandle( static_cast<pthread_t>( handle ) ) );
#    else
        data->id = GetThreadId( static_cast<HANDLE>( handle ) );
#    endif
#  elif defined __APPLE__
        pthread_threadid_np( handle, &data->id );
#  else
        data->id = (uint64_t)handle;
#  endif
        data->name = buf;
        data->next = GetThreadNameData().load( std::memory_order_relaxed );
        while( !GetThreadNameData().compare_exchange_weak( data->next, data, std::memory_order_release, std::memory_order_relaxed ) ) {}
    }
#endif
}

const char* GetThreadName( uint64_t id )
{
    static char buf[256];
#ifdef TRACY_COLLECT_THREAD_NAMES
    auto ptr = GetThreadNameData().load( std::memory_order_relaxed );
    while( ptr )
    {
        if( ptr->id == id )
        {
            return ptr->name;
        }
        ptr = ptr->next;
    }
#else
#  ifdef _WIN32
#    if defined NTDDI_WIN10_RS2 && NTDDI_VERSION >= NTDDI_WIN10_RS2
    auto hnd = OpenThread( THREAD_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)id );
    if( hnd != 0 )
    {
        PWSTR tmp;
        GetThreadDescription( hnd, &tmp );
        auto ret = wcstombs( buf, tmp, 256 );
        CloseHandle( hnd );
        if( ret != 0 )
        {
            return buf;
        }
    }
#    endif
#  elif defined __GLIBC__ && !defined __ANDROID__ && !defined __EMSCRIPTEN__
    if( pthread_getname_np( (pthread_t)id, buf, 256 ) == 0 )
    {
        return buf;
    }
#  elif defined __linux__
    int cs, fd;
    char path[32];
#   ifdef __ANDROID__
    int tid = gettid();
#   else
    int tid = (int) syscall( SYS_gettid );
#   endif
    snprintf( path, sizeof( path ), "/proc/self/task/%d/comm", tid );
    sprintf( buf, "%" PRIu64, id );
#   ifndef __ANDROID__
    pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs );
#   endif
    if ( ( fd = open( path, O_RDONLY ) ) > 0) {
        int len = read( fd, buf, 255 );
        if( len > 0 )
        {
            buf[len] = 0;
            if( len > 1 && buf[len-1] == '\n' )
            {
                buf[len-1] = 0;
            }
        }
        close( fd );
    }
#   ifndef __ANDROID__
    pthread_setcancelstate( cs, 0 );
#   endif
    return buf;
#  endif
#endif
    sprintf( buf, "%" PRIu64, id );
    return buf;
}

}
