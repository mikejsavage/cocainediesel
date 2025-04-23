#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 1, DescriptorSet_RenderPass )]] Texture2D< float > u_BlueNoiseTexture;
[[vk::binding( 2, DescriptorSet_RenderPass )]] SamplerState u_StandardSampler;
#if APPLY_SHADOWS
[[vk::binding( 3, DescriptorSet_RenderPass )]] StructuredBuffer< ShadowmapUniforms > u_Shadowmap;
[[vk::binding( 4, DescriptorSet_RenderPass )]] Texture2DArray< float > u_ShadowmapTextureArray;
[[vk::binding( 5, DescriptorSet_RenderPass )]] SamplerComparisonState u_ShadowmapSampler;
#endif
#if APPLY_DYNAMICS
/* [[vk::binding( 6, DescriptorSet_RenderPass )]] StructuredBuffer< TileCullingDimensions > u_TileDimensions; */
[[vk::binding( 7, DescriptorSet_RenderPass )]] StructuredBuffer< TileCountsUniforms > u_TileCounts;
[[vk::binding( 8, DescriptorSet_RenderPass )]] StructuredBuffer< TileIndices > u_DecalTiles;
[[vk::binding( 9, DescriptorSet_RenderPass )]] StructuredBuffer< TileIndices > u_LightTiles;
[[vk::binding( 10, DescriptorSet_RenderPass )]] StructuredBuffer< Decal > u_Decals;
[[vk::binding( 11, DescriptorSet_RenderPass )]] StructuredBuffer< Light > u_Lights;
[[vk::binding( 12, DescriptorSet_RenderPass )]] Texture2DArray< float > u_SpriteAtlas;
#endif

#include "include/standard_material.hlsl"

[[vk::binding( 0, DescriptorSet_DrawCall )]] StructuredBuffer< float3x4 > u_ModelTransform;

[[vk::binding( 1, DescriptorSet_DrawCall )]] StructuredBuffer< float4 > u_MaterialColor;
#if SKINNED
[[vk::binding( 2, DescriptorSet_DrawCall )]] StructuredBuffer< float3x4 > u_Pose;
#endif

#include "include/decals.hlsl"
#include "include/dither.hlsl"
#include "include/lighting.hlsl"
#include "include/dlights.hlsl"
#include "include/fog.hlsl"
#include "include/shadows.hlsl"
#include "include/skinning.hlsl"

struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float3 position : POSITION;
	[[vk::location( VertexAttribute_Normal )]] float3 normal : NORMAL;
#if VERTEX_COLORS
	[[vk::location( VertexAttribute_Color )]] float4 color : COLOR;
#endif
	[[vk::location( VertexAttribute_TexCoord )]] float2 uv : TEXCOORD0;
#if SKINNED
	[[vk::location( VertexAttribute_JointIndices )]] uint4 indices : BLENDINDICES;
	[[vk::location( VertexAttribute_JointWeights )]] float4 weights : BLENDWEIGHT;
#endif
};

struct VertexOutput {
	float3 world_position : POSITION;
	float4 position : SV_Position;
	float3 normal : NORMAL;
#if VERTEX_COLORS
	float4 color : COLOR;
#endif
	float2 uv : TEXCOORD0;
};

VertexOutput VertexMain( VertexInput input ) {
	float3x4 M = u_ModelTransform[ 0 ];

#if SKINNED
	M = mul34( M, SkinningMatrix( input.indices, input.weights ) );
#endif

	VertexOutput output;
	output.world_position = mul34( M, float4( input.position, 1.0f ) ).xyz;
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, float4( output.world_position, 1.0f ) ) );
	output.normal = mul( Adjugate( M ), input.normal );
	output.uv = input.uv;
#if VERTEX_COLORS
	output.color = sRGBToLinear( input.color );
#endif
	return output;
}

struct FragmentOutput {
	float4 albedo : FragmentShaderOutput_Albedo;
	uint curved_surface_mask : FragmentShaderOutput_CurvedSurfaceMask;
};

FragmentOutput FragmentMain( VertexOutput v ) {
	float4 albedo = u_MaterialColor[ 0 ];
#if !APPLY_DRAWFLAT
	albedo *= u_Texture.SampleBias( u_Sampler, v.uv, u_MaterialProperties[ 0 ].lod_bias );
#endif
#if VERTEX_COLORS
	albedo *= v.color;
#endif

	float3 normal = normalize( v.normal );

#if APPLY_DYNAMICS
	float tile_size = float( FORWARD_PLUS_TILE_SIZE );
	int tile_row = int( ( u_View[ 0 ].viewport_size.y - v.position.y - 1.0f ) / tile_size );
	int tile_col = int( v.position.x / tile_size );
	int cols = int( u_View[ 0 ].viewport_size.x + tile_size - 1 ) / int( tile_size );
	int tile_index = tile_row * cols + tile_col;
	TileCountsUniforms dynamic_tile = u_TileCounts[ tile_index ];

	AddDecals( v.world_position, dynamic_tile.num_decals, tile_index, albedo, normal );
#endif

#if SHADED
	const float3 suncolor = 1.0f;

	float3 viewDir = normalize( u_View[ 0 ].camera_pos - v.world_position );

	float3 lambertlight = suncolor * LambertLight( normal, -u_View[ 0 ].sun_direction );
	float3 specularlight = suncolor * SpecularLight( normal, u_View[ 0 ].sun_direction, viewDir, u_MaterialProperties[ 0 ].shininess ) * u_MaterialProperties[ 0 ].specular;

	float shadowlight = 1.0f;
	#if APPLY_SHADOWS
		shadowlight = GetLight( v.world_position, normal );
		specularlight = specularlight * shadowlight;
	#endif
	shadowlight = shadowlight * 0.5f + 0.5f;

	#if APPLY_DYNAMICS
		AddLights( dynamic_tile.num_lights, tile_index, v.world_position, normal, viewDir, lambertlight, specularlight );
	#endif
	lambertlight = lambertlight * 0.5f + 0.5f;

	#if APPLY_DRAWFLAT
		lambertlight = lambertlight * 0.5f + 0.5f;
	#endif

	albedo.rgb *= shadowlight * ( lambertlight + specularlight );
#endif

#if APPLY_FOG
	albedo.rgb = Fog( albedo.rgb, length( v.world_position - u_View[ 0 ].camera_pos ) );
	albedo.rgb += Dither( v.position.xy );
#endif

	albedo.rgb = VoidFog( albedo.rgb, v.world_position.z );
	albedo.a = VoidFogAlpha( albedo.a, v.world_position.z );

	FragmentOutput output;
	output.albedo = albedo;
	output.curved_surface_mask = length( fwidth( normal ) ) < 0.000001f ? 0u : MASK_CURVED;
	return output;
}
