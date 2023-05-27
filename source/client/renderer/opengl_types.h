// include types.h instead of this

#if !PLATFORM_MACOS

struct Shader {
	u32 program;
	u64 uniforms[ 8 ];
	u64 textures[ 4 ];
	u64 texture_arrays[ 2 ];
	u64 buffers[ 8 ];
};

struct GPUBuffer {
	u32 buffer;
};

struct UniformBlock {
	u32 ubo;
	u32 offset;
	u32 size;
};

struct Mesh {
	u32 vao;
	GPUBuffer vertex_buffers[ VertexAttribute_Count ];
	GPUBuffer index_buffer;

	IndexFormat index_format;
	u32 num_vertices;
	bool cw_winding;
};

struct Texture {
	u32 texture;
	u32 width, height;
	u32 num_mipmaps;
	bool msaa;
	TextureFormat format;
};

struct TextureArray {
	u32 texture;
};

#endif // #if !PLATFORM_MACOS
