#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/span2d.h"
#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_constants.h"
#include "qcommon/array.h"

static StreamingBuffer decals_buffer;
static GPUBuffer decal_tiles_buffer;
static StreamingBuffer dlights_buffer;
static GPUBuffer dlight_tiles_buffer;
static GPUBuffer dynamic_count;

// gets copied directly to GPU so packing order is important
struct Decal {
	Vec3 origin_normal; // floor( origin ) + ( normal * 0.49 + 0.5 )
	// NOTE(msc): normal can't go to 0.0 or 1.0
	float radius_angle; // floor( radius ) + ( angle / 2 / PI )
	Vec4 color_uvwh_height; // vec4( u + layer, v + floor( r * 255 ) + floor( height ) * 256, w + floor( g * 255 ), h + floor( b * 255 ) )
	// NOTE(msc): uvwh should all be < 1.0
};

struct PersistentDecal {
	Decal decal;
	s64 spawn_time;
	s64 duration;
};

STATIC_ASSERT( sizeof( Decal ) == 2 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( Decal ) % alignof( Decal ) == 0 );

// gets copied directly to GPU so packing order is important
struct DynamicLight {
	Vec3 origin_color; // floor( origin ) + ( color * 0.9 )
	float radius;
};

struct PersistentDynamicLight {
	DynamicLight dlight;
	float start_intensity;
	s64 spawn_time;
	s64 duration;
};

STATIC_ASSERT( sizeof( DynamicLight ) == 1 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( DynamicLight ) % alignof( DynamicLight ) == 0 );

static constexpr u32 MAX_DECALS = 100000;
static constexpr u32 MAX_DLIGHTS = 100000;

static Decal decals[ MAX_DECALS ];
static u32 num_decals;

static DynamicLight dlights[ MAX_DLIGHTS ];
static u32 num_dlights;

static PersistentDecal persistent_decals[ MAX_DECALS ];
static u32 num_persistent_decals;

static PersistentDynamicLight persistent_dlights[ MAX_DLIGHTS ];
static u32 num_persistent_dlights;

struct GPUDecalTile {
	u32 decals[ FORWARD_PLUS_TILE_CAPACITY ];
};

struct GPUDynamicLightTile {
	u32 dlights[ FORWARD_PLUS_TILE_CAPACITY ];
};

struct GPUDynamicCount {
	u32 decal_count;
	u32 dlight_count;
};

void InitDecals() {
	ResetDecals();

	decals_buffer = NewStreamingBuffer( sizeof( decals ), "Decals" );
	dlights_buffer = NewStreamingBuffer( sizeof( dlights ), "Dynamic lights" );
	decal_tiles_buffer = { };
	dlight_tiles_buffer = { };
	dynamic_count = { };
}

void ResetDecals() {
	num_decals = 0;
	num_dlights = 0;

	num_persistent_decals = 0;
	num_persistent_dlights = 0;
}

void ShutdownDecals() {
	DeferDeleteStreamingBuffer( decals_buffer );
	DeferDeleteGPUBuffer( decal_tiles_buffer );
	DeferDeleteStreamingBuffer( dlights_buffer );
	DeferDeleteGPUBuffer( dlight_tiles_buffer );
	DeferDeleteGPUBuffer( dynamic_count );
}

void DrawDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color, float height ) {
	if( num_decals == ARRAY_COUNT( decals ) )
		return;

	Decal * decal = &decals[ num_decals ];

	Vec4 uvwh;
	if( !TryFindDecal( name, &uvwh ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	Vec3 c = Floor( color.xyz() * 255.0f );
	c.x += floorf( height ) * 256.0f;

	decal->origin_normal = Floor( origin ) + ( normal * 0.49f + 0.5f );
	decal->radius_angle = floorf( radius ) + angle / 2.0f / PI;
	decal->color_uvwh_height = Vec4( uvwh.x, uvwh.y + c.x, uvwh.z + c.y, uvwh.w + c.z );

	num_decals++;
}

void AddPersistentDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color, s64 duration, float height ) {
	if( num_persistent_decals == ARRAY_COUNT( persistent_decals ) )
		return;

	PersistentDecal * decal = &persistent_decals[ num_persistent_decals ];

	Vec4 uvwh;
	if( !TryFindDecal( name, &uvwh ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	Vec3 c = Floor( color.xyz() * 255.0f );
	c.x += floorf( height ) * 256.0f;

	decal->decal.origin_normal = Floor( origin ) + ( normal * 0.4f + 0.5f );
	decal->decal.radius_angle = floorf( radius ) + angle / 2.0f / PI;
	decal->decal.color_uvwh_height = Vec4( uvwh.x, uvwh.y + c.x, uvwh.z + c.y, uvwh.w + c.z );
	decal->spawn_time = cl.serverTime;
	decal->duration = duration;

	num_persistent_decals++;
}

void DrawPersistentDecals() {
	for( u32 i = 0; i < num_persistent_decals; i++ ) {
		if( num_decals == ARRAY_COUNT( decals ) )
			break;

		PersistentDecal * decal = &persistent_decals[ i ];
		if( decal->spawn_time + decal->duration < cl.serverTime ) {
			num_persistent_decals--;
			Swap2( decal, &persistent_decals[ num_persistent_decals ] );
			i--;
			continue;
		}

		decals[ num_decals ] = decal->decal;
		num_decals++;
	}
}

void DrawDynamicLight( Vec3 origin, Vec3 color, float intensity ) {
	if( num_dlights == ARRAY_COUNT( dlights ) )
		return;

	DynamicLight * dlight = &dlights[ num_dlights ];

	dlight->origin_color = Floor( origin ) + color * 0.9f;
	dlight->radius = sqrtf( intensity / DLIGHT_CUTOFF );

	num_dlights++;
}

void AddPersistentDynamicLight( Vec3 origin, Vec3 color, float intensity, s64 duration ) {
	if( num_persistent_dlights == ARRAY_COUNT( persistent_dlights ) )
		return;

	PersistentDynamicLight * dlight = &persistent_dlights[ num_persistent_dlights ];

	dlight->dlight.origin_color = Floor( origin ) + color * 0.9f;
	dlight->dlight.radius = sqrtf( intensity / DLIGHT_CUTOFF );
	dlight->start_intensity = intensity;
	dlight->spawn_time = cl.serverTime;
	dlight->duration = duration;

	num_persistent_dlights++;
}

void DrawPersistentDynamicLights() {
	for( u32 i = 0; i < num_persistent_dlights; i++ ) {
		if( num_dlights == ARRAY_COUNT( dlights ) )
			break;

		PersistentDynamicLight * dlight = &persistent_dlights[ i ];
		if( dlight->spawn_time + dlight->duration < cl.serverTime ) {
			num_persistent_dlights--;
			Swap2( dlight, &persistent_dlights[ num_persistent_dlights ] );
			i--;
			continue;
		}

		// TODO: add better curves maybe
		float fract = float( cl.serverTime - dlight->spawn_time ) / float( dlight->duration );
		float intensity = Lerp( dlight->start_intensity, fract, 0.0f );
		dlight->dlight.radius = sqrtf( intensity / DLIGHT_CUTOFF );
		dlights[ num_dlights ] = dlight->dlight;
		num_dlights++;
	}
}

static u32 PixelsToTiles( u32 pixels ) {
	return ( pixels + FORWARD_PLUS_TILE_SIZE - 1 ) / FORWARD_PLUS_TILE_SIZE;
}

void AllocateDecalBuffers() {
	if( decal_tiles_buffer.buffer != 0 && !frame_static.viewport_resized )
		return;

	TracyZoneScopedN( "Reallocate tile buffers" );

	DeleteGPUBuffer( decal_tiles_buffer );
	DeleteGPUBuffer( dlight_tiles_buffer );
	DeleteGPUBuffer( dynamic_count );

	u32 rows = PixelsToTiles( frame_static.viewport_height );
	u32 cols = PixelsToTiles( frame_static.viewport_width );

	decal_tiles_buffer = NewGPUBuffer( rows * cols * sizeof( GPUDecalTile ), "Decal tile indices" );
	dlight_tiles_buffer = NewGPUBuffer( rows * cols * sizeof( GPUDynamicLightTile ), "Dynamic light tile indices" );
	dynamic_count = NewGPUBuffer( rows * cols * sizeof( GPUDynamicCount ), "Dynamics tile counts" );
}

void UploadDecalBuffers() {
	TracyZoneScoped;

	u32 rows = PixelsToTiles( frame_static.viewport_height );
	u32 cols = PixelsToTiles( frame_static.viewport_width );

	WriteAndFlushStreamingBuffer( decals_buffer, decals, num_decals );
	WriteAndFlushStreamingBuffer( dlights_buffer, dlights, num_dlights );

	PipelineState pipeline;
	pipeline.pass = frame_static.tile_culling_pass;
	pipeline.shader = &shaders.tile_culling;
	pipeline.bind_streaming_buffer( "b_Decals", decals_buffer );
	pipeline.bind_streaming_buffer( "b_Dlights", dlights_buffer );
	pipeline.bind_buffer( "b_TileCounts", dynamic_count );
	pipeline.bind_buffer( "b_DecalTiles", decal_tiles_buffer );
	pipeline.bind_buffer( "b_DLightTiles", dlight_tiles_buffer );
	pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
	pipeline.bind_uniform( "u_TileCulling", UploadUniformBlock( cols, rows, u32( num_decals ), u32( num_dlights ) ) );
	DispatchCompute( pipeline, ( cols * rows ) / 64 + 1, 1, 1 );

	num_decals = 0;
	num_dlights = 0;
}

void AddDynamicsToPipeline( PipelineState * pipeline ) {
	pipeline->bind_streaming_buffer( "b_Decals", decals_buffer );
	pipeline->bind_buffer( "b_DecalTiles", decal_tiles_buffer );
	pipeline->bind_streaming_buffer( "b_DynamicLights", dlights_buffer );
	pipeline->bind_buffer( "b_DynamicLightTiles", dlight_tiles_buffer );
	pipeline->bind_buffer( "b_DynamicTiles", dynamic_count );
}
