#include "r_local.h"
#include "cgltf/cgltf.h"

struct GLTFMesh {
	// redundant but qf needs these
	drawSurfaceType_t type;
	int num_verts;
	int num_elems;

	mesh_vbo_t * vbo;
	shader_t * shader;
};

struct GLTFModel {
	GLTFMesh meshes[ 16 ];
	int num_meshes;
};

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

static void TouchGLTFModel( model_t * mod ) {
	mod->registrationSequence = rsh.registrationSequence;

	GLTFModel * gltf = ( GLTFModel * ) mod->extradata;
	for( int i = 0; i < gltf->num_meshes; i++ ) {
		R_TouchMeshVBO( gltf->meshes[ i ].vbo );
		R_TouchShader( gltf->meshes[ i ].shader );
	}
}

void Mod_LoadGLTFModel( model_t * mod, void * buffer, int buffer_size, const bspFormatDesc_t * bsp_format ) {
	cgltf_options options = { };
	options.type = cgltf_file_type_glb;

	cgltf_data * data;
	if( cgltf_parse( &options, buffer, buffer_size, &data ) != cgltf_result_success ) {
		ri.Com_Error( ERR_DROP, "%s isn't a GLTF file", mod->name );
	}

	if( !LoadBinaryBuffers( data ) ) {
		cgltf_free( data );
		ri.Com_Error( ERR_DROP, "Couldn't load buffers in %s", mod->name );
	}

	if( cgltf_validate( data ) != cgltf_result_success ) {
		cgltf_free( data );
		ri.Com_Error( ERR_DROP, "%s is invalid GLTF", mod->name );
	}

	if( data->meshes_count != 1 || data->nodes_count != 1 || data->scenes_count != 1 || data->animations_count > 0 ) {
		cgltf_free( data );
		ri.Com_Error( ERR_DROP, "Trivial models only please" );
	}

	GLTFModel * gltf = ( GLTFModel * ) Mod_Malloc( mod, sizeof( GLTFModel ) );

	mod->type = ModelType_GLTF;
	mod->extradata = gltf;
	mod->registrationSequence = rsh.registrationSequence;
	mod->touch = &TouchGLTFModel;
	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	const cgltf_node * node = data->scenes[ 0 ].nodes[ 0 ];

	const mat4_t y_up_to_z_up = {
		1, 0, 0, 0,
		0, 0, 1, 0,
		0, 1, 0, 0,
		0, 0, 0, 1,
	};

	mat4_t transform;
	{
		mat4_t lolqfusion;
		cgltf_node_transform_local( node, lolqfusion );
		Matrix4_Multiply( y_up_to_z_up, lolqfusion, transform );
	}

	gltf->num_meshes = node->mesh->primitives_count;

	for( size_t i = 0; i < node->mesh->primitives_count; i++ ) {
		const cgltf_primitive & prim = node->mesh->primitives[ i ];

		if( prim.indices->component_type != cgltf_component_type_r_16u ) {
			cgltf_free( data );
			ri.Com_Error( ERR_DROP, "16bit indices please" );
		}

		vec4_t * positions = NULL;
		vec4_t * normals = NULL;
		vec2_t * uvs = NULL;

		vattribmask_t attributes = 0;

		for( size_t j = 0; j < prim.attributes_count; j++ ) {
			const cgltf_attribute & attr = prim.attributes[ j ];

			if( attr.type == cgltf_attribute_type_position ) {
				attributes |= VATTRIB_POSITION_BIT;
				positions = ( vec4_t * ) Mod_Malloc( mod, prim.attributes[ j ].data->count * sizeof( vec4_t ) );
				for( size_t k = 0; k < prim.attributes[ j ].data->count; k++ ) {
					vec4_t lolqfusion; // TODO: better vector library
					cgltf_accessor_read_float( attr.data, k, lolqfusion, 3 );
					lolqfusion[ 3 ] = 1.0f;

					Matrix4_Multiply_Vector( transform, lolqfusion, positions[ k ] );
					AddPointToBounds( positions[ k ], mod->mins, mod->maxs );
				}
			}

			if( attr.type == cgltf_attribute_type_normal ) {
				attributes |= VATTRIB_NORMAL_BIT;
				normals = ( vec4_t * ) Mod_Malloc( mod, prim.attributes[ j ].data->count * sizeof( vec4_t ) );
				for( size_t k = 0; k < prim.attributes[ j ].data->count; k++ ) {
					cgltf_accessor_read_float( attr.data, k, normals[ k ], 3 );
					normals[ k ][ 3 ] = 0.0f;
				}
			}

			if( attr.type == cgltf_attribute_type_texcoord ) {
				attributes |= VATTRIB_TEXCOORDS_BIT;
				uvs = ( vec2_t * ) Mod_Malloc( mod, prim.attributes[ j ].data->count * sizeof( vec2_t ) );
				for( size_t k = 0; k < prim.attributes[ j ].data->count; k++ ) {
					cgltf_accessor_read_float( attr.data, k, uvs[ k ], 2 );
				}
			}
		}

		mesh_t mesh = { };
		mesh.numVerts = prim.attributes[ 0 ].data->count;
		mesh.numElems = prim.indices->count;
		mesh.xyzArray = positions;
		mesh.normalsArray = normals;
		mesh.stArray = uvs;

		cgltf_size offset = prim.indices->offset + prim.indices->buffer_view->offset;
		mesh.elems = ( elem_t * ) ( ( const uint8_t * ) prim.indices->buffer_view->buffer->data + offset );

		gltf->meshes[ i ].vbo = R_CreateMeshVBO( NULL, mesh.numVerts, mesh.numElems, 0, attributes, VBO_TAG_NONE, 0 );
		R_UploadVBOVertexData( gltf->meshes[ i ].vbo, 0, attributes, &mesh, 0 );
		R_UploadVBOElemData( gltf->meshes[ i ].vbo, 0, 0, &mesh );

		Mod_MemFree( positions );
		Mod_MemFree( normals );
		Mod_MemFree( uvs );

		const char * material_name = prim.material != NULL ? prim.material->name : "***r_notexture***";
		gltf->meshes[ i ].shader = R_RegisterShader( material_name, SHADER_TYPE_DIFFUSE );

		gltf->meshes[ i ].type = ST_GLTF;
		gltf->meshes[ i ].num_verts = mesh.numVerts;
		gltf->meshes[ i ].num_elems = mesh.numElems;
	}

	mod->radius = RadiusFromBounds( mod->mins, mod->maxs );
}

void R_AddGLTFModelToDrawList( const entity_t * e ) {
	const GLTFModel * gltf = ( const GLTFModel * ) e->model->extradata;
	for( int i = 0; i < gltf->num_meshes; i++ ) {
		const GLTFMesh & mesh = gltf->meshes[ i ];
		int sort_key = R_PackShaderOrder( mesh.shader );
		R_AddSurfToDrawList( rn.meshlist, e, mesh.shader, 0, sort_key, &mesh );
	}
}

void R_DrawGLTFMesh( const entity_t * e, const shader_t * shader, const GLTFMesh * mesh ) {
	RB_BindVBO( mesh->vbo->index, GL_TRIANGLES );
	RB_DrawElements( 0, mesh->num_verts, 0, mesh->num_elems );
}

void R_CacheGLTFModelEntity( const entity_t * e ) {
	// behold qf's masterpiece renderer
	entSceneCache_t * cache = R_ENTCACHE( e );
	const model_t * mod = e->model;
	cache->rotated = true;
	cache->radius = mod->radius * e->scale;
}
