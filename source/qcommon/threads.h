#pragma once

#include "qcommon/opaque.h"

struct Thread;
struct Mutex;
struct Semaphore;

template<> inline constexpr size_t OpaqueSize< Thread > = 16;
template<> inline constexpr size_t OpaqueSize< Mutex > = 64;
template<> inline constexpr size_t OpaqueSize< Semaphore > = 32;
template<> inline constexpr bool OpaqueCopyable< Mutex > = false;
template<> inline constexpr bool OpaqueCopyable< Semaphore > = false;

Opaque< Thread > NewThread( void ( *callback )( void * ), void * data = NULL );
void JoinThread( Opaque< Thread > thread );

void InitMutex( Opaque< Mutex > * mutex );
void DeleteMutex( Opaque< Mutex > * mutex );
void Lock( Opaque< Mutex > * mutex );
void Unlock( Opaque< Mutex > * mutex );

void InitSemaphore( Opaque< Semaphore > * sem );
void DeleteSemaphore( Opaque< Semaphore > * sem );
void Signal( Opaque< Semaphore > * sem, int n = 1 );
void Wait( Opaque< Semaphore > * sem );

u32 GetCoreCount();
