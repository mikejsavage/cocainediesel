#if FRAGMENT_SHADER

uniform sampler2D u_BlueNoiseTexture;

layout( std140 ) uniform u_BlueNoiseTextureParams {
	vec2 u_BlueNoiseTextureSize;
};

vec3 Dither() {
	vec3 noise = texture( u_BlueNoiseTexture, gl_FragCoord.xy / u_BlueNoiseTextureSize ).xxx;
	return ( noise - vec3( 0.5 ) ) / 16.0;
}

#endif
