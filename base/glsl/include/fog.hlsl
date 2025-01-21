#pragma once

static const float FOG_STRENGTH = 0.0007f;

float3 Fog( float3 color, float dist ) {
	float3 fog_color = 0.0f;
	float fog_amount = 1.0f - exp( -FOG_STRENGTH * dist );
	return lerp( color, fog_color, fog_amount );
}

float FogAlpha( float color, float dist ) {
	float fog_color = 0.0f;
	float fog_amount = 1.0f - exp( -FOG_STRENGTH * dist );
	return lerp( color, fog_color, fog_amount );
}

static const float VOID_FADE_START = -600.0f;
static const float VOID_FADE_END = -1300.0f;

float3 VoidFog( float3 color, float height ) {
	float3 void_color = 0.01f;
	float void_amount = smoothstep( VOID_FADE_START, VOID_FADE_END, height );
	return lerp( color, void_color, void_amount );
}

float VoidFogAlpha( float alpha, float height ) {
	float void_amount = 1.0f - smoothstep( VOID_FADE_START, VOID_FADE_END, height );
	return alpha * void_amount;
}

float3 VoidFog( float3 color, float2 frag_coord, float depth ) {
	float4 clip_pos = float4( float3( frag_coord / u_View[ 0 ].viewport_size, depth ) * 2.0f - 1.0f, 1.0f );
	float4 world = mul( u_View[ 0 ].inverse_P, clip_pos );
	float height = mul( u_View[ 0 ].inverse_V, world / world.w ).z;
	return VoidFog( color, height );
}

float3 VoidFog( float3 color, float2 frag_coord ) {
	return VoidFog( color, frag_coord, 0.999f );
}

float VoidFogAlpha( float alpha, float2 frag_coord, float depth ) {
	float4 clip_pos = float4( float3( frag_coord / u_View[ 0 ].viewport_size, depth ) * 2.0f - 1.0f, 1.0f );
	float4 world = mul( u_View[ 0 ].inverse_P, clip_pos );
	float height = mul( u_View[ 0 ].inverse_V, world / world.w ).z;
	return VoidFogAlpha( alpha, height );
}

float VoidFogAlpha( float alpha, float2 frag_coord ) {
	return VoidFogAlpha( alpha, frag_coord, 0.999f );
}
