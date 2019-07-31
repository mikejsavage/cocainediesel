qf_attribute vec4 a_Position;
qf_attribute vec2 a_TexCoord;

qf_varying vec2 v_TexCoord;

void main() {
	gl_Position = a_Position;
	v_TexCoord = a_TexCoord;
}
