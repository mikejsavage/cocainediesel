#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/api.h"

enum WaveFunc {
	WaveFunc_Sin,
	WaveFunc_Triangle,
	WaveFunc_Sawtooth,
	WaveFunc_InverseSawtooth,
};

enum ColorGenType {
	ColorGenType_Constant,
	ColorGenType_Entity,
	ColorGenType_Wave,
	ColorGenType_EntityWave,
};

struct Wave {
	WaveFunc type;
	float args[ 4 ]; // offset, amplitude, phase_offset, rate
};

struct ColorGen {
	ColorGenType type = ColorGenType_Constant;
	float args[ 4 ] = { 1.0f, 1.0f, 1.0f, 1.0f };
	Wave wave = { };
};

// TODO
template< size_t N >
struct BoundedString {
	char str[ N ];
	size_t n;

	BoundedString() = default;

	BoundedString( Span< const char > str_ ) {
		n = Min2( str_.n, N );
		memcpy( str, str_.ptr, n );
	}

	Span< const char > span() const { return Span< const char >( str, n ); }
};

struct Material {
	BoundedString< 64 > name;
	u64 hash;

	PoolHandle< RenderPipeline > shader;
	PoolHandle< BindGroup > bind_group;
	PoolHandle< Texture > texture;
	RenderPipelineDynamicState renderer_dynamic_state;

	ColorGen rgbgen;
	ColorGen alphagen;
	bool outlined = true;
	bool shaded = false;
	bool world = false;
	// float specular = 0.0f;
	// float shininess = 64.0f;
};

bool CompressedTextureFormat( TextureFormat format );
u32 BitsPerPixel( TextureFormat format );

void InitMaterials();
void HotloadMaterials();
void ShutdownMaterials();

const Material * FindMaterial( StringHash name );
const Material * FindMaterial( const char * name );
bool TryFindMaterial( StringHash name, const Material ** material );

bool TryFindSprite( StringHash name, Vec4 * uvwh, Vec4 * trim );
PoolHandle< BindGroup > SpriteAtlasBindGroup();
PoolHandle< Texture > SpriteAtlasTexture();

Vec2 HalfPixelSize( const Material * material );

Vec4 EvaluateMaterialColor( const Material & material, Vec4 entity_color );
