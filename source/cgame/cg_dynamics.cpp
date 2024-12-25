#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/time.h"
#include "cgame/cg_local.h"
#include "client/renderer/api.h"
#include "client/renderer/shader.h"
#include "client/renderer/shader_shared.h"

STATIC_ASSERT( sizeof( Decal ) == 2 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( Decal ) % alignof( Decal ) == 0 );

STATIC_ASSERT( sizeof( DynamicLight ) == 1 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( DynamicLight ) % alignof( DynamicLight ) == 0 );

static GPUTempBuffer this_frame_decals_buffer;
static GPUBuffer decal_tiles_buffer;
static GPUTempBuffer this_frame_dlights_buffer;
static GPUBuffer dlight_tiles_buffer;
static GPUBuffer dynamic_count;

struct PersistentDecal {
	Decal decal;
	Time expiration;
};

struct PersistentDynamicLight {
	DynamicLight dlight;
	float start_intensity;
	Time spawn_time;
	Time lifetime;
};

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

	this_frame_decals_buffer = NewStreamingBuffer( sizeof( decals ), "Decals" );
	this_frame_dlights_buffer = NewStreamingBuffer( sizeof( dlights ), "Dynamic lights" );
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

void DrawDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, float height ) {
	Vec4 uvwh;
	if( !TryFindDecal( name, &uvwh, NULL ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	Vec3 c = Floor( color.xyz() * 255.0f );
	c.x += floorf( height ) * 256.0f;

	[[maybe_unused]] bool ok = decals.add( Decal {
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

	[[maybe_unused]] bool ok = persistent_decals.add( PersistentDecal {
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
	[[maybe_unused]] bool ok = dlights.add( DynamicLight {
		.origin_color = Floor( origin ) + color * 0.9f,
		.radius = sqrtf( intensity / DLIGHT_CUTOFF ),
	} );
}

void AddPersistentDynamicLight( Vec3 origin, Vec3 color, float intensity, Time lifetime ) {
	[[maybe_unused]] bool ok = persistent_dlights.add( PersistentDynamicLight {
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
	this_frame_decals_buffer = NewTempBuffer( decals.num_bytes(), alignof( Decal ) );
	this_frame_dlights_buffer = NewTempBuffer( dlights.num_bytes(), alignof( DynamicLight ) );

	if( decal_tiles_buffer.size != 0 && !frame_static.viewport_resized )
		return;

	TracyZoneScopedN( "Reallocate tile buffers" );

	u32 rows = PixelsToTiles( frame_static.viewport_height );
	u32 cols = PixelsToTiles( frame_static.viewport_width );

	decal_tiles_buffer = NewBuffer( GPULifetime_Framebuffer, "Decal tile indices", rows * cols * sizeof( GPUDecalTile ), sizeof( GPUDecalTile ), false );
	dlight_tiles_buffer = NewBuffer( GPULifetime_Framebuffer, "Dynamic light tile indices", rows * cols * sizeof( GPUDynamicLightTile ), sizeof( GPUDynamicLightTile ), false );
	dynamic_count = NewBuffer( GPULifetime_Framebuffer, "Dynamics tile counts", rows * cols * sizeof( GPUDynamicCount ), sizeof( GPUDynamicCount ), false );
}

void UploadDecalBuffers() {
	TracyZoneScoped;

	u32 rows = PixelsToTiles( frame_static.viewport_height );
	u32 cols = PixelsToTiles( frame_static.viewport_width );

	memcpy( this_frame_decals_buffer.ptr, decals.ptr(), decals.num_bytes() );
	memcpy( this_frame_dlights_buffer.ptr, dlights.ptr(), dlights.num_bytes() );

	frame_static.render_passes[ RenderPass_TileCulling ] = NewComputePass( ComputePassConfig {
		.name = "Particle/dlight tile culling",
	} );

	TileCullingUniforms uniforms = {
		.rows = rows,
		.cols = cols,
		.num_decals = u32( decals.size() ),
		.num_dlights = u32( dlights.size() ),
	};

	EncodeComputeCall( RenderPass_TileCulling, shaders.tile_culling, ( cols * rows ) / 64 + 1, 1, 1, {
		{ "u_View", frame_static.view_uniforms },
		{ "u_TileCulling", NewTempBuffer( uniforms ) },
		{ "b_Decals", this_frame_decals_buffer },
		{ "b_Dlights", this_frame_dlights_buffer },
		{ "b_TileCounts", dynamic_count },
		{ "b_DecalTiles", decal_tiles_buffer },
		{ "b_DLightTiles", dlight_tiles_buffer },
	} );

	decals.clear();
	dlights.clear();
}

void AddDynamicsToPipeline( PipelineState * pipeline ) {
	// TODO: this is no good now
	pipeline->bind_streaming_buffer( "b_Decals", this_frame_decals_buffer );
	pipeline->bind_buffer( "b_DecalTiles", decal_tiles_buffer );
	pipeline->bind_streaming_buffer( "b_DynamicLights", this_frame_dlights_buffer );
	pipeline->bind_buffer( "b_DynamicLightTiles", dlight_tiles_buffer );
	pipeline->bind_buffer( "b_DynamicTiles", dynamic_count );
}
