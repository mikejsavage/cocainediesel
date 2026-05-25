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

bool CompressedTextureFormat( TextureFormat format );
u32 BitsPerPixel( TextureFormat format );
u32 BlockSize( TextureFormat format );

void InitMaterials();
void HotloadMaterials();
void ShutdownMaterials();

// const Material * FindMaterial( StringHash name );
// const Material * FindMaterial( const char * name );
// bool TryFindMaterial( StringHash name, const Material ** material );

struct Sprite {
	u16 layer;
	u16x2 uv, wh;
	u16x4 trim;
};

Optional< Sprite > TryFindSprite( StringHash name );
PoolHandle< BindGroup > SpriteAtlasBindGroup();
PoolHandle< Texture > SpriteAtlasTexture();

struct Material;
Vec2 HalfPixelSize( PoolHandle< Material > material );

Vec4 EvaluateMaterialColor( PoolHandle< Material > material, Vec4 entity_color );
