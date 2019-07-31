in vec2 v_TexCoord;

uniform sampler2D u_BaseTexture;

uniform vec4 u_TextColor;

uniform bool u_HasBorder;
uniform vec4 u_BorderColor;

uniform float u_PixelRange;

float median( vec3 v ) {
	return max( min( v.x, v.y ), min( max( v.x, v.y ), v.z ) );
}

float linearstep( float lo, float hi, float x ) {
	return ( clamp( x, lo, hi ) - lo ) / ( hi - lo );
}

vec4 sample_msdf( vec2 uv, float half_pixel_size ) {
	vec3 sample = qf_texture( u_BaseTexture, uv ).rgb;
	float d = 2.0 * median( sample ) - 1.0; // rescale to [-1,1], positive being inside

	if( u_HasBorder ) {
		float border_amount = linearstep( -half_pixel_size, half_pixel_size, d );
		vec4 color = mix( u_BorderColor, u_TextColor, border_amount );

		float alpha = linearstep( -3.0 * half_pixel_size, -half_pixel_size, d );
		return vec4( color.rgb, color.a * alpha );
	}

	float alpha = linearstep( -half_pixel_size, half_pixel_size, d );
	return vec4( u_TextColor.rgb, u_TextColor.a * alpha );
}

void main() {
	vec2 fw = fwidth( v_TexCoord );
	float half_pixel_size = 0.5 * ( 1.0 / u_PixelRange ) * dot( fw, textureSize( u_BaseTexture, 0 ) );

	float supersample_offset = 0.354; // rsqrt( 2 ) / 2
	vec2 ssx = vec2( supersample_offset * fw.x, 0.0 );
	vec2 ssy = vec2( 0.0, supersample_offset * fw.y );

	vec4 color = sample_msdf( v_TexCoord, half_pixel_size );
	color += 0.5 * sample_msdf( v_TexCoord - ssx, half_pixel_size );
	color += 0.5 * sample_msdf( v_TexCoord + ssx, half_pixel_size );
	color += 0.5 * sample_msdf( v_TexCoord - ssy, half_pixel_size );
	color += 0.5 * sample_msdf( v_TexCoord + ssy, half_pixel_size );

	qf_FragColor = color * ( 1.0 / 3.0 );
}

