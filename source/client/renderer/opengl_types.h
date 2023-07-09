// include types.h instead of this

#if !PLATFORM_MACOS

struct Shader {
	u32 program;
	u64 uniforms[ 8 ];
	u64 textures[ 4 ];
	u64 buffers[ 8 ];
};

struct GPUBuffer {
	u32 buffer;
};

using TextureHandle = u32;

#endif // #if !PLATFORM_MACOS
