#include "qcommon/base.h"
#include "qcommon/hash.h"
#include "qcommon/hashmap.h"
#include "client/assets.h"
#include "client/renderer/api.h"
#include "client/renderer/model.h"
#include "client/renderer/gltf.h"
#include "gameshared/q_shared.h"
#include "gameshared/collision.h"

#include "cgltf/cgltf.h"

static constexpr u32 MAX_MODELS = 1024;

static HashMap< GLTFRenderData, MAX_MODELS > gltf_models;

static bool LoadGLTFRenderData( GLTFRenderData * render_data, Span< const char > path ) {
	TracyZoneScoped;
	TracyZoneSpan( path );

	Span< const u8 > data = AssetBinary( path );

	cgltf_options options = { };
	options.type = cgltf_file_type_glb;

	cgltf_data * gltf;
	if( cgltf_parse( &options, data.ptr, data.num_bytes(), &gltf ) != cgltf_result_success ) {
		Com_GGPrint( S_COLOR_YELLOW "{} isn't a GLTF file", path );
		return false;
	}

	defer { cgltf_free( gltf ); };

	if( !LoadGLBBuffers( gltf ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Couldn't load buffers in {}", path );
		return false;
	}

	if( cgltf_validate( gltf ) != cgltf_result_success ) {
		Com_GGPrint( S_COLOR_YELLOW "{} is invalid GLTF", path );
		return false;
	}

	if( gltf->scenes_count != 1 || gltf->skins_count > 1 || gltf->cameras_count > 1 ) {
		Com_GGPrint( S_COLOR_YELLOW "Trivial models only please ({})", path );
		return false;
	}

	if( gltf->lights_count != 0 ) {
		Com_GGPrint( S_COLOR_YELLOW "We can't load models that have lights in them ({})", path );
		return false;
	}

	for( size_t i = 0; i < gltf->meshes_count; i++ ) {
		if( gltf->meshes[ i ].primitives_count != 1 ) {
			Com_GGPrint( S_COLOR_YELLOW "Meshes with multiple primitives are unsupported ({})", path );
			return false;
		}
	}

	return NewGLTFRenderData( render_data, gltf, path );
}

static void LoadGLTF( Span< const char > path ) {
	GLTFRenderData render_data;
	if( !LoadGLTFRenderData( &render_data, path ) )
		return;

	u64 hash = Hash64( StripExtension( path ) );

	GLTFRenderData * slot = gltf_models.get( hash );
	if( slot == NULL ) {
		if( !gltf_models.add( hash, render_data ) ) {
			Com_Printf( S_COLOR_YELLOW "Too many models!\n" );
			DeleteGLTFRenderData( &render_data );
			return;
		}
	}
	else {
		DeleteGLTFRenderData( slot );
		*slot = render_data;
	}
}

void InitGLTFModels() {
	TracyZoneScoped;

	gltf_models.clear();

	for( Span< const char > path : AssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".glb" )
			continue;
		LoadGLTF( path );
	}
}

void ShutdownGLTFModels() {
	for( GLTFRenderData & model : gltf_models.span() ) {
		DeleteGLTFRenderData( &model );
	}
}

void HotloadGLTFModels() {
	TracyZoneScoped;

	for( Span< const char > path : ModifiedAssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".glb" )
			continue;
		LoadGLTF( path );
	}
}

const GLTFRenderData * FindGLTFRenderData( StringHash name ) {
	return gltf_models.get( name.hash );
}
