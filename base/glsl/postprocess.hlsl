#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 1, DescriptorSet_RenderPass )]] StructuredBuffer< PostprocessUniforms > u_Postprocess;
[[vk::binding( 2, DescriptorSet_RenderPass )]] Texture2D< float4 > u_Framebuffer;
[[vk::binding( 3, DescriptorSet_RenderPass )]] Texture2D< float4 > u_RGBNoise;
[[vk::binding( 4, DescriptorSet_RenderPass )]] SamplerState u_StandardSampler;

float4 VertexMain( [[vk::location( VertexAttribute_Position )]] float4 position : POSITION ) : SV_Position {
	return position;
}

static const float SPEED = 0.025f; // 0.1
static const float ABERRATION = 1.0f; // 1.0
static const float STRENGTH = 1.0f; // 1.0

static const float MPEG_BLOCKSIZE = 4.0f; // 16.0
static const float MPEG_OFFSET = 0.3f; // 0.3
static const float MPEG_INTERLEAVE = 8.0f; // 3.0

static const float MPEG_BLOCKS = 0.1f; // 0.2
static const float MPEG_BLOCK_LUMA = 0.1f; // 0.5
static const float MPEG_BLOCK_INTERLEAVE = 0.6f; // 0.6

static const float MPEG_LINES = 0.1f; // 0.7
static const float MPEG_LINE_DISCOLOR = 1.0f; // 0.3
static const float MPEG_LINE_INTERLEAVE = 0.4f; // 0.4

static const float LENS_DISTORT = 1.5f; // -1.0

float2 lensDistort( float2 p, float power ) {
	float2 new_p = p - 0.5f;
	float rsq = new_p.x * new_p.x + new_p.y * new_p.y;
	new_p += new_p * ( power * rsq );
	new_p *= 1.0f - ( 0.25f * power );
	new_p += 0.5f;
	new_p = saturate( new_p );
	return lerp( p, new_p, step( abs( p - 0.5f ), Broadcast2( 0.5f ) ) );
}

float4 noise( float2 p ) {
	return u_RGBNoise.Sample( u_StandardSampler, p );
}

float fnoise( float2 v ) {
	return frac( sin( dot( v, float2( 12.9898f, 78.233f ) ) ) * 43758.5453f );
}

float4 float4pow( float4 v, float p ) {
	return float4( pow( v.xyz, Broadcast3( p ) ), v.w );
}

float3 interleave( float2 uv ) {
	float row = frac( uv.y / MPEG_INTERLEAVE );
	float3 mask = float3( MPEG_INTERLEAVE, 0.0f, 0.0f );
	if( row > 0.333f )
		mask = float3( 0.0f, MPEG_INTERLEAVE, 0.0f );
	if( row > 0.666f )
		mask = float3( 0.0f, 0.0f, MPEG_INTERLEAVE );

	return mask;
}

float3 glitch( float2 uv, float amount ) {
	float time = u_Postprocess[ 0 ].time * SPEED;

	float2 distorted_fragCoord = uv * u_View[ 0 ].viewport_size;

	float2 block = floor( distorted_fragCoord / MPEG_BLOCKSIZE );
	float2 uv_noise = block / Broadcast2( 64.0f ) + floor( Broadcast2( time ) * float2( 1234.0f, 3543.0f ) ) / Broadcast2( 64.0f );

	float block_thresh = pow( frac( time * 1236.0453f ), 2.0f ) * MPEG_BLOCKS * amount;
	float line_thresh = pow( frac( time * 2236.0453f ), 3.0f ) * MPEG_LINES * amount;

	float2 uv_r = uv;
	float2 uv_g = uv;
	float2 uv_b = uv;

	float4 blocknoise = noise( uv_noise );
	float4 linenoise = noise( float2( uv_noise.y, 0.0f ) );

	// glitch some blocks and lines / chromatic aberration
	if( blocknoise.r < block_thresh || linenoise.g < line_thresh ) {
		float2 dist = ( frac( uv_noise ) - 0.5f ) * MPEG_OFFSET * ABERRATION;
		uv_r += dist * 0.1f;
		uv_g += dist * 0.2f;
		uv_b += dist * 0.125f;
	}
	else { // remainder gets just chromatic aberration
		float4 power = float4pow( noise( float2( time, time * 0.08f ) ), 8.0f );
		float4 shift = power * float4( Broadcast3( amount * ABERRATION ), 1.0f );
		shift *= 2.0f * shift.w - 1.0f;
		uv_r += float2( shift.x, -shift.y );
		uv_g += float2( shift.y, -shift.z );
		uv_b += float2( shift.z, -shift.x );
	}

	float3 col = float3(
		u_Framebuffer.Sample( u_StandardSampler, uv_r ).r,
		u_Framebuffer.Sample( u_StandardSampler, uv_g ).g,
		u_Framebuffer.Sample( u_StandardSampler, uv_b ).b
	);

	// lose luma for some blocks
	if( blocknoise.g < block_thresh * MPEG_BLOCK_LUMA ) {
		col = col.ggg;
	}

	// discolor block lines
	if( linenoise.b < line_thresh * MPEG_LINE_DISCOLOR ) {
		col = Broadcast3( 0.0f );
	}

	// interleave lines in some blocks
	if( blocknoise.g < block_thresh * MPEG_BLOCK_INTERLEAVE || linenoise.g < line_thresh * MPEG_LINE_INTERLEAVE ) {
		col *= interleave( distorted_fragCoord );
	}

	return col;
}

