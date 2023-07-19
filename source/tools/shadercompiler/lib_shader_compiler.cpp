#include "api.h"

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/threadpool.h"
#include "gameshared/q_shared.h"

#include "client/renderer/shader_variants.h"

static const char * EXE_SUFFIX = IFDEF( PLATFORM_WINDOWS ) ? ".exe" : "";

struct CompileShaderJob {
	bool graphics;
	const char * src;
	Span< const char * > features;

	bool ok;
};

static bool CompileGraphicsShader( TempAllocator & temp, const char * file, Span< const char * > features ) {
	const char * src_path = temp( "base/glsl/{}", file );

	DynamicString features_filename( &temp );
	for( const char * feature : features ) {
		features_filename.append( "_{}", feature );
	}
	const char * out_path = temp( "{}{}", StripExtension( src_path ), features_filename );

	DynamicString features_glslc( &temp );
	for( const char * feature : features ) {
		features_filename.append( " -D{}=1", feature );
	}

	DynamicArray< const char * > commands( &temp );
	DynamicArray< const char * > files_to_remove( &temp );

	commands.add( temp( "glslc{} -std=450core --target-env=opengl4.5 -fshader-stage=vertex -DVERTEX_SHADER=1 -Dv2f=out {} -fauto-map-locations -fauto-bind-uniforms {} -o {}.vert.spv", EXE_SUFFIX, features_glslc, src_path, out_path ) );
	commands.add( temp( "glslc{} -std=450core --target-env=opengl4.5 -fshader-stage=fragment -DFRAGMENT_SHADER=1 -Dv2f=in {} -fauto-map-locations -fauto-bind-uniforms {} -o {}.frag.spv", EXE_SUFFIX, features_glslc, src_path, out_path ) );

	commands.add( temp( "spirv-opt{} -O {}.vert.spv -o {}.vert.spv", EXE_SUFFIX, out_path, out_path ) );
	commands.add( temp( "spirv-opt{} -O {}.frag.spv -o {}.frag.spv", EXE_SUFFIX, out_path, out_path ) );

	if( IFDEF( PLATFORM_MACOS ) ) {
		commands.add( temp( "spirv-cross --msl --rename-entry-point main vertex_main vert --output {}.vert.metal {}.vert.spv", out_path, out_path ) );
		commands.add( temp( "spirv-cross --msl --rename-entry-point main fragment_main frag --output {}.frag.metal {}.frag.spv", out_path, out_path ) );

		commands.add( temp( "xcrun -sdk macosx metal -c {}.vert.metal -o {}.vert.air", out_path, out_path ) );
		commands.add( temp( "xcrun -sdk macosx metal -c {}.frag.metal -o {}.frag.air", out_path, out_path ) );
		commands.add( temp( "xcrun -sdk macosx metallib {}.vert.air {}.frag.air -o {}.metallib", out_path, out_path, out_path ) );

		files_to_remove.add( temp( "{}.vert.spv", out_path ) );
		files_to_remove.add( temp( "{}.frag.spv", out_path ) );
		files_to_remove.add( temp( "{}.vert.metal", out_path ) );
		files_to_remove.add( temp( "{}.frag.metal", out_path ) );
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

static bool CompileComputeShader( TempAllocator & temp, const char * file ) {
	const char * src_path = temp( "base/glsl/{}", file );
	Span< const char > out_path = StripExtension( src_path );

	DynamicArray< const char * > commands( &temp );
	DynamicArray< const char * > files_to_remove( &temp );

	commands.add( temp( "glslc{} -std=450core --target-env=opengl4.5 -fshader-stage=compute -fauto-map-locations -fauto-bind-uniforms {} -o {}.spv", EXE_SUFFIX, src_path, out_path ) );
	commands.add( temp( "spirv-opt{} -O {}.spv -o {}.spv", EXE_SUFFIX, out_path, out_path ) );

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
		job->ok = CompileGraphicsShader( *temp, job->src, job->features );
	}
	else {
		job->ok = CompileComputeShader( *temp, job->src );
	}
}

static bool CompileShadersImpl( const ShaderDescriptors & shaders, ArenaAllocator * a ) {
	DynamicArray< CompileShaderJob > jobs( a );

	for( const GraphicsShaderDescriptor & shader : shaders.graphics_shaders ) {
		jobs.add( CompileShaderJob {
			.graphics = true,
			.src = shader.src,
			.features = shader.features,
		} );
	}

	for( const ComputeShaderDescriptor & shader : shaders.compute_shaders ) {
		jobs.add( CompileShaderJob {
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
	return VisitShaderDescriptors< bool >( CompileShadersImpl, a );
}
