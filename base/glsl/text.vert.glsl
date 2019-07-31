in vec4 a_Position;
in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
	gl_Position = a_Position;
	v_TexCoord = a_TexCoord;
}
