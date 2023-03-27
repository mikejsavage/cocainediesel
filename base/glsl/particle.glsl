#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/fog.glsl"
#include "include/particles.glsl"

v2f vec3 v_Position;
v2f vec2 v_TexCoord;
flat v2f float v_Layer;
flat v2f vec4 v_Color;

#if VERTEX_SHADER

#if MODEL
	in vec3 a_Position;
	in vec2 a_TexCoord;
#else
	const vec2 quad_positions[] = {
		vec2( -0.5f, -0.5f ),
		vec2( 0.5f, -0.5f ),
		vec2( -0.5f, 0.5f ),
		vec2( 0.5f, 0.5f ),
	};

	const int index_buffer[] = { 0, 1, 2, 2, 1, 3 };

	vec2 PositionToTexCoord( vec2 p ) {
		return p * vec2( 1.0, -1.0 ) + 0.5;
	}
#endif

layout( std430 ) readonly buffer b_Particles {
	Particle particles[];
};

void main() {
	Particle particle = particles[ gl_InstanceID ];

	float fage = particle.age / particle.lifetime;

	v_Color = mix( sRGBToLinear( particle.start_color ), sRGBToLinear( particle.end_color ), fage );
#if MODEL
	v_TexCoord = a_TexCoord;
#else
	vec2 position = quad_positions[ index_buffer[ gl_VertexID ] ];
	v_TexCoord = PositionToTexCoord( position ) * particle.uvwh.zw + particle.uvwh.xy;
	v_Layer = floor( particle.uvwh.x );
#endif
	float scale = mix( particle.start_size, particle.end_size, fage );

#if MODEL
	vec3 position = particle.position + ( u_M * vec4( a_Position * scale, 1.0 ) ).xyz;

	v_Position = position;
	gl_Position = u_P * u_V * vec4( position, 1.0 );
#else
	// stretched billboards based on
	// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/emittedparticleVS.hlsl
	vec3 view_velocity = ( u_V * vec4( particle.velocity * 0.01, 0.0 ) ).xyz;
	vec3 quadPos = vec3( scale * position, 0.0 );
	float angle = particle.angle;
	if ( ( particle.flags & PARTICLE_ROTATE ) != 0u ) {
		angle += atan( view_velocity.x, -view_velocity.y );
	}
	float ca = cos( angle );
	float sa = sin( angle );
	mat2 rot = mat2( ca, sa, -sa, ca );
	quadPos.xy = rot * quadPos.xy;
	if ( ( particle.flags & PARTICLE_STRETCH ) != 0u ) {
		vec3 stretch = dot( quadPos, view_velocity ) * view_velocity;
		quadPos += normalize( stretch ) * clamp( length( stretch ), 0.0, scale );
	}
	v_Position = particle.position;
	gl_Position = u_P * ( u_V * vec4( particle.position, 1.0 ) + vec4( quadPos, 0.0 ) );
#endif
}

#else

uniform sampler2D u_BaseTexture;
uniform lowp sampler2DArray u_DecalAtlases;

out vec4 f_Albedo;

void main() {
	// TODO: soft particles
	vec4 color;
#if MODEL
	color = texture( u_BaseTexture, v_TexCoord ) * v_Color;
#else
	color = vec4( vec3( 1.0 ), texture( u_DecalAtlases, vec3( v_TexCoord, v_Layer ) ).r ) * v_Color;
#endif
	color.a = FogAlpha( color.a, length( v_Position - u_CameraPos ) );
	color.a = VoidFogAlpha( color.a, v_Position.z );

	if( color.a < 0.01 )
		discard;

	f_Albedo = color;
}

#endif
