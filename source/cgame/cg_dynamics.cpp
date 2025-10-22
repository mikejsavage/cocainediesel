#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/time.h"
#include "cgame/cg_local.h"
#include "client/renderer/api.h"
#include "client/renderer/shader.h"
#include "client/renderer/shader_shared.h"

STATIC_ASSERT( sizeof( Decal ) == 2 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( Decal ) % alignof( Decal ) == 0 );

STATIC_ASSERT( sizeof( Light ) == 1 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( Light ) % alignof( Light ) == 0 );

static GPUBuffer dynamic_count;
static CoherentBuffer lights_buffer;
static CoherentBuffer decals_buffer;
static GPUBuffer decal_tiles_buffer;
static GPUBuffer light_tiles_buffer;

struct PersistentDecal {
	Decal decal;
	Time expiration;
};

struct PersistentLight {
	Light light;
	float start_intensity;
	Time spawn_time;
	Time lifetime;
};

static constexpr u32 MAX_DECALS = 100000;
static constexpr u32 MAX_LIGHTS = 100000;

static BoundedDynamicArray< Decal, MAX_DECALS > decals;
static BoundedDynamicArray< Light, MAX_LIGHTS > lights;
static BoundedDynamicArray< PersistentDecal, MAX_DECALS > persistent_decals;
static BoundedDynamicArray< PersistentLight, MAX_LIGHTS > persistent_lights;

struct GPUDecalTile {
	u32 decals[ FORWARD_PLUS_TILE_CAPACITY ];
};

struct GPULightTile {
	u32 lights[ FORWARD_PLUS_TILE_CAPACITY ];
};

struct GPUDynamicCount {
	u32 decal_count;
	u32 light_count;
};

void InitDecals() {
	ResetDecals();
}

void ResetDecals() {
	decals.clear();
	lights.clear();
	persistent_decals.clear();
	persistent_lights.clear();
}

void DrawDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, float height ) {
	Optional< Sprite > sprite = TryFindSprite( name );
	if( !sprite.exists ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	Vec3 c = Floor( color.xyz() * 255.0f );
	c.x += floorf( height ) * 256.0f;

	Vec4 uvwh = sprite.value.uvwh;
	[[maybe_unused]] bool fail_silently = decals.add( Decal {
		.origin_orientation_xyz = Floor( origin ) + ( orientation.im() * 0.49f + 0.5f ),
		.radius_orientation_w = floorf( radius ) + ( orientation.w * 0.49f + 0.5f ),
		.color_uvwh_height = Vec4( uvwh.x, uvwh.y + c.x, uvwh.z + c.y, uvwh.w + c.z ),
	} );
}

void AddPersistentDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, Time lifetime, float height ) {
	Optional< Sprite > sprite = TryFindSprite( name );
	if( !sprite.exists ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	Vec3 c = Floor( color.xyz() * 255.0f );
	c.x += floorf( height ) * 256.0f;

	Vec4 uvwh = sprite.value.uvwh;
	[[maybe_unused]] bool fail_silently = persistent_decals.add( PersistentDecal {
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

void DrawLight( Vec3 origin, Vec3 color, float intensity ) {
	[[maybe_unused]] bool ok = lights.add( Light {
		.origin_color = Floor( origin ) + color * 0.9f,
		.radius = sqrtf( intensity / LIGHT_CUTOFF ),
	} );
}

void AddPersistentLight( Vec3 origin, Vec3 color, float intensity, Time lifetime ) {
	[[maybe_unused]] bool ok = persistent_lights.add( PersistentLight {
		.light = Light {
			.origin_color = Floor( origin ) + color * 0.9f,
			.radius = sqrtf( intensity / LIGHT_CUTOFF ),
		},
		.start_intensity = intensity,
		.spawn_time = cls.game_time,
		.lifetime = lifetime,
	} );
}

void DrawPersistentLights() {
	// remove expired lights
	for( u32 i = 0; i < persistent_lights.size(); i++ ) {
		if( cls.game_time > persistent_lights[ i ].spawn_time + persistent_lights[ i ].lifetime ) {
			persistent_lights.remove_swap( i );
			i--;
		}
	}

	// draw
	for( const PersistentLight & light : persistent_lights ) {
		float fract = ToSeconds( cls.game_time - light.spawn_time ) / ToSeconds( light.lifetime );
		float intensity = Lerp( light.start_intensity, fract, 0.0f );
		Light faded = {
			.origin_color = light.light.origin_color,
			.radius = sqrtf( intensity / LIGHT_CUTOFF ),
		};
		if( !lights.add( faded ) ) {
			break;
		}
	}
}

static u32 PixelsToTiles( u32 pixels ) {
	return ( pixels + FORWARD_PLUS_TILE_SIZE - 1 ) / FORWARD_PLUS_TILE_SIZE;
}

void AllocateDecalBuffers() {
	u32 num_tiles = PixelsToTiles( frame_static.viewport_height ) * PixelsToTiles( frame_static.viewport_width );

	dynamic_count = NewDeviceTempBuffer( "Dynamics tile counts", num_tiles * sizeof( GPUDynamicCount ), alignof( GPUDynamicCount ) );
	decals_buffer = NewTempBuffer( decals.num_bytes(), alignof( Decal ) );
	lights_buffer = NewTempBuffer( lights.num_bytes(), alignof( Light ) );
	decal_tiles_buffer = NewDeviceTempBuffer( "Decal tile indices", num_tiles * sizeof( GPUDecalTile ), alignof( GPUDecalTile ) );
	light_tiles_buffer = NewDeviceTempBuffer( "Light tile indices", num_tiles * sizeof( GPULightTile ), alignof( GPULightTile ) );
}

void UploadDecalBuffers() {
	TracyZoneScoped;

	u32 rows = PixelsToTiles( frame_static.viewport_height );
	u32 cols = PixelsToTiles( frame_static.viewport_width );

	memcpy( decals_buffer.ptr, decals.ptr(), decals.num_bytes() );
	memcpy( lights_buffer.ptr, lights.ptr(), lights.num_bytes() );

	frame_static.render_passes[ RenderPass_TileCulling ] = NewComputePass( ComputePassConfig {
		.name = "Particle/light tile culling",
	} );

	GPUBuffer tile_culling = NewTempBuffer( TileCullingInputs {
		.rows = rows,
		.cols = cols,
		.num_decals = u32( decals.size() ),
		.num_lights = u32( lights.size() ),
	} );

	EncodeComputeCall( RenderPass_TileCulling, shaders.tile_culling, ( cols * rows ) / 64 + 1, 1, 1, {
		{ "u_View", frame_static.view_uniforms },
		{ "u_TileCulling", tile_culling },
		{ "b_Decals", decals_buffer.buffer },
		{ "b_lights", lights_buffer.buffer },
		{ "b_TileCounts", dynamic_count },
		{ "b_DecalTiles", decal_tiles_buffer },
		{ "b_LightTiles", light_tiles_buffer },
	} );

	decals.clear();
	lights.clear();
}

DynamicsBuffers GetDynamicsBuffers() {
	return DynamicsBuffers {
		.tile_counts = dynamic_count,
		.decal_tiles = decal_tiles_buffer,
		.light_tiles = light_tiles_buffer,
		.decals = decals_buffer.buffer,
		.lights = lights_buffer.buffer,
	};
}
