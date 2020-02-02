#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/dither.glsl"

v2f vec3 v_Position;

layout( std140 ) uniform u_Sky {
	vec3 u_EquatorColor;
	vec3 u_PoleColor;
};

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	v_Position = a_Position.xyz;
	gl_Position = u_P * u_V * vec4( a_Position );
}

#else

out vec3 f_Albedo;

vec3 permute(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }

float snoise(vec2 v){
  const vec4 C = vec4(0.211324865405187, 0.366025403784439,
           -0.577350269189626, 0.024390243902439);
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);
  vec2 i1;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;
  i = mod(i, 289.0);
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
  + i.x + vec3(0.0, i1.x, 1.0 ));
  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy),
    dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}

void main() {
	vec3 normal_pos = normalize( v_Position );
	float elevation = acos( normal_pos.z ) / ( M_PI * 0.5 );
	float latitude = atan( v_Position.y, v_Position.x );

	float r = elevation * 2.0;
	vec2 movement = vec2( distance( vec3( 10000.0 ), u_CameraPos ) * 0.0005 );
	float n = snoise( vec2( sin( latitude ), cos( latitude ) ) * r + movement );
	n += snoise( vec2( sin( latitude ), cos( latitude ) ) * r * 4.0 + movement * 0.5 ) * 1.0;
	n += snoise( vec2( sin( latitude ), cos( latitude ) ) * r * 8.0 + movement * 0.25 ) * 0.5;
	float factor = elevation + 0.02 * n;
	float lines = 10.0;
	float value = floor( factor * lines ) / lines;
	value -= 0.25;
	float smoothness = mod( factor, 1.0 / lines ) * elevation * 0.25;
	smoothness = clamp( smoothness, 0.0, 1.0 );
	value += smoothness;
	value = clamp( value, 0.0, 1.0 );
	value = 1.0 - value;
	value = abs( value ) * 0.1;
	value = clamp( value, 0.0, 1.0 );

	vec3 color = vec3( value, value, value );

	f_Albedo = LinearTosRGB( color + Dither() );
}

#endif
