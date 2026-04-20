#pragma once

float3 Dither( Texture2D< float4 > blue_noise, SamplerState standard_sampler, float2 screen_uv ) {
	float2 texture_size;
	blue_noise.GetDimensions( texture_size.x, texture_size.y );
	float n = blue_noise.Sample( standard_sampler, screen_uv / texture_size ).x;
	return ( n - 0.5f ) / 256.0f;
}
