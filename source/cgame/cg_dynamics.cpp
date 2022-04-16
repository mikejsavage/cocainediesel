#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/span2d.h"
#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"
#include "qcommon/array.h"

static GPUBuffer decal_tiles_buffer;
static GPUBuffer decals_buffer;
static GPUBuffer dlight_tiles_buffer;
static GPUBuffer dlights_buffer;
static GPUBuffer dynamic_count;

static u32 last_viewport_width, last_viewport_height;

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
static constexpr u32 MAX_DECALS_PER_TILE = 50;

static constexpr u32 MAX_DLIGHTS = 100000;
static constexpr u32 MAX_DLIGHTS_PER_TILE = 50;

static constexpr u32 MAX_DYNAMICS_PER_SET = 500;

static Decal decals[ MAX_DECALS ];
static u32 num_decals;

static DynamicLight dlights[ MAX_DLIGHTS ];
static u32 num_dlights;

static PersistentDecal persistent_decals[ MAX_DECALS ];
static u32 num_persistent_decals;

static PersistentDynamicLight persistent_dlights[ MAX_DLIGHTS ];
static u32 num_persistent_dlights;

struct DecalTile {
	u32 decals[ MAX_DECALS_PER_TILE ];
};

struct DynamicLightTile {
	u32 dlights[ MAX_DLIGHTS_PER_TILE ];
};

static Span2D< DecalTile > gpu_decal_tiles;
static Span2D< DynamicLightTile > gpu_dlight_tiles;

struct DynamicCount {
	u32 decal_count;
	u32 dlight_count;
};

static Span2D< DynamicCount > gpu_dynamic_counts;

void InitDecals() {
	num_persistent_decals = 0;
	num_persistent_dlights = 0;

	last_viewport_width = U32_MAX;
	last_viewport_height = U32_MAX;

	decals_buffer = NewGPUBuffer( sizeof( decals ), "Decals" );
	dlights_buffer = NewGPUBuffer( sizeof( dlights ), "Dynamic lights" );
}

void ShutdownDecals() {
	FREE( sys_allocator, gpu_decal_tiles.ptr );
	gpu_decal_tiles.ptr = NULL;
	DeferDeleteGPUBuffer( decal_tiles_buffer );
	DeferDeleteGPUBuffer( decals_buffer );

	FREE( sys_allocator, gpu_dlight_tiles.ptr );
	gpu_dlight_tiles.ptr = NULL;
	DeferDeleteGPUBuffer( dlight_tiles_buffer );
	DeferDeleteGPUBuffer( dlights_buffer );

	FREE( sys_allocator, gpu_dynamic_counts.ptr );
	gpu_dynamic_counts.ptr = NULL;
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

void DrawDynamicLight( Vec3 origin, Vec4 color, float intensity ) {
	if( num_dlights == ARRAY_COUNT( dlights ) )
		return;

	DynamicLight * dlight = &dlights[ num_dlights ];

	dlight->origin_color = Floor( origin ) + color.xyz() * 0.9f;
	dlight->radius = sqrtf( intensity / DLIGHT_CUTOFF );

	num_dlights++;
}

void AddPersistentDynamicLight( Vec3 origin, Vec4 color, float intensity, s64 duration ) {
	if( num_persistent_dlights == ARRAY_COUNT( persistent_dlights ) )
		return;

	PersistentDynamicLight * dlight = &persistent_dlights[ num_persistent_dlights ];

	dlight->dlight.origin_color = Floor( origin ) + color.xyz() * 0.9f;
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

void AllocateDecalBuffers() {
	u32 rows = ( frame_static.viewport_height + TILE_SIZE - 1 ) / TILE_SIZE;
	u32 cols = ( frame_static.viewport_width + TILE_SIZE - 1 ) / TILE_SIZE;

	if( frame_static.viewport_width != last_viewport_width || frame_static.viewport_height != last_viewport_height ) {
		TracyZoneScopedN( "Reallocate TBOs" );

		FREE( sys_allocator, gpu_decal_tiles.ptr );
		gpu_decal_tiles = ALLOC_SPAN2D( sys_allocator, DecalTile, cols, rows );
		decal_tiles_buffer = NewGPUBuffer( gpu_decal_tiles.num_bytes(), "Decal tile indices" );

		FREE( sys_allocator, gpu_dlight_tiles.ptr );
		gpu_dlight_tiles = ALLOC_SPAN2D( sys_allocator, DynamicLightTile, cols, rows );
		dlight_tiles_buffer = NewGPUBuffer( gpu_dlight_tiles.num_bytes(), "Dynamic light tile indices" );

		FREE( sys_allocator, gpu_dynamic_counts.ptr );
		gpu_dynamic_counts = ALLOC_SPAN2D( sys_allocator, DynamicCount, cols, rows );
		dynamic_count = NewGPUBuffer( gpu_dynamic_counts.num_bytes(), "Dynamics tile counts" );

		last_viewport_width = frame_static.viewport_width;
		last_viewport_height = frame_static.viewport_height;
	}
}

void UploadDecalBuffers() {
	TracyZoneScoped;

	u32 rows = ( frame_static.viewport_height + TILE_SIZE - 1 ) / TILE_SIZE;
	u32 cols = ( frame_static.viewport_width + TILE_SIZE - 1 ) / TILE_SIZE;

	{
		TracyZoneScopedN( "Upload decals/dlights" );
		WriteGPUBuffer( decals_buffer, decals, num_decals * sizeof( Decal ) );
		WriteGPUBuffer( dlights_buffer, dlights, num_dlights * sizeof( DynamicLight ) );
	}

	{
		PipelineState pipeline;
		pipeline.pass = frame_static.particle_update_pass;
		pipeline.shader = &shaders.culling;
		pipeline.set_buffer( "b_Decals", decals_buffer );
		pipeline.set_buffer( "b_Dlights", dlights_buffer );
		pipeline.set_buffer( "b_TileCounts", dynamic_count );
		pipeline.set_buffer( "b_DecalTiles", decal_tiles_buffer );
		pipeline.set_buffer( "b_DLightTiles", dlight_tiles_buffer );
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_TileCulling", UploadUniformBlock( u32( cols ), u32( rows ), u32( num_decals ), u32( num_dlights ) ) );
		DispatchCompute( pipeline, ( cols * rows ) / 64 + 1, 1, 1 );
	}

	num_decals = 0;
	num_dlights = 0;
}

void AddDynamicsToPipeline( PipelineState * pipeline ) {
	pipeline->set_buffer( "b_DecalTiles", decal_tiles_buffer );
	pipeline->set_buffer( "b_Decals", decals_buffer );

	pipeline->set_buffer( "b_DynamicLightTiles", dlight_tiles_buffer );
	pipeline->set_buffer( "b_DynamicLights", dlights_buffer );

	pipeline->set_buffer( "b_DynamicTiles", dynamic_count );
}
