#include "qcommon/base.h"
#include "qcommon/time.h"
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
	Vec3 origin_orientation_xyz; // floor( origin ) + ( orientation.xyz * 0.49 + 0.5 )
	float radius_orientation_w; // floor( radius ) + ( orientation.w * 0.49 + 0.5 )
	Vec4 color_uvwh_height; // vec4( u + layer, v + floor( r * 255 ) + floor( height ) * 256, w + floor( g * 255 ), h + floor( b * 255 ) )
	// NOTE(msc): uvwh should all be < 1.0
};

struct PersistentDecal {
	Decal decal;
	Time expiration;
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
	Time spawn_time;
	Time lifetime;
};

STATIC_ASSERT( sizeof( DynamicLight ) == 1 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( DynamicLight ) % alignof( DynamicLight ) == 0 );

static constexpr u32 MAX_DECALS = 100000;
static constexpr u32 MAX_DLIGHTS = 100000;

static BoundedDynamicArray< Decal, MAX_DECALS > decals;
static BoundedDynamicArray< DynamicLight, MAX_DLIGHTS > dlights;
static BoundedDynamicArray< PersistentDecal, MAX_DECALS > persistent_decals;
static BoundedDynamicArray< PersistentDynamicLight, MAX_DLIGHTS > persistent_dlights;

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
	decals.clear();
	dlights.clear();
	persistent_decals.clear();
	persistent_dlights.clear();
}

void ShutdownDecals() {
	DeferDeleteStreamingBuffer( decals_buffer );
	DeferDeleteGPUBuffer( decal_tiles_buffer );
	DeferDeleteStreamingBuffer( dlights_buffer );
	DeferDeleteGPUBuffer( dlight_tiles_buffer );
	DeferDeleteGPUBuffer( dynamic_count );
}

void DrawDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, float height ) {
	Vec4 uvwh;
	if( !TryFindDecal( name, &uvwh, NULL ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	Vec3 c = Floor( color.xyz() * 255.0f );
	c.x += floorf( height ) * 256.0f;

	decals.add( Decal {
		.origin_orientation_xyz = Floor( origin ) + ( orientation.im() * 0.49f + 0.5f ),
		.radius_orientation_w = floorf( radius ) + ( orientation.w * 0.49f + 0.5f ),
		.color_uvwh_height = Vec4( uvwh.x, uvwh.y + c.x, uvwh.z + c.y, uvwh.w + c.z ),
	} );
}

void AddPersistentDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, Time lifetime, float height ) {
	Vec4 uvwh;
	if( !TryFindDecal( name, &uvwh, NULL ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	Vec3 c = Floor( color.xyz() * 255.0f );
	c.x += floorf( height ) * 256.0f;

	persistent_decals.add( PersistentDecal {
		.decal = Decal {
			.origin_orientation_xyz = Floor( origin ) + ( orientation.im() * 0.49f + 0.5f ),
			.radius_orientation_w = floorf( radius ) + ( orientation.w * 0.49f + 0.5f ),
			.color_uvwh_height = Vec4( uvwh.x, uvwh.y + c.x, uvwh.z + c.y, uvwh.w + c.z ),
		},
		.expiration = cls.game_time + lifetime,
	} );
}

void DrawPersistentDecals() {
	// remove expired decals
	for( u32 i = 0; i < persistent_decals.size(); i++ ) {
		if( cls.game_time > persistent_decals[ i ].expiration ) {
			persistent_decals.remove_swap( i );
			i--;
		}
	}

	// draw
	for( const PersistentDecal & decal : persistent_decals ) {
		if( !decals.add( decal.decal ) ) {
			break;
		}
	}
}

void DrawDynamicLight( Vec3 origin, Vec3 color, float intensity ) {
	dlights.add( DynamicLight {
		.origin_color = Floor( origin ) + color * 0.9f,
		.radius = sqrtf( intensity / DLIGHT_CUTOFF ),
	} );
}

void AddPersistentDynamicLight( Vec3 origin, Vec3 color, float intensity, Time lifetime ) {
	persistent_dlights.add( PersistentDynamicLight {
		.dlight = DynamicLight {
			.origin_color = Floor( origin ) + color * 0.9f,
			.radius = sqrtf( intensity / DLIGHT_CUTOFF ),
		},
		.start_intensity = intensity,
		.spawn_time = cls.game_time,
		.lifetime = lifetime,
	} );
}

void DrawPersistentDynamicLights() {
	// remove expired dlights
	for( u32 i = 0; i < persistent_dlights.size(); i++ ) {
		if( cls.game_time > persistent_dlights[ i ].spawn_time + persistent_dlights[ i ].lifetime ) {
			persistent_dlights.remove_swap( i );
			i--;
		}
	}

	// draw
	for( const PersistentDynamicLight & dlight : persistent_dlights ) {
		float fract = ToSeconds( cls.game_time - dlight.spawn_time ) / ToSeconds( dlight.lifetime );
		float intensity = Lerp( dlight.start_intensity, fract, 0.0f );
		DynamicLight faded = {
			.origin_color = dlight.dlight.origin_color,
			.radius = sqrtf( intensity / DLIGHT_CUTOFF ),
		};
		if( !dlights.add( faded ) ) {
			break;
		}
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

	decal_tiles_buffer = NewGPUBuffer( NULL, rows * cols * sizeof( GPUDecalTile ), "Decal tile indices" );
	dlight_tiles_buffer = NewGPUBuffer( NULL, rows * cols * sizeof( GPUDynamicLightTile ), "Dynamic light tile indices" );
	dynamic_count = NewGPUBuffer( NULL, rows * cols * sizeof( GPUDynamicCount ), "Dynamics tile counts" );
}

void UploadDecalBuffers() {
	TracyZoneScoped;

	u32 rows = PixelsToTiles( frame_static.viewport_height );
	u32 cols = PixelsToTiles( frame_static.viewport_width );

	memcpy( GetStreamingBufferMemory( decals_buffer ), decals.ptr(), decals.num_bytes() );
	memcpy( GetStreamingBufferMemory( dlights_buffer ), dlights.ptr(), dlights.num_bytes() );

	PipelineState pipeline;
	pipeline.pass = frame_static.tile_culling_pass;
	pipeline.shader = &shaders.tile_culling;
	pipeline.bind_streaming_buffer( "b_Decals", decals_buffer );
	pipeline.bind_streaming_buffer( "b_Dlights", dlights_buffer );
	pipeline.bind_buffer( "b_TileCounts", dynamic_count );
	pipeline.bind_buffer( "b_DecalTiles", decal_tiles_buffer );
	pipeline.bind_buffer( "b_DLightTiles", dlight_tiles_buffer );
	pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
	pipeline.bind_uniform( "u_TileCulling", UploadUniformBlock( cols, rows, u32( decals.size() ), u32( dlights.size() ) ) );
	DispatchCompute( pipeline, ( cols * rows ) / 64 + 1, 1, 1 );

	decals.clear();
	dlights.clear();
}

void AddDynamicsToPipeline( PipelineState * pipeline ) {
	pipeline->bind_streaming_buffer( "b_Decals", decals_buffer );
	pipeline->bind_buffer( "b_DecalTiles", decal_tiles_buffer );
	pipeline->bind_streaming_buffer( "b_DynamicLights", dlights_buffer );
	pipeline->bind_buffer( "b_DynamicLightTiles", dlight_tiles_buffer );
	pipeline->bind_buffer( "b_DynamicTiles", dynamic_count );
}
