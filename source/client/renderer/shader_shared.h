#ifndef SHADER_SHARED_H
#define SHADER_SHARED_H

#if __cplusplus
  #include "qcommon/types.h"
  #define shaderconst constexpr
#else
  #define s32 int32_t
  #define u32 uint32_t
  #define shaderconst const
  #define Vec3 vec3
  #define Vec4 vec4
#endif

// mirror this in BuildShaderSrcs
shaderconst u32 FORWARD_PLUS_TILE_SIZE = 32;
shaderconst u32 FORWARD_PLUS_TILE_CAPACITY = 50;
shaderconst float DLIGHT_CUTOFF = 0.5f;
shaderconst u32 SKINNED_MODEL_MAX_JOINTS = 100;

enum ParticleFlags : u32 {
	ParticleFlag_CollisionPoint = u32( 1 ) << u32( 0 ),
	ParticleFlag_CollisionSphere = u32( 1 ) << u32( 1 ),
	ParticleFlag_Rotate = u32( 1 ) << u32( 2 ),
	ParticleFlag_Stretch = u32( 1 ) << u32( 3 ),
};

enum VertexAttributeType {
	VertexAttribute_Position,
	VertexAttribute_Normal,
	VertexAttribute_TexCoord,
	VertexAttribute_Color,
	VertexAttribute_JointIndices,
	VertexAttribute_JointWeights,

	VertexAttribute_Count
};

enum FragmentShaderOutputType {
	FragmentShaderOutput_Albedo,
	FragmentShaderOutput_CurvedSurfaceMask,

	FragmentShaderOutput_Count
};

enum DescriptorSetType {
	DescriptorSet_RenderPass,
	DescriptorSet_Material,
	DescriptorSet_DrawCall,

	DescriptorSet_Count
};

struct TextUniforms {
	Vec4 color;
	Vec4 border_color;
	float dSDF_dTexel;
	s32 has_border;
};

#endif // header guard
