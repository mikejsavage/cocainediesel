#include "include/uniforms.glsl"

layout( set = DescriptorSet_Material ) uniform sampler2D u_BaseTexture;

layout( std140, set = DescriptorSet_DrawCall ) uniform u_Text {
	vec4 u_TextColor;
	vec4 u_BorderColor;
	float u_dSDF_dTexel;
	int u_HasBorder;
};

v2f vec2 v_TexCoord;

#if VERTEX_SHADER

layout( location = VertexAttribute_Position ) in vec4 a_Position;
layout( location = VertexAttribute_TexCoord ) in vec2 a_TexCoord;

void main() {
	gl_Position = u_P * a_Position;
	v_TexCoord = a_TexCoord;
}

#else

layout( location = FragmentShaderOutput_Albedo ) out vec4 f_Albedo;

float Median( vec3 v ) {
	return max( min( v.x, v.y ), min( max( v.x, v.y ), v.z ) );
}

float LinearStep( float lo, float hi, float x ) {
	return ( clamp( x, lo, hi ) - lo ) / ( hi - lo );
}

vec4 SampleMSDF( vec2 uv, float half_pixel_size ) {
	float d = 2.0 * Median( texture( u_BaseTexture, uv ).rgb ) - 1.0; // rescale to [-1,1], positive being inside

	if( u_HasBorder != 0 ) {
		float border_amount = LinearStep( -half_pixel_size, half_pixel_size, d );
		vec4 color = mix( u_BorderColor, u_TextColor, border_amount );

		float alpha = LinearStep( -3.0 * half_pixel_size, -half_pixel_size, d );
		return vec4( color.rgb, color.a * alpha );
	}

	float alpha = LinearStep( -half_pixel_size, half_pixel_size, d );
	return vec4( u_TextColor.rgb, u_TextColor.a * alpha );
}

void main() {
	vec2 fw = fwidth( v_TexCoord );
	float half_pixel_size = 0.5 * u_dSDF_dTexel * dot( fw, textureSize( u_BaseTexture, 0 ) );

	float supersample_offset = 0.354; // rsqrt( 2 ) / 2
	vec2 ssx = vec2( supersample_offset * fw.x, 0.0 );
	vec2 ssy = vec2( 0.0, supersample_offset * fw.y );

	vec4 color = SampleMSDF( v_TexCoord, half_pixel_size );
	color += 0.5 * SampleMSDF( v_TexCoord - ssx, half_pixel_size );
	color += 0.5 * SampleMSDF( v_TexCoord + ssx, half_pixel_size );
	color += 0.5 * SampleMSDF( v_TexCoord - ssy, half_pixel_size );
	color += 0.5 * SampleMSDF( v_TexCoord + ssy, half_pixel_size );

	f_Albedo = color * ( 1.0 / 3.0 );
}

#endif
