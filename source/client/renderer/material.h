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
	float args[ 4 ];                      // offset, amplitude, phase_offset, rate
};

struct TCMod {
	TCModType type;
	float args[ 2 ];
	Wave wave;
};

struct ColorGen {
	ColorGenType type = ColorGenType_Constant;
	float args[ 4 ] = { 1.0f, 1.0f, 1.0f, 1.0f };
	Wave wave;
};

enum SamplerType {
	SamplerType_Normal,
	SamplerType_Clamp,
	SamplerType_ClampAlphaMask,
};

struct TextureSampler {
	Texture texture;
	SamplerType sampler;
};

struct Material {
	TextureSampler textures[ 16 ] = { };
	u8 num_anim_frames = 0;
	float anim_fps = 0;

	ColorGen rgbgen;
	ColorGen alphagen;
	BlendFunc blend_func = BlendFunc_Disabled;
	bool double_sided = false;
	bool discard = false;
	float alpha_cutoff = 0.0f;

	TCMod tcmod = { };
};

extern Material world_material;

void InitMaterials();
void ShutdownMaterials();

Texture FindTexture( StringHash name );
bool TryFindTexture( StringHash name, Texture * texture );

const Material * FindMaterial( StringHash name, const Material * def = NULL );
const Material * FindMaterial( const char * name, const Material * def = NULL );
bool TryFindMaterial( StringHash name, const Material ** material );
