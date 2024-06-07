#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/backend.h"

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

enum TCModType {
	TCModFunc_None,
	TCModFunc_Scroll,
	TCModFunc_Rotate,
	TCModFunc_Stretch,
};

struct Wave {
	WaveFunc type;
	float args[ 4 ]; // offset, amplitude, phase_offset, rate
};

struct TCMod {
	TCModType type;
	float args[ 2 ];
	Wave wave;
};

struct ColorGen {
	ColorGenType type = ColorGenType_Constant;
	float args[ 4 ] = { 1.0f, 1.0f, 1.0f, 1.0f };
	Wave wave = { };
};

struct Material {
	Span< char > name;
	u64 hash;

	const Texture * texture;
	SamplerType sampler = Sampler_Standard;

	ColorGen rgbgen;
	ColorGen alphagen;
	BlendFunc blend_func = BlendFunc_Disabled;
	bool double_sided = false;
	bool decal = false;
	bool mask_outlines = false;
	bool outlined = true;
	bool shaded = false;
	float specular = 0.0f;
	float shininess = 64.0f;

	TCMod tcmod = { };
};

extern Material world_material;
extern Material wallbang_material;

bool CompressedTextureFormat( TextureFormat format );
u32 BitsPerPixel( TextureFormat format );

void InitMaterials();
void HotloadMaterials();
void ShutdownMaterials();

const Material * FindMaterial( StringHash name, const Material * def = NULL );
const Material * FindMaterial( const char * name, const Material * def = NULL );
bool TryFindMaterial( StringHash name, const Material ** material );

Sampler GetSampler( SamplerType sampler );

bool TryFindDecal( StringHash name, Vec4 * uvwh, Vec4 * trim );
const Texture * DecalAtlasTextureArray();

Vec2 HalfPixelSize( const Material * material );
