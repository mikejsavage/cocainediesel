#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/dither.glsl"
#include "include/fog.glsl"

v2f vec3 v_Position;
v2f float v_Height;

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	v_Position = a_Position.xyz;
	gl_Position = u_P * u_V * vec4( a_Position );
}

#else

layout( std140 ) uniform u_Time {
	float u_T;
};
uniform sampler2D u_Noise;

out vec3 f_Albedo;

float value( vec2 p ) {
  vec2 f = floor( p );
  vec2 s = ( p - f );
  s *= s * ( 3.0 - 2.0 * s );
  return texture( u_Noise, ( f + s - 0.5 ) / 256.0 ).r;
}

void main() {
  float time = u_T * 1.0;
  vec2 uv = v_Position.xy / v_Position.z;

  vec3 cloud_color = 0.01 * vec3( 1.0, 1.0, 1.0 );
  vec3 sky_color = 0.06 * vec3( 1.0, 1.0, 1.0 );
  vec2 wind = vec2( 0.3, -0.3 );
  float iterations = 4.0;

  vec2 c = uv;
  vec2 h = vec2( 0.0 );
  float a = 1.0;
  float s = 1.0;
  for ( float i = 0.0; i < iterations; i++ ) {
    a *= 2.2;
    s *= 2.0;
    vec2 v_uv = uv * s;
    v_uv += uv * i * 0.5; // parallax
    v_uv += wind * time; // movement
    v_uv += h.x * a / s * vec2( 4.0, 7.0 ); // warping
    h += vec2( value( v_uv ), 1.0 ) / a;
  }
  float g = -h.x / h.y;
  float n = smoothstep( 0.0, 0.5, v_Position.z );
  float m = smoothstep( 30.0, 0.0, length( uv ) );

  vec3 color = mix( sky_color, cloud_color, exp( g ) * n * m );

  color = VoidFog( color, gl_FragCoord.xy );

  f_Albedo = LinearTosRGB( color + Dither() );
}

#endif
