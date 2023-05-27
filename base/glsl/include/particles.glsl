// this must match c++
struct Particle {
	vec3 position;
	float angle;
	vec3 velocity;
	float angular_velocity;
	float acceleration;
	float drag;
	float restitution;
	float PADDING;
	vec4 uvwh;
	uint start_color;
	uint end_color;
	float start_size;
	float end_size;
	float age;
	float lifetime;
	uint flags;
	uint PADDING2;
};
