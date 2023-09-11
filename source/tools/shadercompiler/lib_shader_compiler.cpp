#include "api.h"

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/threadpool.h"
#include "gameshared/q_shared.h"

#include "client/renderer/shader_shared.h"
#include "client/renderer/shader_variants.h"
#include "client/renderer/spirv.h"

static const char * EXE_SUFFIX = IFDEF( PLATFORM_WINDOWS ) ? ".exe" : "";

struct CompileShaderJob {
	const CompileShadersSettings * settings;
	bool graphics;
	const char * src;
	Span< const char * > features;

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

static bool CompileGraphicsShader( TempAllocator & temp, const char * file, Span< const char * > features, const CompileShadersSettings * settings ) {
	const char * src_path = temp( "base/glsl/{}", file );

	DynamicString features_filename( &temp );
	for( const char * feature : features ) {
		features_filename.append( "_{}", feature );
	}
	const char * out_path = temp( "{}{}", StripExtension( src_path ), features_filename );

	DynamicString features_glslc( &temp );
	for( const char * feature : features ) {
		features_glslc.append( " -D{}=1", feature );
	}

	constexpr const char * target = IFDEF( PLATFORM_MACOS ) ? "vulkan1.3" : "opengl4.5";

	DynamicArray< const char * > commands( &temp );
	DynamicArray< const char * > files_to_remove( &temp );

	commands.add( temp( "glslc{} -std=450core --target-env={} -fshader-stage=vertex -DVERTEX_SHADER=1 -Dv2f=out {} -fauto-map-locations -fauto-bind-uniforms {} -o {}.vert.spv", EXE_SUFFIX, target, features_glslc, src_path, out_path ) );
	commands.add( temp( "glslc{} -std=450core --target-env={} -fshader-stage=fragment -DFRAGMENT_SHADER=1 -Dv2f=in {} -fauto-map-locations -fauto-bind-uniforms {} -o {}.frag.spv", EXE_SUFFIX, target, features_glslc, src_path, out_path ) );

	if( settings->optimize ) {
		commands.add( temp( "spirv-opt{} -O {}.vert.spv -o {}.vert.spv", EXE_SUFFIX, out_path, out_path ) );
		commands.add( temp( "spirv-opt{} -O {}.frag.spv -o {}.frag.spv", EXE_SUFFIX, out_path, out_path ) );
	}

	if( IFDEF( PLATFORM_MACOS ) ) {
		commands.add( temp( "spirv-cross --msl --msl-version 20000 --msl-argument-buffers --msl-discrete-descriptor-set {} --msl-force-active-argument-buffer-resources --rename-entry-point main vertex_main vert --output {}.vert.metal {}.vert.spv", DescriptorSet_DrawCall, out_path, out_path ) );
		commands.add( temp( "spirv-cross --msl --msl-version 20000 --msl-argument-buffers --msl-discrete-descriptor-set {} --msl-force-active-argument-buffer-resources --rename-entry-point main fragment_main frag --output {}.frag.metal {}.frag.spv", DescriptorSet_DrawCall, out_path, out_path ) );

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
		printf( "%s\n", command );
		if( system( command ) != 0 ) {
			return false;
		}
	}

	return true;
}

static bool CompileComputeShader( TempAllocator & temp, const char * file, const CompileShadersSettings * settings ) {
	const char * src_path = temp( "base/glsl/{}", file );
	Span< const char > out_path = StripExtension( src_path );

	DynamicArray< const char * > commands( &temp );
	DynamicArray< const char * > files_to_remove( &temp );

	constexpr const char * target = IFDEF( PLATFORM_MACOS ) ? "vulkan1.3" : "opengl4.5";

	commands.add( temp( "glslc{} -std=450core --target-env={} -fshader-stage=compute -fauto-map-locations -fauto-bind-uniforms {} -o {}.spv", EXE_SUFFIX, target, src_path, out_path ) );

	if( settings->optimize ) {
		commands.add( temp( "spirv-opt{} -O {}.spv -o {}.spv", EXE_SUFFIX, out_path, out_path ) );
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
		printf( "%s\n", command );
		if( system( command ) != 0 ) {
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
			return false;
		}
	}

	return true;
}

bool CompileShaders( ArenaAllocator * a, const CompileShadersSettings & settings ) {
	return VisitShaderDescriptors< bool >( CompileShadersImpl, a, settings );
}
