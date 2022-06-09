#include "qcommon/base.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/qcommon.h"
#include "client/assets.h"
#include "client/maps.h"
#include "client/renderer/renderer.h"
#include "client/renderer/model.h"
#include "client/renderer/gltf.h"
#include "gameshared/q_shared.h"

#include "cgltf/cgltf.h"

static constexpr u32 MAX_MODELS = 1024;

static GLTFRenderData gltf_models[ MAX_MODELS ];
static u32 num_gltf_models;
static Hashtable< MAX_MODELS * 2 > gltf_models_hashtable;

// like cgltf_load_buffers, but doesn't try to load URIs
static bool LoadBinaryBuffers( cgltf_data * data ) {
	if( data->buffers_count && data->buffers[0].data == NULL && data->buffers[0].uri == NULL && data->bin ) {
		if( data->bin_size < data->buffers[0].size )
			return false;
		data->buffers[0].data = const_cast< void * >( data->bin );
	}

	for( cgltf_size i = 0; i < data->buffers_count; i++ ) {
		if( data->buffers[i].data == NULL )
			return false;
	}

	return true;
}

static bool LoadGLTF( GLTFRenderData * render_data, const char * path ) {
	TracyZoneScoped;
	TracyZoneText( path, strlen( path ) );

	Span< const u8 > data = AssetBinary( path );

	cgltf_options options = { };
	options.type = cgltf_file_type_glb;

	cgltf_data * gltf;
	if( cgltf_parse( &options, data.ptr, data.num_bytes(), &gltf ) != cgltf_result_success ) {
		Com_Printf( S_COLOR_YELLOW "%s isn't a GLTF file\n", path );
		return false;
	}

	defer { cgltf_free( gltf ); };

	if( !LoadBinaryBuffers( gltf ) ) {
		Com_Printf( S_COLOR_YELLOW "Couldn't load buffers in %s\n", path );
		return false;
	}

	if( cgltf_validate( gltf ) != cgltf_result_success ) {
		Com_Printf( S_COLOR_YELLOW "%s is invalid GLTF\n", path );
		return false;
	}

	if( gltf->scenes_count != 1 || gltf->skins_count > 1 || gltf->cameras_count > 1 ) {
		Com_Printf( S_COLOR_YELLOW "Trivial models only please (%s)\n", path );
		return false;
	}

	if( gltf->lights_count != 0 ) {
		Com_Printf( S_COLOR_YELLOW "We can't load models that have lights in them (%s)\n", path );
		return false;
	}

	for( size_t i = 0; i < gltf->meshes_count; i++ ) {
		if( gltf->meshes[ i ].primitives_count != 1 ) {
			Com_Printf( S_COLOR_YELLOW "Meshes with multiple primitives are unsupported (%s)\n", path );
			return false;
		}
	}

	return NewGLTFRenderData( render_data, gltf, path );
}

static void LoadGLTF( const char * path ) {
	GLTFRenderData render_data;
	if( !LoadGLTF( &render_data, path ) )
		return;

	u64 hash = Hash64( StripExtension( path ) );

	u64 idx = num_gltf_models;
	if( !gltf_models_hashtable.get( hash, &idx ) ) {
		assert( num_gltf_models < ARRAY_COUNT( gltf_models ) );
		gltf_models_hashtable.add( hash, num_gltf_models );
		num_gltf_models++;
	}
	else {
		DeleteGLTFRenderData( &gltf_models[ idx ] );
	}

	gltf_models[ idx ] = render_data;
}

void InitGLTFModels() {
	TracyZoneScoped;

	num_gltf_models = 0;

	for( const char * path : AssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".glb" )
			continue;
		LoadGLTF( path );
	}

	InitGLTFInstancing();
}

void ShutdownGLTFModels() {
	for( u32 i = 0; i < num_gltf_models; i++ ) {
		DeleteGLTFRenderData( &gltf_models[ i ] );
	}

	ShutdownGLTFInstancing();
}

void HotloadGLTFModels() {
	TracyZoneScoped;

	for( const char * path : ModifiedAssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".glb" )
			continue;
		LoadGLTF( path );
	}
}
const GLTFRenderData * FindGLTFRenderData( StringHash name ) {
	u64 idx;
	if( !gltf_models_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &gltf_models[ idx ];
}
