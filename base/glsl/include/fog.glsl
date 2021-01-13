layout( std140 ) uniform u_Fog {
	float u_FogStrength;
};

vec3 Fog( vec3 color, float dist ) {
	vec3 fog_color = vec3( 0.0 );
	float fog_amount = 1.0 - exp( -u_FogStrength * dist );
	return mix( color, fog_color, fog_amount );
}

float FogAlpha( float color, float dist ) {
	float fog_color = 0.0;
	float fog_amount = 1.0 - exp( -u_FogStrength * dist );
	return mix( color, fog_color, fog_amount );
}

#define VOID_FADE_START -600.0
#define VOID_FADE_END -1300.0

vec3 VoidFog( vec3 color, float height ) {
	vec3 void_color = vec3( 0.01 );
	float void_amount = smoothstep( VOID_FADE_START, VOID_FADE_END, height );
	return mix( color, void_color, void_amount );
}

float VoidFogAlpha( float alpha, float height ) {
	float void_amount = 1.0 - smoothstep( VOID_FADE_START, VOID_FADE_END, height );
	return alpha * void_amount;
}

vec3 VoidFog( vec3 color, vec2 frag_coord, float depth = 0.999 ) {
	vec4 clip = vec4( vec3( frag_coord / u_ViewportSize, depth ) * 2.0 - 1.0, 1.0 );
	vec4 world = u_InverseP * clip;
	float height = ( u_InverseV * ( world / world.w ) ).z;
	return VoidFog( color, height );
}

float VoidFogAlpha( float alpha, vec2 frag_coord, float depth = 0.999 ) {
	vec4 clip = vec4( vec3( frag_coord / u_ViewportSize, depth ) * 2.0 - 1.0, 1.0 );
	vec4 world = u_InverseP * clip;
	float height = ( u_InverseV * ( world / world.w ) ).z;
	return VoidFogAlpha( alpha, height );
}
