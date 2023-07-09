// include types.h instead of this

#if PLATFORM_MACOS

namespace MTL {
	class Buffer;
	class Texture;
};

struct Shader {
	u64 buffers[ 16 ];
	u64 textures[ 4 ];
	u64 uniforms[ 1 ]; // TODO: nuke
};

struct GPUBuffer {
	MTL::Buffer * buffer;
};

using TextureHandle = MTL::Texture *;

#endif // #if PLATFORM_MACOS
