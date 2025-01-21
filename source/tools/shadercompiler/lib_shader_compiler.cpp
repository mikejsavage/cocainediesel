#include "tools/shadercompiler/api.h"

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/threadpool.h"
#include "gameshared/q_shared.h"

#include "client/renderer/shader_shared.h"
#include "client/renderer/shader_variants.h"
#include "client/renderer/spirv.h"

static constexpr Span< const char > ExeExtension = IFDEF( PLATFORM_WINDOWS ) ? ".exe"_sp : ""_sp;
static constexpr Span< const char > ShaderExtension = IFDEF( PLATFORM_MACOS ) ? ".metallib"_sp : ".spv"_sp;

struct CompileShaderJob {
	const CompileShadersSettings * settings;
	bool graphics;
	Span< const char > src;
	Span< Span< const char > > features;

	bool ok;
};

static Optional< u32 > GetSPVInputHash( TempAllocator & temp, const char * path ) {
	FILE * f = OpenFile( &temp, path, OpenFile_Read );
	if( f == NULL ) {
		return NONE;
	}
	defer { fclose( f ); };

	SPIRVHeader header;
	size_t n;
	if( !ReadPartialFile( f, &header, sizeof( header ), &n ) || n != sizeof( header ) ) {
		return NONE;
	}

	return header.generator;
}

static void WriteSPVInputHash( TempAllocator & temp, const char * path, u32 hash ) {
	FILE * f = OpenFile( &temp, path, OpenFile_AppendExisting );
	if( f == NULL ) {
		Fatal( "Can't open %s", path );
	}
	defer { fclose( f ); };

	if( FileSize( f ) < sizeof( SPIRVHeader ) ) {
		Fatal( "%s doesn't have a header", path );
	}

	Seek( f, offsetof( SPIRVHeader, generator ) );

	size_t w = fwrite( &hash, 1, sizeof( hash ), f );
	if( w != sizeof( hash ) ) {
		FatalErrno( "Can't write hash" );
	}
}

static Span< const char > ShaderFilename( Allocator * a, Span< const char > src_filename, Span< Span< const char > > features ) {
	DynamicString filename( a, "{}", StripExtension( src_filename ) );
	for( Span< const char > feature : features ) {
		filename.append( "_{}", feature );
	}
	filename += ShaderExtension;
	return CloneSpan( a, filename.span() );
}

Span< const char > ShaderFilename( Allocator * a, const GraphicsShaderDescriptor & shader ) {
	return ShaderFilename( a, shader.src, shader.features );
}

Span< const char > ShaderFilename( const ComputeShaderDescriptor & shader ) {
	return StripExtension( shader.src );
}

static bool CompileGraphicsShader( TempAllocator & temp, Span< const char > file, Span< Span< const char > > features, const CompileShadersSettings * settings ) {
	Span< const char > src_path = temp.sv( "base/glsl/{}", file );
	Span< const char > out_path = ShaderFilename( &temp, file, features );

	DynamicString features_defines( &temp );
	for( Span< const char > feature : features ) {
		features_defines.append( " -D {}", feature );
	}

	constexpr Span< const char > target = IFDEF( PLATFORM_MACOS ) ? "vulkan1.2"_sp : "vulkan1.3"_sp;

	DynamicArray< const char * > commands( &temp );
	DynamicArray< const char * > files_to_remove( &temp );

	commands.add( temp( "dxc{} -spirv -T vs_6_0 -fspv-target-env={} -fvk-use-scalar-layout -fspv-entrypoint-name=main -E VertexMain {} -Fo {}.vert.spv {}", ExeExtension, target, features_defines, out_path, src_path ) );
	commands.add( temp( "dxc{} -spirv -T ps_6_0 -fspv-target-env={} -fvk-use-scalar-layout -fspv-entrypoint-name=main -E FragmentMain {} -Fo {}.frag.spv {}", ExeExtension, target, features_defines, out_path, src_path ) );

	if( settings->optimize ) {
		commands.add( temp( "spirv-opt{} -O {}.vert.spv -o {}.vert.spv", ExeExtension, out_path, out_path ) );
		commands.add( temp( "spirv-opt{} -O {}.frag.spv -o {}.frag.spv", ExeExtension, out_path, out_path ) );
	}

	if( IFDEF( PLATFORM_MACOS ) ) {
		commands.add( temp( "spirv-cross --msl --msl-version 20000 --msl-argument-buffers --msl-argument-buffer-tier 1 --msl-force-active-argument-buffer-resources --rename-entry-point main vertex_main vert --output {}.vert.metal {}.vert.spv", out_path, out_path ) );
		commands.add( temp( "spirv-cross --msl --msl-version 20000 --msl-argument-buffers --msl-argument-buffer-tier 1 --msl-force-active-argument-buffer-resources --rename-entry-point main fragment_main frag --output {}.frag.metal {}.frag.spv", out_path, out_path ) );

		commands.add( temp( "xcrun -sdk macosx metal -c {}.vert.metal -o {}.vert.air", out_path, out_path ) );
		commands.add( temp( "xcrun -sdk macosx metal -c {}.frag.metal -o {}.frag.air", out_path, out_path ) );
		commands.add( temp( "xcrun -sdk macosx metallib {}.vert.air {}.frag.air -o {}.metallib", out_path, out_path, out_path ) );

		files_to_remove.add( temp( "{}.vert.spv", out_path ) );
		files_to_remove.add( temp( "{}.frag.spv", out_path ) );
		// files_to_remove.add( temp( "{}.vert.metal", out_path ) );
		// files_to_remove.add( temp( "{}.frag.metal", out_path ) );
		files_to_remove.add( temp( "{}.vert.air", out_path ) );
		files_to_remove.add( temp( "{}.frag.air", out_path ) );
	}

	defer {
		for( const char * file : files_to_remove ) {
			RemoveFile( &temp, file );
		}
	};

	for( const char * command : commands ) {
		// printf( "%s\n", command );
		if( system( command ) != 0 ) {
			printf( "\e[1;33m%s failed\e[0m\n", command );
			return false;
		}
	}

	return true;
}

