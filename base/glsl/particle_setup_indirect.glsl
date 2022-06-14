layout( local_size_x = 1 ) in;

layout( std140 ) uniform u_ParticleUpdate {
	uint new_particles;
};

layout( std430 ) writeonly buffer b_NextComputeCount {
	uint next_num_particles;
};

layout( std430 ) coherent buffer b_ComputeCount {
	uint num_particles;
};

struct DispatchIndirectCommand {
	uint num_groups_x;
	uint num_groups_y;
	uint num_groups_z;
};

layout( std430 ) writeonly buffer b_ComputeIndirect {
	DispatchIndirectCommand compute_indirect;
};

struct DrawElementsIndirectCommand {
	uint count;
	uint instanceCount;
	uint firstIndex;
	uint baseVertex;
	uint baseInstance;
};

layout( std430 ) writeonly buffer b_DrawIndirect {
	DrawElementsIndirectCommand draw_indirect;
};

void main() {
	num_particles += new_particles;
	next_num_particles = 0;
	compute_indirect.num_groups_x = num_particles / 64 + 1;
	draw_indirect.instanceCount = num_particles;
}
