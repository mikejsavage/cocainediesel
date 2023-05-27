// include types.h instead of this

#if PLATFORM_MACOS

namespace MTL {
	class Buffer;
	class Texture;
};

struct Shader {
	u64 buffers[ 16 ];
	u64 textures[ 4 ];
	u64 texture_arrays[ 2 ];
	u64 uniforms[ 1 ];
};

struct GPUBuffer {
	MTL::Buffer * buffer;
};

struct UniformBlock {
	// TODO
	u32 ubo;
	u32 offset;
	u32 size;
};

struct Mesh {
	GPUBuffer vertex_buffers[ VertexAttribute_Count ];
	GPUBuffer index_buffer;

	IndexFormat index_format;
	u32 num_vertices;
	bool cw_winding;
};

struct Texture {
	MTL::Texture * texture;
	u32 width, height;
	u32 num_mipmaps;
	bool msaa;
	TextureFormat format;
};

struct TextureArray {
	MTL::Texture * texture;
};

#endif // #if PLATFORM_MACOS
