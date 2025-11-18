#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 0, DescriptorSet_Material )]] Texture2DArray< float4 > u_SpriteAtlas;
[[vk::binding( 1, DescriptorSet_Material )]] SamplerState u_Sampler;
[[vk::binding( 0, DescriptorSet_DrawCall )]] StructuredBuffer< Particle > b_Particles;

#include "include/fog.hlsl"

struct VertexInput {
	float3 position : POSITION;
	float2 uv : TEXCOORD0;
};

struct VertexOutput {
	float4 position : SV_Position;
	float3 world_position : POSITION;
	float2 uv : TEXCOORD0;
	nointerpolation float layer : LAYER;
	float4 color : COLOR;
};

static const float2 quad_positions[] = {
	float2( -0.5f, -0.5f ),
	float2( 0.5f, -0.5f ),
	float2( -0.5f, 0.5f ),
	float2( 0.5f, 0.5f ),
};

static const int index_buffer[] = { 0, 1, 2, 2, 1, 3 };

float2 PositionToTexCoord( float2 p ) {
	return p * float2( 1.0f, -1.0f ) + 0.5f;
}

VertexOutput VertexMain( VertexInput input, uint32_t instance_id : SV_InstanceID, uint32_t vertex_id : SV_VertexID ) {
	Particle particle = b_Particles[ instance_id ];

	float fage = particle.age / particle.lifetime;

	VertexOutput output;
	output.color = lerp( sRGBToLinear( particle.start_color ), sRGBToLinear( particle.end_color ), fage );
	float2 position = quad_positions[ index_buffer[ vertex_id ] ];
	output.uv = PositionToTexCoord( position );
	position += 0.5f;
	position = position * particle.trim.zw + particle.trim.xy;
	position -= 0.5f;
	output.uv *= particle.trim.zw + particle.trim.xy;
	output.uv *= particle.uvwh.zw + particle.uvwh.xy;
	output.layer = floor( particle.uvwh.x );
	float scale = lerp( particle.start_size, particle.end_size, fage );

	// stretched billboards based on
	// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/emittedparticleVS.hlsl
	float3 view_velocity = mul34( u_View[ 0 ].V, float4( particle.velocity * 0.01f, 0.0f ) ).xyz;
	float3 quadPos = float3( scale * position, 0.0f );
	float angle = particle.angle;
	if( ( particle.flags & ParticleFlag_Rotate ) != 0u ) {
		angle += atan2( view_velocity.x, -view_velocity.y );
	}
	float ca = cos( angle );
	float sa = sin( angle );
	float2x2 rot = float2x2( ca, sa, -sa, ca );
	quadPos.xy = mul( rot, quadPos.xy );
	if( ( particle.flags & ParticleFlag_Stretch ) != 0u ) {
		float3 stretch = dot( quadPos, view_velocity ) * view_velocity;
		quadPos += normalize( stretch ) * clamp( length( stretch ), 0.0f, scale );
	}
	output.world_position = particle.position;
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, float4( particle.position + quadPos, 1.0f ) ) );
	return output;
}

float4 FragmentMain( VertexOutput v ) : FragmentShaderOutput_Albedo {
	// TODO: soft particles
	float4 color = float4( float3( 1.0f, 1.0f, 1.0f ), u_SpriteAtlas.Sample( u_Sampler, float3( v.uv, v.layer ) ).a ) * v.color;
	color.a = FogAlpha( color.a, length( v.world_position - u_View[ 0 ].camera_pos ) );
	color.a = VoidFogAlpha( color.a, v.world_position.z );

	if( color.a < 0.01f )
		discard;

	return color;
}
