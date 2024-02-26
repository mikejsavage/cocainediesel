#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/fog.glsl"
#include "include/particles.glsl"

#define TRIM 1

v2f vec3 v_Position;
v2f vec2 v_TexCoord;
flat v2f float v_Layer;
flat v2f vec4 v_Color;

#if VERTEX_SHADER

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

layout( std430 ) readonly buffer b_Particles {
	Particle particles[];
};

void main() {
	Particle particle = particles[ gl_InstanceID ];

	float fage = particle.age / particle.lifetime;

	v_Color = mix( sRGBToLinear( particle.start_color ), sRGBToLinear( particle.end_color ), fage );
	vec2 position = quad_positions[ index_buffer[ gl_VertexID ] ];
	v_TexCoord = PositionToTexCoord( position );
#if TRIM
	position += 0.5;
	position = position * particle.trim.zw + particle.trim.xy;
	position -= 0.5;
	v_TexCoord = v_TexCoord * particle.trim.zw + particle.trim.xy;
#endif
	v_TexCoord = v_TexCoord * particle.uvwh.zw + particle.uvwh.xy;
	v_Layer = floor( particle.uvwh.x );
	float scale = mix( particle.start_size, particle.end_size, fage );

	// stretched billboards based on
	// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/emittedparticleVS.hlsl
	vec3 view_velocity = ( AffineToMat4( u_V ) * vec4( particle.velocity * 0.01, 0.0 ) ).xyz;
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
	gl_Position = u_P * ( AffineToMat4( u_V ) * vec4( particle.position, 1.0 ) + vec4( quadPos, 0.0 ) );
}

#else

uniform sampler2D u_BaseTexture;
uniform sampler2DArray u_DecalAtlases;

layout( location = FragmentShaderOutput_Albedo ) out vec4 f_Albedo;

void main() {
	// TODO: soft particles
	vec4 color;
	color = vec4( vec3( 1.0 ), texture( u_DecalAtlases, vec3( v_TexCoord, v_Layer ) ).a ) * v_Color;
	color.a = FogAlpha( color.a, length( v_Position - u_CameraPos ) );
	color.a = VoidFogAlpha( color.a, v_Position.z );

	if( color.a < 0.01 )
		discard;

	f_Albedo = color;
}

#endif
