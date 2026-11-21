#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] Texture2D< float4 > u_Framebuffer;

float4 VertexMain( [[vk::location( VertexAttribute_Position )]] float4 position : POSITION ) : SV_Position {
	return position;
}

float4 FragmentMain( float4 v : SV_Position ) : FragmentShaderOutput_Albedo {
	return LinearTosRGB( u_Framebuffer.Load( int3( (int)v.x, (int)v.y, 0 ) ) );
}
