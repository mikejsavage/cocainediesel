#if FRAGMENT_SHADER

uniform sampler2D u_BlueNoiseTexture;

vec3 Dither() {
	float noise = texture( u_BlueNoiseTexture, gl_FragCoord.xy / vec2( textureSize( u_BlueNoiseTexture, 0 ) ) ).x;
	return vec3( noise - 0.5 ) / 256.0;
}

#endif
