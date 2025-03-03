const float FOG_STRENGTH = 0.0007;

vec3 Fog( vec3 color, float dist ) {
	vec3 fog_color = vec3( 0.0 );
	float fog_amount = 1.0 - exp( -FOG_STRENGTH * dist );
	return mix( color, fog_color, fog_amount );
}

float FogAlpha( float color, float dist ) {
	float fog_color = 0.0;
	float fog_amount = 1.0 - exp( -FOG_STRENGTH * dist );
	return mix( color, fog_color, fog_amount );
}

const float VOID_FADE_START = -600.0;
const float VOID_FADE_END = -1300.0;

vec3 VoidFog( vec3 color, float height ) {
	vec3 void_color = vec3( 0.01 );
	float void_amount = smoothstep( VOID_FADE_START, VOID_FADE_END, height );
	return mix( color, void_color, void_amount );
}

float VoidFogAlpha( float alpha, float height ) {
	float void_amount = 1.0 - smoothstep( VOID_FADE_START, VOID_FADE_END, height );
	return alpha * void_amount;
}

vec3 VoidFog( vec3 color, vec2 frag_coord, float depth ) {
	vec4 clip = vec4( vec3( frag_coord / u_ViewportSize, depth ) * 2.0 - 1.0, 1.0 );
	vec4 world = u_InverseP * clip;
	float height = ( AffineToMat4( u_InverseV ) * ( world / world.w ) ).z;
	return VoidFog( color, height );
}

vec3 VoidFog( vec3 color, vec2 frag_coord ) {
	return VoidFog( color, frag_coord, 0.999 );
}

float VoidFogAlpha( float alpha, vec2 frag_coord, float depth ) {
	vec4 clip = vec4( vec3( frag_coord / u_ViewportSize, depth ) * 2.0 - 1.0, 1.0 );
	vec4 world = u_InverseP * clip;
	float height = ( AffineToMat4( u_InverseV ) * ( world / world.w ) ).z;
	return VoidFogAlpha( alpha, height );
}

float VoidFogAlpha( float alpha, vec2 frag_coord ) {
	return VoidFogAlpha( alpha, frag_coord, 0.999 );
}
