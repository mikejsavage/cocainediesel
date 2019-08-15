uniform sampler2D u_BlueNoiseTexture;
uniform vec2 u_BlueNoiseTextureSize;

vec3 Dither() {
	vec3 noise = qf_texture( u_BlueNoiseTexture, gl_FragCoord.xy / u_BlueNoiseTextureSize ).xxx;
	return ( noise - vec3( 0.5 ) ) / 256.0;
}
