#include <string>

static std::string shader_v2f_c4f_t2f_vert() {
  return R"(#version 150

// Program Uniforms
layout( std140 ) uniform u_State { vec4 State; };
layout( std140 ) uniform u_Transform { mat4 Transform; };
layout( std140 ) uniform u_Scalar4 { vec4 Scalar4[2]; };
layout( std140 ) uniform u_Vector { vec4 Vector[8]; };
layout( std140 ) uniform u_ClipSize { uint ClipSize; };
layout( std140 ) uniform u_Clip { mat4 Clip[8]; };

// Uniform Accessor Functions
float Time() { return State[0]; }
float ScreenWidth() { return State[1]; }
float ScreenHeight() { return State[2]; }
float ScreenScale() { return State[3]; }
float Scalar(uint i) { if (i < 4u) return Scalar4[0][i]; else return Scalar4[1][i - 4u]; }
vec4 sRGBToLinear(vec4 val) { return vec4(val.xyz * (val.xyz * (val.xyz * 0.305306011 + 0.682171111) + 0.012522878), val.w); }

// Vertex Attributes
in vec2 in_Position;
in vec4 in_Color;
in vec2 in_TexCoord;

// Out Params
out vec4 ex_Color;
out vec2 ex_ObjectCoord;
out vec2 ex_ScreenCoord;

void main(void)
{
  ex_ObjectCoord = in_TexCoord;
  gl_Position = Transform * vec4(in_Position, 0.0, 1.0);
  ex_Color = sRGBToLinear(in_Color);
}

)";
}
