#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/f16.h"
#include "qcommon/srgb.h"
#include "qcommon/time.h"
#include "cgame/cg_local.h"
#include "client/renderer/api.h"
#include "client/renderer/shader.h"
#include "client/renderer/shader_shared.h"

static GPUBuffer dynamic_count;
static CoherentSpan< Decal > decals_buffer;
static CoherentSpan< Light > lights_buffer;
static GPUBuffer decal_tiles_buffer;
static GPUBuffer light_tiles_buffer;

struct PersistentDecal {
	Decal decal;
	Time expiration;
};

struct PersistentLight {
	s16x3 origin;
	RGBA8 color;
	float start_intensity;
	Time spawn_time;
	Time lifetime;
};

static constexpr u32 MAX_DECALS = 100'000;
static constexpr u32 MAX_LIGHTS = 100'000;

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

static s16x3 FloorToS16( Vec3 v ) {
	return s16x3 {
		checked_cast< s16 >( floorf( v.x ) ),
		checked_cast< s16 >( floorf( v.y ) ),
		checked_cast< s16 >( floorf( v.z ) ),
	};
}

static s16x4 QuantizeQuaternion( Quaternion q ) {
	return s16x4 {
		.x = Quantize11< s16 >( q.x ),
		.y = Quantize11< s16 >( q.y ),
		.z = Quantize11< s16 >( q.z ),
		.w = Quantize11< s16 >( q.w ),
	};
}

static Decal MakeDecal( Vec3 origin, Quaternion orientation, float radius, Vec4 color, float height, Sprite sprite ) {
	return Decal {
		.color = LinearTosRGB( color ),
		.origin = FloorToS16( origin ),
		.radius = FloatToHalf( radius ),
		.orientation = QuantizeQuaternion( orientation ),
		.height = FloatToHalf( height ),
		.layer = sprite.layer,
		.uv = sprite.uv,
		.wh = sprite.wh,
	};
}

void DrawDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, float height ) {
	Optional< Sprite > sprite = TryFindSprite( name );
	if( !sprite.exists ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have sprite key", name );
		return;
	}

	[[maybe_unused]] bool ok = decals.add( MakeDecal( origin, orientation, radius, color, height, sprite.value ) );
}

void AddPersistentDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, Time lifetime, float height ) {
	Optional< Sprite > sprite = TryFindSprite( name );
	if( !sprite.exists ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have sprite key", name );
		return;
	}

	[[maybe_unused]] bool ok = persistent_decals.add( PersistentDecal {
		.decal = MakeDecal( origin, orientation, radius, color, height, sprite.value ),
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

static float LightIntensityToRadius( float intensity ) {
	return sqrtf( intensity / LIGHT_CUTOFF );
}

void DrawLight( Vec3 origin, Vec3 color, float intensity ) {
	[[maybe_unused]] bool ok = lights.add( Light {
		.color = RGBA8( LinearTosRGB( color ) ),
		.origin = FloorToS16( origin ),
		.radius = FloatToHalf( LightIntensityToRadius( intensity ) ),
	} );
}

void AddPersistentLight( Vec3 origin, Vec3 color, float intensity, Time lifetime ) {
	[[maybe_unused]] bool ok = persistent_lights.add( PersistentLight {
		.origin = FloorToS16( origin ),
		.color = RGBA8( LinearTosRGB( color ) ),
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
			.color = light.color,
			.origin = light.origin,
			.radius = FloatToHalf( LightIntensityToRadius( intensity ) ),
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
	decals_buffer = NewCoherentSpan< Decal >( decals.capacity() );
	lights_buffer = NewCoherentSpan< Light >( lights.capacity() );
	decal_tiles_buffer = NewDeviceTempBuffer( "Decal tile indices", num_tiles * sizeof( GPUDecalTile ), alignof( GPUDecalTile ) );
	light_tiles_buffer = NewDeviceTempBuffer( "Light tile indices", num_tiles * sizeof( GPULightTile ), alignof( GPULightTile ) );
}

void UploadDecalBuffers() {
	TracyZoneScoped;

	u32 rows = PixelsToTiles( frame_static.viewport_height );
	u32 cols = PixelsToTiles( frame_static.viewport_width );

	for( size_t i = 0; i < decals.size(); i++ ) {
		decals_buffer.data[ i ] = decals[ i ];
	}
	for( size_t i = 0; i < lights.size(); i++ ) {
		lights_buffer.data[ i ] = lights[ i ];
	}

	frame_static.render_passes[ RenderPass_TileCulling ] = NewComputePass( ComputePassConfig {
		.name = "Decal/light tile culling",
		.pass = RenderPass_TileCulling,
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
		{ "u_Decals", decals_buffer.buffer },
		{ "u_Lights", lights_buffer.buffer },
		{ "u_TileCounts", dynamic_count },
		{ "u_DecalTiles", decal_tiles_buffer },
		{ "u_LightTiles", light_tiles_buffer },
	} );

	decals.clear();
	lights.clear();
}

DynamicsBuffers GetDynamicsBuffers() {
	return DynamicsBuffers {
		.tile_counts = dynamic_count,
		.decals = decals_buffer.buffer,
		.lights = lights_buffer.buffer,
		.decal_tiles = decal_tiles_buffer,
		.light_tiles = light_tiles_buffer,
	};
}
