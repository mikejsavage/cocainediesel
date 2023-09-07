#ifndef SHADER_SHARED_H
#define SHADER_SHARED_H

#if __cplusplus
  #include "qcommon/types.h"
  #define shaderconst constexpr
  #define shaderenumdef( t, body ) enum t { body };
  #define shaderenumval( name, value ) name = value,
#else
  #define u32 uint
  #define shaderconst const
  #define shaderenumdef( t, body ) body
  #define shaderenumval( name, value ) const uint name = value;

  // #define gl_VertexIndex gl_VertexID
  // #define gl_InstanceIndex gl_InstanceID
#endif

// mirror this in BuildShaderSrcs
shaderconst u32 FORWARD_PLUS_TILE_SIZE = 32;
shaderconst u32 FORWARD_PLUS_TILE_CAPACITY = 50;
shaderconst float DLIGHT_CUTOFF = 0.5f;
shaderconst u32 SKINNED_MODEL_MAX_JOINTS = 100;

shaderenumdef( ParticleFlags : u32,
	shaderenumval( ParticleFlag_CollisionPoint, u32( 1 ) << u32( 0 ) )
	shaderenumval( ParticleFlag_CollisionSphere, u32( 1 ) << u32( 1 ) )
	shaderenumval( ParticleFlag_Rotate, u32( 1 ) << u32( 2 ) )
	shaderenumval( ParticleFlag_Stretch, u32( 1 ) << u32( 3 ) )
)

shaderenumdef( VertexAttributeType : u32,
	shaderenumval( VertexAttribute_Position, 0 )
	shaderenumval( VertexAttribute_Normal, 1 )
	shaderenumval( VertexAttribute_TexCoord, 2 )
	shaderenumval( VertexAttribute_Color, 3 )
	shaderenumval( VertexAttribute_JointIndices, 4 )
	shaderenumval( VertexAttribute_JointWeights, 5 )

	shaderenumval( VertexAttribute_Count, 6 )
)

shaderenumdef( FragmentShaderOutputType : u32,
	shaderenumval( FragmentShaderOutput_Albedo, 0 )
	shaderenumval( FragmentShaderOutput_CurvedSurfaceMask, 1 )

	shaderenumval( FragmentShaderOutput_Count, 2 )
)

shaderenumdef( DescriptorSetType : u32,
	shaderenumval( DescriptorSet_RenderPass, 0 )
	shaderenumval( DescriptorSet_Material, 1 )
	shaderenumval( DescriptorSet_DrawCall, 2 )
)

#endif // header guard
