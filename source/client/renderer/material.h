#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/backend.h"

enum WaveFunc {
	WaveFunc_None,
	WaveFunc_Sin,
	WaveFunc_Triangle,
	WaveFunc_Square,
	WaveFunc_Sawtooth,
	WaveFunc_InverseSawtooth,
	WaveFunc_Noise,
	WaveFunc_Constant,
};

enum ColorGenType {
	ColorGenType_Unknown,
	ColorGenType_Constant,
	ColorGenType_Vertex,
	ColorGenType_Wave,
	ColorGenType_EntityWave,
};

enum TCModType {
	TCModFunc_None,
	TCModFunc_Scale,
	TCModFunc_Scroll,
	TCModFunc_Rotate,
	TCModFunc_Turb,
	TCModFunc_Stretch,
};

enum VertexDeformType {
	DEFORMV_NONE,
	DEFORMV_WAVE,
	DEFORMV_BULGE,
	DEFORMV_MOVE,
};

struct Wave {
	WaveFunc type;
	float args[ 4 ];                      // offset, amplitude, phase_offset, rate
};

struct TCMod {
	TCModType type;
	float args[ 6 ];
};

struct ColorGen {
	ColorGenType type;
	float args[ 4 ];
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
	TextureSampler textures[ 16 ];
	u8 num_anim_frames;
	float anim_fps;

	ColorGen rgbgen;
	ColorGen alphagen;
	BlendFunc blend_func;
	bool double_sided;
	bool discard;
	float alpha_cutoff;

	TCMod tcmods[ 4 ];
	u8 num_tcmods;
};

extern Material world_material;

void InitMaterials();
void ShutdownMaterials();

Texture FindTexture( StringHash name );
bool TryFindTexture( StringHash name, Texture * texture );

const Material * FindMaterial( StringHash name, const Material * def = NULL );
const Material * FindMaterial( const char * name, const Material * def = NULL );
bool TryFindMaterial( StringHash name, const Material ** material );
