#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/fog.glsl"

v2f vec3 v_Position;
v2f vec2 v_TexCoord;
v2f float v_Layer;
v2f vec4 v_Color;

#if VERTEX_SHADER

#if MODEL
	in vec3 a_Position;
#else
	in vec2 a_Position;
#endif
in vec2 a_TexCoord;

in vec4 a_ParticlePosition;
in vec4 a_ParticleVelocity;
in float a_ParticleAccelDragRest;
in vec4 a_ParticleUVWH;
in vec4 a_ParticleStartColor;
in vec4 a_ParticleEndColor;
in vec2 a_ParticleSize;
in vec2 a_ParticleAgeLifetime;
in uint a_ParticleFlags;

uniform sampler2D u_GradientTexture;

layout( std140 ) uniform u_GradientMaterial {
	float u_GradientHalfPixel;
};

// must match source
#define PARTICLE_COLLISION_POINT 1u
#define PARTICLE_COLLISION_SPHERE 2u
#define PARTICLE_ROTATE 4u
#define PARTICLE_STRETCH 8u

void main() {
	float fage = a_ParticleAgeLifetime.x / a_ParticleAgeLifetime.y;

	float uv = mix( u_GradientHalfPixel, fage, 1.0 - u_GradientHalfPixel );
	vec4 gradColor = texture( u_GradientTexture, vec2( uv, 0.5 ) );

	v_Color = sRGBToLinear( mix( a_ParticleStartColor, a_ParticleEndColor, fage ) ) * gradColor;
#if MODEL
	v_TexCoord = a_TexCoord;
#else
	v_TexCoord = a_TexCoord * a_ParticleUVWH.zw + a_ParticleUVWH.xy;
	v_Layer = floor( a_ParticleUVWH.x );
#endif
	float scale = mix( a_ParticleSize.x, a_ParticleSize.y, fage );

#if MODEL
	vec3 position = a_ParticlePosition.xyz + ( u_M * vec4( a_Position * scale, 1.0 ) ).xyz;

	v_Position = position;
	gl_Position = u_P * u_V * vec4( position, 1.0 );
#else
	// stretched billboards based on
	// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/emittedparticleVS.hlsl
	vec3 view_velocity = ( u_V * vec4( a_ParticleVelocity.xyz * 0.01, 0.0 ) ).xyz;
	vec3 quadPos = vec3( scale * a_Position, 0.0 );
	float angle = a_ParticlePosition.w;
	if ( ( a_ParticleFlags & PARTICLE_ROTATE ) != 0u ) {
		angle += atan( view_velocity.x, -view_velocity.y );
	}
	float ca = cos( angle );
	float sa = sin( angle );
	mat2 rot = mat2( ca, sa, -sa, ca );
	quadPos.xy = rot * quadPos.xy;
	if ( ( a_ParticleFlags & PARTICLE_STRETCH ) != 0u ) {
		vec3 stretch = dot( quadPos, view_velocity ) * view_velocity;
		quadPos += normalize( stretch ) * clamp( length( stretch ), 0.0, scale );
	}
	v_Position = a_ParticlePosition.xyz;
	gl_Position = u_P * ( u_V * vec4( a_ParticlePosition.xyz, 1.0 ) + vec4( quadPos, 0.0 ) );
#endif
}

#else

uniform sampler2D u_BaseTexture;
uniform sampler2DArray u_DecalAtlases;

out vec4 f_Albedo;

void main() {
	// TODO: soft particles
	vec4 color;
#if MODEL
	color = texture( u_BaseTexture, v_TexCoord ) * v_Color;
#else
	color = texture( u_DecalAtlases, vec3( v_TexCoord, v_Layer ) ) * v_Color;
#endif
	color.a = FogAlpha( color.a, length( v_Position - u_CameraPos ) );
	color.a = VoidFogAlpha( color.a, v_Position.z );

	f_Albedo = LinearTosRGB( color );
}

#endif
