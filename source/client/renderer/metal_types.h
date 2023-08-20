// include types.h instead of this

#if PLATFORM_MACOS

namespace MTL {
	class Buffer;
	class SamplerState;
	class Texture;
	class RenderPipelineState;
};

struct Shader {
    const char * name;

    struct Variant {
        VertexDescriptor mesh_format;
        MTL::RenderPipelineState * pso;
    };

    Variant variants[ 8 ];
    size_t num_variants;

    u64 buffers[ 16 ];
    u64 textures[ 4 ];
};

struct GPUBuffer {
	MTL::Buffer * buffer;
};

using SamplerHandle = MTL::SamplerState *;
using TextureHandle = MTL::Texture *;

#endif // #if PLATFORM_MACOS
