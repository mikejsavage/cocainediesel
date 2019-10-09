#ifndef __TRACYC_HPP__
#define __TRACYC_HPP__

#include <stddef.h>
#include <stdint.h>

#include "client/TracyCallstack.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TRACY_ENABLE

typedef const void* TracyCZoneCtx;

#define TracyCZone(c,x)
#define TracyCZoneN(c,x,y)
#define TracyCZoneC(c,x,y)
#define TracyCZoneNC(c,x,y,z)
#define TracyCZoneEnd(c)
#define TracyCZoneText(c,x,y)
#define TracyCZoneName(c,x,y)

#define TracyCAlloc(x,y)
#define TracyCFree(x)

#define TracyCFrameMark
#define TracyCFrameMarkNamed(x)
#define TracyCFrameMarkStart(x)
#define TracyCFrameMarkEnd(x)
#define TracyCFrameImage(x,y,z,w,a)

#define TracyCPlot(x,y)
#define TracyCMessage(x,y)
#define TracyCMessageL(x)
#define TracyCMessageC(x,y,z)
#define TracyCMessageLC(x,y)
#define TracyCAppInfo(x,y)

#else

#ifndef TracyConcat
#  define TracyConcat(x,y) TracyConcatIndirect(x,y)
#endif
#ifndef TracyConcatIndirect
#  define TracyConcatIndirect(x,y) x##y
#endif

struct ___tracy_source_location_data
{
    const char* name;
    const char* function;
    const char* file;
    uint32_t line;
    uint32_t color;
};

struct ___tracy_c_zone_context
{
    uint32_t id;
    int active;
};

// Some containers don't support storing const types.
// This struct, as visible to user, is immutable, so treat it as if const was declared here.
typedef /*const*/ struct ___tracy_c_zone_context TracyCZoneCtx;

TracyCZoneCtx ___tracy_emit_zone_begin( const struct ___tracy_source_location_data* srcloc, int active );
TracyCZoneCtx ___tracy_emit_zone_begin_callstack( const struct ___tracy_source_location_data* srcloc, int depth, int active );
void ___tracy_emit_zone_end( TracyCZoneCtx ctx );
void ___tracy_emit_zone_text( TracyCZoneCtx ctx, const char* txt, size_t size );
void ___tracy_emit_zone_name( TracyCZoneCtx ctx, const char* txt, size_t size );

#if defined TRACY_HAS_CALLSTACK && defined TRACY_CALLSTACK
#  define TracyCZone( ctx, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { NULL, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin_callstack( &TracyConcat(__tracy_source_location,__LINE__), TRACY_CALLSTACK, active );
#  define TracyCZoneN( ctx, name, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin_callstack( &TracyConcat(__tracy_source_location,__LINE__), TRACY_CALLSTACK, active );
#  define TracyCZoneC( ctx, color, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { NULL, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, color }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin_callstack( &TracyConcat(__tracy_source_location,__LINE__), TRACY_CALLSTACK, active );
#  define TracyCZoneNC( ctx, name, color, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, color }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin_callstack( &TracyConcat(__tracy_source_location,__LINE__), TRACY_CALLSTACK, active );
#else
#  define TracyCZone( ctx, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { NULL, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin( &TracyConcat(__tracy_source_location,__LINE__), active );
#  define TracyCZoneN( ctx, name, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin( &TracyConcat(__tracy_source_location,__LINE__), active );
#  define TracyCZoneC( ctx, color, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { NULL, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, color }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin( &TracyConcat(__tracy_source_location,__LINE__), active );
#  define TracyCZoneNC( ctx, name, color, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, color }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin( &TracyConcat(__tracy_source_location,__LINE__), active );
#endif

#define TracyCZoneEnd( ctx ) ___tracy_emit_zone_end( ctx );

