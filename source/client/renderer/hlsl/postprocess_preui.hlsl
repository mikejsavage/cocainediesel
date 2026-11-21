#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 1, DescriptorSet_RenderPass )]] StructuredBuffer< PostprocessPreUIUniforms > u_Postprocess;
[[vk::binding( 2, DescriptorSet_RenderPass )]] Texture2D< float4 > u_Framebuffer;
[[vk::binding( 3, DescriptorSet_RenderPass )]] Texture2D< float4 > u_RGBNoise;
[[vk::binding( 4, DescriptorSet_RenderPass )]] SamplerState u_StandardSampler;

float4 VertexMain( [[vk::location( VertexAttribute_Position )]] float4 position : POSITION ) : SV_Position {
	return position;
}

float minc( float3 c ) { return min( min( c.r, c.g ), c.b ); }
float maxc( float3 c ) { return max( max( c.r, c.g ), c.b ); }

float ExposureLuminance( float3 c ) { return 0.298839f*c.r + 0.586811f*c.g + 0.11435f*c.b; }
float ExposureSaturation( float3 c ) { return maxc( c ) - minc( c ); }

float3 ExposureClipColor( float3 c ) {
	float l = ExposureLuminance( c );
	float n = minc( c );
	float x = maxc( c );

	if( n < 0.0f ) {
		c = max( 0.0f, ( c - l ) * l / ( l - n ) + l );
	}
	if( x > 1.0f ) {
		c = min( 1.0f, ( c - l ) * ( 1.0f - l ) / ( x - l ) + l );
	}

	return c;
}

float3 ExposureSetLuminance( float3 c, float l ) {
	return ExposureClipColor( c + l - ExposureLuminance( c ) );
}

float3 ExposureSetSaturation( float3 c, float s ) {
	float cmin = minc( c );
	float cmax = maxc( c );
	float cDiff = cmax - cmin;

	float3 res = float3( s, s, s );

	if( cmax > cmin ) {
		if( c.r == cmin && c.b == cmax ) { // R < G < B
			res.r = 0.0f;
			res.g = ( ( c.g - cmin ) * s ) / cDiff;
		}
		else if( c.r == cmin && c.g == cmax ) { // R < B < G max
			res.r = 0.0f;
			res.b = ( ( c.b - cmin ) * s ) / cDiff;
		}
		else if( c.g == cmin && c.b == cmax ) { // G < R < B max
			res.r = ( ( c.r - cmin ) * s ) / cDiff;
			res.g = 0.0f;
		}
		else if( c.g == cmin && c.r == cmax ) { // G < B < R max
			res.g = 0.0f;
			res.b = ( ( c.b - cmin ) * s ) / cDiff;
		}
		else if( c.b == cmin && c.r == cmax ) { // B < G < R max
			res.g = ( ( c.g - cmin ) * s ) / cDiff;
			res.b = 0.0f;
		}
		else { // B < R < G max
			res.r = ( ( c.r - cmin ) * s ) / cDiff;
			res.b = 0.0f;
		}
	}

	return res;
}

float ExposureRamp( float t ) {
    t *= 2.0f;
    if( t >= 1.0f ) {
      t -= 1.0f;
      t = log( 0.5f ) / log( 0.5f * ( 1.0f - t ) + 0.9332f * t );
    }
    return clamp( t, 0.001f, 10.0f );
}

float3 ColorCorrection( float3 color ) {
	const float3 LumCoeff = float3( 0.2125f, 0.7154f, 0.0721f );

	float exposure = lerp( 0.009f, 0.98f, u_Postprocess[ 0 ].exposure );
	float gamma = ( u_Postprocess[ 0 ].gamma + 1.0f ) * 0.5f;
	float3 res = lerp( color, 1.0f, exposure );
    float3 blend = lerp( 1.0f, pow( color, 1.0f / 0.7f ), exposure );
    res = max( 1.0f - ( ( 1.0f - res ) / blend ), 0.0f );

    color = pow(
    	ExposureSetLuminance(
    		ExposureSetSaturation( color, ExposureSaturation( res ) ),
    		ExposureLuminance( res )
    	),
    	ExposureRamp( 1.0f - gamma )
    );

	color *= u_Postprocess[ 0 ].brightness;
 	float3 AvgLumin = float3( gamma, gamma, gamma );
 	float3 intensity = dot( color, LumCoeff );

	float3 satColor = lerp( intensity, color, u_Postprocess[ 0 ].saturation );
 	float3 conColor = lerp( AvgLumin, satColor, u_Postprocess[ 0 ].contrast );

	return conColor;
}

float3 radialBlur( float2 uv ) {
	const int SAMPLES = 16;
	const float BLUR_INTENSITY = 0.03f;
	const float2 CENTER = float2( 0.5f, 0.5f );

	float3 col = 0.0f;
	float2 dist = uv - CENTER;

	float len = sqrt( length( dist ) );
	for( int j = 0; j < SAMPLES; j++ ) {
		float scale = 1.0f - ( len * BLUR_INTENSITY * u_Postprocess[ 0 ].radial_blur * j ) / SAMPLES;
		col += u_Framebuffer.Sample( u_StandardSampler, saturate( dist * scale + CENTER ) ).rgb;
	}

	return col / SAMPLES;
}

float IGNDither( float2 pos, float intensity ) {
	return ( fmod( 52.9829189f * fmod( 0.06711056f * pos.x + 0.00583715f * pos.y, 1.0f ), 1.0f ) - 0.5f ) * intensity;
}

float Vignette( float2 uv ) {
	const float VIGNETTE_OFFSET = 0.45f;

	float len = length( uv - float2( 0.5f, 0.5f ) ) + IGNDither( uv * u_View[ 0 ].viewport_size, 0.02f );
	return 1.0 - max( len - VIGNETTE_OFFSET, 0.0f ) * ( 1.0f + u_Postprocess[ 0 ].vignette );
}

float4 FragmentMain( float4 v : SV_Position ) : FragmentShaderOutput_Albedo {
	float2 uv = v.xy / u_View[ 0 ].viewport_size;

	float3 color = radialBlur( uv );
	color = ColorCorrection( color );
	color *= Vignette( uv );

	return float4( color, 1.0f );
}