float roundBox( float2 p, float2 b, float r ) {
	return length( max( abs( p ) - b, 0.0f ) ) - r;
}

float crt_on_screen( float2 uv ) {
	float dist_screen = roundBox( uv, Broadcast2( 1.0f ), 0.0f ) * 150.0f;
	return 1.0f - saturate( dist_screen );
}

float2 crtDistort( float2 uv, float amount ) {
	float2 screen_uv = uv;

	// screen distort
	screen_uv = screen_uv * 2.0f - 1.0f;
	screen_uv *= float2( 1.11f, 1.14f ) * 0.97f;
	float bend = 4.0f;
	screen_uv.x *= 1.0f + pow( abs( screen_uv.y ) / bend, 2.0f );
	screen_uv.y *= 1.0f + pow( abs( screen_uv.x ) / bend, 2.0f );

	float on_screen = crt_on_screen( screen_uv );

	screen_uv = lerp( screen_uv, clamp( screen_uv, -1.0f, 1.0f ), on_screen );

	screen_uv = screen_uv * 0.5f + 0.5f;

	// horizontal fuzz
	screen_uv.x += fnoise( float2( u_Postprocess[ 0 ].time * 15.0f, uv.y * 80.0f ) ) * 0.002f * on_screen;

	uv = lerp( uv, screen_uv, amount );

	return uv;
}

float3 crtEffect( float3 color, float2 uv, float amount ) {
	const float3 edge_color = float3( 0.3f, 0.25f, 0.3f ) * 0.01f;
	float3 new_color = color;

	// screen edge
	float2 screen_uv = uv * 2.0f - 1.0f;

	float dist_in = roundBox( screen_uv, float2( 1.0f, 1.03f ), 0.05f ) * 150.0f;
	dist_in = saturate( dist_in );

	float on_screen = crt_on_screen( screen_uv );

	float light = dot( screen_uv, Broadcast2( -1.0f ) );
	light = smoothstep( -0.05f, 0.2f, saturate( light ) );
	// return float3( light );

	float3 screen_edge = lerp( ( edge_color + light * 0.03f ) * dist_in, Broadcast3( 0.0f ), 0.0f );

	// banding
	float banding = 0.6f + frac( uv.y + fnoise( uv ) * 0.05f - u_Postprocess[ 0 ].time * 0.3f ) * 0.4f;
	new_color = lerp( screen_edge, new_color, banding * on_screen );

	// scanlines
	float scanline = abs( sin( u_Postprocess[ 0 ].time * 10.0f - uv.y * 400.0f ) ) * 0.01f * on_screen;
	new_color -= scanline;

	new_color = lerp( color, new_color, amount );
	return lerp( new_color, screen_edge, 1.0f - on_screen );
}

float3 brightnessContrast( float3 value ) {
	return ( value - 0.5f ) * u_Postprocess[ 0 ].contrast + 0.5f + u_Postprocess[ 0 ].brightness;
}

float4 FragmentMain( float4 v : SV_Position ) : FragmentShaderOutput_Albedo {
	float2 uv = v.xy / u_View[ 0 ].viewport_size;

	float crt_amount = u_Postprocess[ 0 ].crt;
	float glitch_amount = u_Postprocess[ 0 ].damage * STRENGTH;
	uv = crtDistort( uv, crt_amount );
	uv = lensDistort( uv, glitch_amount * LENS_DISTORT );

	float3 color = glitch( uv, glitch_amount );
	color = crtEffect( color, uv, crt_amount );
	if( all( abs( uv - 0.5f ) <= Broadcast2( 0.5f ) ) ) {
		color = sRGBToLinear( brightnessContrast( LinearTosRGB( color ) ) );
	}
	return float4( color, 1.0f );
}
