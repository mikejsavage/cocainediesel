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

	BoundedString( const char * str_ ) {
		n = Min2( strlen( str_ ), N );
		memcpy( str, str_, n );
	}

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
	RenderPipelineDynamicState dynamic_state;
	PoolHandle< BindGroup > bind_group;

	ColorGen rgbgen;
	ColorGen alphagen;
	BlendFunc blend_func = BlendFunc_Disabled;
	bool double_sided = false;
	bool decal = false;
	bool outlined = true;
	bool shaded = false;
	bool world = false;
	float specular = 0.0f;
	float shininess = 64.0f;
	SamplerType sampler = Sampler_Standard;
};

bool CompressedTextureFormat( TextureFormat format );
u32 BitsPerPixel( TextureFormat format );

void InitMaterials();
void HotloadMaterials();
void ShutdownMaterials();

const Material * FindMaterial( StringHash name );
const Material * FindMaterial( const char * name );
bool TryFindMaterial( StringHash name, const Material ** material );

bool TryFindDecal( StringHash name, Vec4 * uvwh, Vec4 * trim );
PoolHandle< BindGroup > DecalAtlasBindGroup();

Vec2 HalfPixelSize( const Material * material );