static bool CompileComputeShader( TempAllocator & temp, Span< const char > file, const CompileShadersSettings * settings ) {
	const char * src_path = temp( "base/glsl/{}", file );
	Span< const char > out_path = StripExtension( src_path );

	DynamicArray< const char * > commands( &temp );
	DynamicArray< const char * > files_to_remove( &temp );

	constexpr const char * target = IFDEF( PLATFORM_MACOS ) ? "vulkan1.3" : "opengl4.5";

	commands.add( temp( "dxc{} -spirv -T cs_6_0 -fspv-target-env={} -fvk-use-scalar-layout -E main -Fo {}.spv {}", ExeExtension, target, out_path, src_path ) );

	if( settings->optimize ) {
		commands.add( temp( "spirv-opt{} -O {}.spv -o {}.spv", ExeExtension, out_path, out_path ) );
	}

	if( IFDEF( PLATFORM_MACOS ) ) {
		commands.add( temp( "spirv-cross --msl --output {}.metal {}.spv", out_path, out_path ) );
		commands.add( temp( "xcrun -sdk macosx metal -c {}.metal -o {}.air", out_path, out_path ) );
		commands.add( temp( "xcrun -sdk macosx metallib {}.air -o {}.metallib", out_path, out_path ) );

		files_to_remove.add( temp( "{}.spv", out_path ) );
		files_to_remove.add( temp( "{}.metal", out_path ) );
		files_to_remove.add( temp( "{}.air", out_path ) );
	}

	defer {
		for( const char * file : files_to_remove ) {
			RemoveFile( &temp, file );
		}
	};

	for( const char * command : commands ) {
		// printf( "%s\n", command );
		if( system( command ) != 0 ) {
			printf( "\e[1;33m%s failed\e[0m\n", command );
			return false;
		}
	}

	return true;
}

static void CompileShader( TempAllocator * temp, void * data ) {
	CompileShaderJob * job = ( CompileShaderJob * ) data;

	if( job->graphics ) {
		job->ok = CompileGraphicsShader( *temp, job->src, job->features, job->settings );
	}
	else {
		job->ok = CompileComputeShader( *temp, job->src, job->settings );
	}
}

static bool CompileShadersImpl( const ShaderDescriptors & shaders, ArenaAllocator * a, const CompileShadersSettings & settings ) {
	DynamicArray< CompileShaderJob > jobs( a );

	for( const GraphicsShaderDescriptor & shader : shaders.graphics_shaders ) {
		jobs.add( CompileShaderJob {
			.settings = &settings,
			.graphics = true,
			.src = shader.src,
			.features = shader.features,
		} );
	}

	for( const ComputeShaderDescriptor & shader : shaders.compute_shaders ) {
		jobs.add( CompileShaderJob {
			.settings = &settings,
			.graphics = false,
			.src = shader.src,
		} );
	}

	ParallelFor( jobs.span(), CompileShader );

	for( const CompileShaderJob & job : jobs ) {
		if( !job.ok ) {
			ggprint( "failed {}\n", job.src );
			return false;
		}
	}

	return true;
}

bool CompileShaders( ArenaAllocator * a, const CompileShadersSettings & settings ) {
	return VisitShaderDescriptors< bool >( CompileShadersImpl, a, settings );
}