#define TracyCZoneText( ctx, txt, size ) ___tracy_emit_zone_text( ctx, txt, size );
#define TracyCZoneName( ctx, txt, size ) ___tracy_emit_zone_name( ctx, txt, size );


void ___tracy_emit_memory_alloc( const void* ptr, size_t size );
void ___tracy_emit_memory_alloc_callstack( const void* ptr, size_t size, int depth );
void ___tracy_emit_memory_free( const void* ptr );
void ___tracy_emit_memory_free_callstack( const void* ptr, int depth );

#if defined TRACY_HAS_CALLSTACK && defined TRACY_CALLSTACK
#  define TracyCAlloc( ptr, size ) ___tracy_emit_memory_alloc_callstack( ptr, size, TRACY_CALLSTACK )
#  define TracyCFree( ptr ) ___tracy_emit_memory_alloc_free_callstack( ptr, TRACY_CALLSTACK )
#else
#  define TracyCAlloc( ptr, size ) ___tracy_emit_memory_alloc( ptr, size );
#  define TracyCFree( ptr ) ___tracy_emit_memory_free( ptr );
#endif


void ___tracy_emit_frame_mark( const char* name );
void ___tracy_emit_frame_mark_start( const char* name );
void ___tracy_emit_frame_mark_end( const char* name );
void ___tracy_emit_frame_image( const void* image, uint16_t w, uint16_t h, uint8_t offset, int flip );

#define TracyCFrameMark ___tracy_emit_frame_mark( 0 );
#define TracyCFrameMarkNamed( name ) ___tracy_emit_frame_mark( name );
#define TracyCFrameMarkStart( name ) ___tracy_emit_frame_mark_start( name );
#define TracyCFrameMarkEnd( name ) ___tracy_emit_frame_mark_end( name );
#define TracyCFrameImage( image, width, height, offset, flip ) ___tracy_emit_frame_image( image, width, height, offset, flip );


void ___tracy_emit_plot( const char* name, double val );
void ___tracy_emit_message( const char* txt, size_t size );
void ___tracy_emit_messageL( const char* txt );
void ___tracy_emit_messageC( const char* txt, size_t size, uint32_t color );
void ___tracy_emit_messageLC( const char* txt, uint32_t color );
void ___tracy_emit_message_appinfo( const char* txt, size_t size );

#define TracyCPlot( name, val ) ___tracy_emit_plot( name, val );
#define TracyCMessage( txt, size ) ___tracy_emit_message( txt, size );
#define TracyCMessageL( txt ) ___tracy_emit_messageL( txt );
#define TracyCMessageC( txt, size, color ) ___tracy_emit_messageC( txt, size, color );
#define TracyCMessageLC( txt, color ) ___tracy_emit_messageLC( txt, color );
#define TracyCAppInfo( txt, color ) ___tracy_emit_message_appinfo( txt, color );


#ifdef TRACY_HAS_CALLSTACK
#  define TracyCZoneS( ctx, depth, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { NULL, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin_callstack( &TracyConcat(__tracy_source_location,__LINE__), depth, active );
#  define TracyCZoneNS( ctx, name, depth, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin_callstack( &TracyConcat(__tracy_source_location,__LINE__), depth, active );
#  define TracyCZoneCS( ctx, color, depth, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { NULL, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, color }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin_callstack( &TracyConcat(__tracy_source_location,__LINE__), depth, active );
#  define TracyCZoneNCS( ctx, name, color, depth, active ) static const struct ___tracy_source_location_data TracyConcat(__tracy_source_location,__LINE__) = { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, color }; TracyCZoneCtx ctx = ___tracy_emit_zone_begin_callstack( &TracyConcat(__tracy_source_location,__LINE__), depth, active );

#  define TracyCAllocS( ptr, size, depth ) ___tracy_emit_memory_alloc_callstack( ptr, size, depth )
#  define TracyCFreeS( ptr, depth ) ___tracy_emit_memory_alloc_free_callstack( ptr, depth )
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif
