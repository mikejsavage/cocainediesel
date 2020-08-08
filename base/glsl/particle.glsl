#include "include/uniforms.glsl"
#include "include/common.glsl"

v2f vec4 v_UVWH;
v2f vec2 v_TexCoord;
v2f vec4 v_Color;

#if VERTEX_SHADER

#if MODEL
	in vec3 a_Position;
#else
	in vec2 a_Position;
#endif
in vec2 a_TexCoord;

in vec3 a_ParticlePosition;
in vec3 a_ParticleVelocity;
in vec4 a_ParticleUVWH;
in vec4 a_ParticleStartColor;
in vec4 a_ParticleEndColor;
in vec2 a_ParticleSize;
in vec2 a_ParticleAgeLifetime;

uniform sampler2D u_GradientTexture;

layout( std140 ) uniform u_GradientMaterial {
	float u_GradientHalfPixel;
};

void main() {
	float fage = a_ParticleAgeLifetime.x / a_ParticleAgeLifetime.y;

	float uv = mix( u_GradientHalfPixel, fage, 1.0 - u_GradientHalfPixel );
	vec4 gradColor = texture( u_GradientTexture, vec2( uv, 0.5 ) );

	v_Color = sRGBToLinear( mix( a_ParticleStartColor, a_ParticleEndColor, fage ) ) * gradColor;
	v_TexCoord = a_TexCoord;
	v_UVWH = a_ParticleUVWH;
	float scale = mix( a_ParticleSize.x, a_ParticleSize.y, fage );

#if MODEL
	vec3 position = a_ParticlePosition + ( u_M * vec4( a_Position * scale, 1.0 ) ).xyz;

	gl_Position = u_P * u_V * vec4( position, 1.0 );
#else
	// stretched billboards based on
	// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/emittedparticleVS.hlsl
	vec3 view_velocity = ( u_V * vec4( a_ParticleVelocity * 0.01, 0.0 ) ).xyz;
	float angle = atan( view_velocity.x, -view_velocity.y );
	float ca = cos( angle );
	float sa = sin( angle );
	mat2 rot = mat2( ca, sa, -sa, ca );
	vec3 quadPos = vec3( scale * rot * a_Position, 0.0 );
	vec3 stretch = dot( quadPos, view_velocity ) * view_velocity;
	quadPos += normalize( stretch ) * clamp( length( stretch ), 1.0, scale );
	gl_Position = u_P * ( u_V * vec4( a_ParticlePosition, 1.0 ) + vec4( quadPos, 0.0 ) );
#endif
}

#else

// uniform sampler2D u_BaseTexture;
uniform sampler2DArray u_DecalAtlases;

out vec4 f_Albedo;

void main() {
	// TODO: soft particles
	vec2 uv = v_TexCoord * v_UVWH.zw + v_UVWH.xy;
	float layer = floor( v_UVWH.x );
	vec4 sample = texture( u_DecalAtlases, vec3( uv, layer ) );
	f_Albedo = LinearTosRGB( sample * v_Color );
}

#endif
