#include "include/common.hlsl"
#include "include/decals.hlsl"
#include "include/dither.hlsl"
#include "include/lighting.hlsl"
#include "include/lights.hlsl"
#include "include/fog.hlsl"
#include "include/shadows.hlsl"
#include "include/skinning.hlsl"

#include "include/standard_renderpass.hlsl"
#include "include/standard_material.hlsl"

struct DrawCallPushConstants {
	vk::BufferPointer< Float3x4 > model_transform;
	vk::BufferPointer< float4 > material_color;
	vk::BufferPointer< Float3x4 > pose;
};
[[vk::push_constant]] DrawCallPushConstants u_DrawCall;


struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float3 position : POSITION;
	[[vk::location( VertexAttribute_Normal )]] float3 normal : NORMAL;
#ifdef VERTEX_COLORS
	[[vk::location( VertexAttribute_Color )]] float4 color : COLOR;
#endif
	[[vk::location( VertexAttribute_TexCoord )]] float2 uv : TEXCOORD0;
#ifdef SKINNED
	[[vk::location( VertexAttribute_JointIndices )]] uint4 indices : BLENDINDICES;
	[[vk::location( VertexAttribute_JointWeights )]] float4 weights : BLENDWEIGHT;
#endif
};

struct VertexOutput {
	float3 world_position : POSITION;
	float4 position : SV_Position;
	float3 normal : NORMAL;
#ifdef VERTEX_COLORS
	float4 color : COLOR;
#endif
	float2 uv : TEXCOORD0;
};

VertexOutput VertexMain( VertexInput input ) {
	float3x4 M = u_DrawCall.model_transform.Get().m;

#ifdef SKINNED
	M = mul34( M, SkinningMatrix( u_DrawCall.pose, input.indices, input.weights ) );
#endif

	VertexOutput output;
	output.world_position = mul34( M, float4( input.position, 1.0f ) ).xyz;
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, float4( output.world_position, 1.0f ) ) );
	output.normal = mul( Adjugate( M ), input.normal );
	output.uv = input.uv;
#ifdef VERTEX_COLORS
	output.color = sRGBToLinear( input.color );
#endif
	return output;
}

struct FragmentOutput {
	float4 albedo : FragmentShaderOutput_Albedo;
	uint curved_surface_mask : FragmentShaderOutput_CurvedSurfaceMask;
};

FragmentOutput FragmentMain( VertexOutput v ) {
	float4 albedo = u_DrawCall.material_color.Get();
#ifndef WORLD
	albedo *= u_Texture.SampleBias( u_Sampler, v.uv, u_MaterialProperties[ 0 ].lod_bias );
#endif
#ifdef VERTEX_COLORS
	albedo *= v.color;
#endif

	float3 normal = normalize( v.normal );

#ifdef APPLY_DYNAMICS
	float tile_size = float( FORWARD_PLUS_TILE_SIZE );
	int tile_row = int( ( u_View[ 0 ].viewport_size.y - v.position.y - 1.0f ) / tile_size );
	int tile_col = int( v.position.x / tile_size );
	int cols = int( u_View[ 0 ].viewport_size.x + tile_size - 1 ) / int( tile_size );
	int tile_index = tile_row * cols + tile_col;
	TileCountsUniforms dynamic_tile = u_TileCounts[ tile_index ];

	AddDecals( v.world_position, dynamic_tile.num_decals, tile_index, albedo, normal );
#endif

#ifdef SHADED
	const float3 suncolor = 1.0f;

	float3 viewDir = normalize( u_View[ 0 ].camera_pos - v.world_position );

	float3 lambertlight = suncolor * LambertLight( normal, -u_View[ 0 ].sun_direction );
	float3 specularlight = suncolor * SpecularLight( normal, u_View[ 0 ].sun_direction, viewDir, u_MaterialProperties[ 0 ].shininess ) * u_MaterialProperties[ 0 ].specular;

	float shadowlight = 1.0f;
	#ifdef APPLY_SHADOWS
		shadowlight = GetLight( v.world_position, normal );
		specularlight = specularlight * shadowlight;
	#endif
	shadowlight = shadowlight * 0.5f + 0.5f;

	#ifdef APPLY_DYNAMICS
		AddLights( dynamic_tile.num_lights, tile_index, v.world_position, normal, viewDir, lambertlight, specularlight );
	#endif
	lambertlight = lambertlight * 0.5f + 0.5f;

	#ifdef WORLD
		lambertlight = lambertlight * 0.5f + 0.5f;
	#endif

	albedo.rgb *= shadowlight * ( lambertlight + specularlight );
#endif

#ifdef APPLY_FOG
	albedo.rgb = Fog( albedo.rgb, length( v.world_position - u_View[ 0 ].camera_pos ) );
	albedo.rgb += Dither( u_BlueNoise, v.position.xy );
#endif

	albedo.rgb = VoidFog( albedo.rgb, v.world_position.z );
	albedo.a = VoidFogAlpha( albedo.a, v.world_position.z );

	FragmentOutput output;
	output.albedo = albedo;
	output.curved_surface_mask = length( fwidth( normal ) ) < 0.000001f ? 0u : MASK_CURVED;
	return output;
}
