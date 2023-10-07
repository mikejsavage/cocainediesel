#include "qcommon/base.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/assets.h"
#include "cgame/cg_particles.h"
#include "cgame/cg_dynamics.h"

#include "cgltf/cgltf.h"

#define JSMN_HEADER
#include "jsmn/jsmn.h"

static u8 GetNodeIdx( const cgltf_node * node ) {
	return u8( uintptr_t( node->light ) - 1 );
}

static void SetNodeIdx( cgltf_node * node, u8 idx ) {
	node->light = ( cgltf_light * ) uintptr_t( idx + 1 );
}

static Span< const u8 > AccessorToSpan( const cgltf_accessor * accessor ) {
	cgltf_size offset = accessor->offset + accessor->buffer_view->offset;
	return Span< const u8 >( ( const u8 * ) accessor->buffer_view->buffer->data + offset, accessor->count * accessor->stride );
}

static VertexFormat VertexFormatFromGLTF( cgltf_type dim, cgltf_component_type component, bool normalized ) {
	// TODO: support signed types
	if( dim == cgltf_type_vec2 ) {
		if( component == cgltf_component_type_r_8u )
			return normalized ? VertexFormat_U8x2_Norm : VertexFormat_U8x2;
		if( component == cgltf_component_type_r_16u )
			return normalized ? VertexFormat_U16x2_Norm : VertexFormat_U16x2;
		if( component == cgltf_component_type_r_32f )
			return VertexFormat_Floatx2;
	}

	if( dim == cgltf_type_vec3 ) {
		if( component == cgltf_component_type_r_8u )
			return normalized ? VertexFormat_U8x3_Norm : VertexFormat_U8x3;
		if( component == cgltf_component_type_r_16u )
			return normalized ? VertexFormat_U16x3_Norm : VertexFormat_U16x3;
		if( component == cgltf_component_type_r_32f )
			return VertexFormat_Floatx3;
	}

	if( dim == cgltf_type_vec4 ) {
		if( component == cgltf_component_type_r_8u )
			return normalized ? VertexFormat_U8x4_Norm : VertexFormat_U8x4;
		if( component == cgltf_component_type_r_16u )
			return normalized ? VertexFormat_U16x4_Norm : VertexFormat_U16x4;
		if( component == cgltf_component_type_r_32f )
			return VertexFormat_Floatx4;
	}

	Assert( false );
	return VertexFormat_Floatx4;
}

static void LoadGeometry( GLTFRenderData * render_data, u8 node_idx, const cgltf_node * node, const Mat4 & transform ) {
	TempAllocator temp = cls.frame_arena.temp();

	const cgltf_primitive & prim = node->mesh->primitives[ 0 ];
	if( prim.material && StartsWith( prim.material->name, "editor/" ) )
		return;

	StaticMeshConfig mesh_config = { };
	mesh_config.name = temp( "{} nodes[{}]", render_data->name, node->name );

	for( size_t i = 0; i < prim.attributes_count; i++ ) {
		const cgltf_attribute & attr = prim.attributes[ i ];
		VertexFormat format = VertexFormatFromGLTF( attr.data->type, attr.data->component_type, attr.data->normalized );

		if( attr.type == cgltf_attribute_type_position ) {
			mesh_config.num_vertices = attr.data->count;
			mesh_config.set_attribute( VertexAttribute_Position, AccessorToSpan( attr.data ), format );

			Vec3 min, max;
			for( int j = 0; j < 3; j++ ) {
				min[ j ] = attr.data->min[ j ];
				max[ j ] = attr.data->max[ j ];
			}

			render_data->bounds = Union( render_data->bounds, ( transform * Vec4( min, 1.0f ) ).xyz() );
			render_data->bounds = Union( render_data->bounds, ( transform * Vec4( max, 1.0f ) ).xyz() );

			for( size_t j = 0; j < 3; j++ ) {
				if( min[ j ] == max[ j ] ) {
					min[ j ] -= 0.5f;
					max[ j ] += 0.5f;
				}
			}
			mesh_config.bounds = MinMax3( min, max );
		}

		if( attr.type == cgltf_attribute_type_normal ) {
			mesh_config.set_attribute( VertexAttribute_Normal, AccessorToSpan( attr.data ), format );
		}

		if( attr.type == cgltf_attribute_type_texcoord ) {
			if( mesh_config.vertex_data[ VertexAttribute_TexCoord ].n != 0 ) {
				Com_Printf( S_COLOR_YELLOW "%s has multiple sets of uvs\n", render_data->name );
			}
			else {
				mesh_config.set_attribute( VertexAttribute_TexCoord, AccessorToSpan( attr.data ), format );
			}
		}

		if( attr.type == cgltf_attribute_type_color ) {
			mesh_config.set_attribute( VertexAttribute_Color, AccessorToSpan( attr.data ), format );
		}

		if( attr.type == cgltf_attribute_type_joints ) {
			mesh_config.set_attribute( VertexAttribute_JointIndices, AccessorToSpan( attr.data ), format );
		}

		if( attr.type == cgltf_attribute_type_weights ) {
			mesh_config.set_attribute( VertexAttribute_JointWeights, AccessorToSpan( attr.data ), format );
		}
	}

	mesh_config.index_data = AccessorToSpan( prim.indices );
	mesh_config.index_format = prim.indices->component_type == cgltf_component_type_r_16u ? IndexFormat_U16 : IndexFormat_U32;
	mesh_config.num_indices = prim.indices->count;

	render_data->nodes[ node_idx ].material = prim.material == NULL ? EMPTY_HASH : StringHash( prim.material->name );
	render_data->nodes[ node_idx ].static_mesh = NewStaticMesh( mesh_config );
}

static constexpr u32 MAX_EXTRAS = 16;
struct GLTFExtras {
	Span< const char > keys[ MAX_EXTRAS ];
	Span< const char > values[ MAX_EXTRAS ];
	u32 num_extras = 0;
};

static GLTFExtras LoadExtras( cgltf_data * gltf, cgltf_node * gltf_node ) {
	char json_data[ 1024 ];
	size_t json_size = 1024;
	cgltf_copy_extras_json( gltf, &gltf_node->extras, json_data, &json_size );
	Span< const char > json( json_data, json_size );

	GLTFExtras extras = { };

	jsmn_parser p;
	constexpr u32 max_tokens = MAX_EXTRAS * 2 + 1;
	jsmntok_t tokens[ max_tokens ];
	jsmn_init( &p );
	int res = jsmn_parse( &p, json.ptr, json.n, tokens, max_tokens );
	if( res < 0 ) {
		return extras;
	}
	if( res < 1 || tokens[ 0 ].type != JSMN_OBJECT ) {
		return extras;
	}

	for( s32 i = 0; i < tokens[ 0 ].size; i++ ) {
		u32 idx = 1 + i * 2;
		extras.keys[ extras.num_extras ] = json.slice( tokens[ idx ].start, tokens[ idx ].end );
		// TODO: value could be object or array...
		extras.values[ extras.num_extras ] = json.slice( tokens[ idx + 1 ].start, tokens[ idx + 1 ].end );
		extras.num_extras++;
	}

	return extras;
}

static Span< const char > GetExtrasKey( const char * key, GLTFExtras * extras ) {
	for( u32 i = 0; i < extras->num_extras; i++ ) {
		if( StrEqual( extras->keys[ i ], key ) ) {
			return extras->values[ i ];
		}
	}
	return Span< const char >();
}

static void LoadNode( GLTFRenderData * model, cgltf_data * gltf, cgltf_node * gltf_node, u8 * node_idx ) {
	u8 idx = *node_idx;
	*node_idx += 1;
	SetNodeIdx( gltf_node, idx );

	GLTFRenderData::Node * node = &model->nodes[ idx ];
	node->parent = gltf_node->parent != NULL ? GetNodeIdx( gltf_node->parent ) : U8_MAX;
	node->name = gltf_node->name == NULL ? EMPTY_HASH : StringHash( gltf_node->name );

	cgltf_node_transform_world( gltf_node, node->global_transform.ptr() );

	node->local_transform.rotation = Quaternion::Identity();
	node->local_transform.translation = Vec3( 0.0f );
	node->local_transform.scale = 1.0f;

	if( gltf_node->has_rotation ) {
		node->local_transform.rotation = Quaternion(
			gltf_node->rotation[ 0 ],
			gltf_node->rotation[ 1 ],
			gltf_node->rotation[ 2 ],
			gltf_node->rotation[ 3 ]
		);
	}

	if( gltf_node->has_translation ) {
		node->local_transform.translation = Vec3(
			gltf_node->translation[ 0 ],
			gltf_node->translation[ 1 ],
			gltf_node->translation[ 2 ]
		);
	}

	if( gltf_node->has_scale ) {
		// TODO
		// Assert( Abs( gltf_node->scale[ 0 ] / gltf_node->scale[ 1 ] - 1.0f ) < 0.001f );
		// Assert( Abs( gltf_node->scale[ 0 ] / gltf_node->scale[ 2 ] - 1.0f ) < 0.001f );
		node->local_transform.scale = gltf_node->scale[ 0 ];
	}

	{
		GLTFExtras extras = LoadExtras( gltf, gltf_node );
		Span< const char > type = GetExtrasKey( "type", &extras );
		Span< const char > color_value = GetExtrasKey( "color", &extras );
		Vec4 color;
		for( u32 i = 0; i < 4; i++ ) {
			color[ i ] = ParseFloat( &color_value, 1.0f, Parse_StopOnNewLine );
		}
		if( type == "vfx" ) {
			node->vfx_type = ModelVfxType_Vfx;
			node->vfx_node.name = StringHash( GetExtrasKey( "name", &extras ) );
			node->vfx_node.color = color;
		}
		else if( type == "dlight" ) {
			node->vfx_type = ModelVfxType_DynamicLight;
			node->dlight_node.color = color.xyz();
			Span< const char > intensity = GetExtrasKey( "intensity", &extras );
			node->dlight_node.intensity = ParseFloat( &intensity, 0.0f, Parse_DontStopOnNewLine );
		}
		else if( type == "decal" ) {
			node->vfx_type = ModelVfxType_Decal;
			node->decal_node.color = color;
			Span< const char > angle = GetExtrasKey( "angle", &extras );
			node->decal_node.angle = ParseFloat( &angle, 0.0f, Parse_DontStopOnNewLine );
			Span< const char > radius = GetExtrasKey( "radius", &extras );
			node->decal_node.radius = ParseFloat( &radius, 0.0f, Parse_DontStopOnNewLine );
			node->decal_node.name = StringHash( GetExtrasKey( "name", &extras ) );
		}
	}

	node->skinned = gltf_node->skin != NULL;

	if( gltf_node->mesh != NULL ) {
		LoadGeometry( model, idx, gltf_node, node->global_transform );
	}

	for( size_t i = 0; i < gltf_node->children_count; i++ ) {
		LoadNode( model, gltf, gltf_node->children[ i ], node_idx );
	}

	if( gltf_node->children_count == 0 ) {
		node->first_child = U8_MAX;
	}
	else {
		node->first_child = GetNodeIdx( gltf_node->children[ 0 ] );

		for( size_t i = 0; i < gltf_node->children_count - 1; i++ ) {
			model->nodes[ GetNodeIdx( gltf_node->children[ i ] ) ].sibling = GetNodeIdx( gltf_node->children[ i + 1 ] );
		}

		model->nodes[ GetNodeIdx( gltf_node->children[ gltf_node->children_count - 1 ] ) ].sibling = U8_MAX;
	}
}

static GLTFInterpolationMode InterpolationModeFromGLTF( cgltf_interpolation_type interpolation ) {
	// TODO: cubic
	return interpolation == cgltf_interpolation_type_step ? GLTFInterpolationMode_Step : GLTFInterpolationMode_Linear;
}

template< typename T >
static float LoadChannel( const cgltf_animation_channel * chan, GLTFRenderData::AnimationChannel< T > * out_channel ) {
	constexpr size_t lanes = sizeof( T ) / sizeof( float );
	size_t n = chan->sampler->input->count;

	out_channel->samples = AllocSpan< GLTFRenderData::AnimationSample< T > >( sys_allocator, n );
	out_channel->interpolation = InterpolationModeFromGLTF( chan->sampler->interpolation );

	for( size_t i = 0; i < n; i++ ) {
		cgltf_bool ok = cgltf_accessor_read_float( chan->sampler->input, i, &out_channel->samples[ i ].time, 1 );
		ok = ok && cgltf_accessor_read_float( chan->sampler->output, i, out_channel->samples[ i ].value.ptr(), lanes );
		Assert( ok != 0 );
	}

	float duration = chan->sampler->input->max[ 0 ] - chan->sampler->input->min[ 0 ];
	return duration;
}

static float LoadScaleChannel( const cgltf_animation_channel * chan, GLTFRenderData::AnimationChannel< float > * out_channel ) {
	size_t n = chan->sampler->input->count;

	out_channel->samples = AllocSpan< GLTFRenderData::AnimationSample< float > >( sys_allocator, n );
	out_channel->interpolation = InterpolationModeFromGLTF( chan->sampler->interpolation );

	for( size_t i = 0; i < n; i++ ) {
		cgltf_accessor_read_float( chan->sampler->input, i, &out_channel->samples[ i ].time, 1 );

		float scale[ 3 ];
		cgltf_accessor_read_float( chan->sampler->output, i, scale, 3 );

		Assert( Abs( scale[ 0 ] - scale[ 1 ] ) < 0.001f );
		Assert( Abs( scale[ 0 ] - scale[ 2 ] ) < 0.001f );

		out_channel->samples[ i ].value = scale[ 0 ];
	}

	float duration = chan->sampler->input->max[ 0 ] - chan->sampler->input->min[ 0 ];
	return duration;
}

template< typename T >
static void CreateSingleSampleChannel( GLTFRenderData::AnimationChannel< T > * out_channel, T value ) {
	out_channel->samples = AllocSpan< T >( sys_allocator, 1 );
	out_channel->samples[ 0 ] = value;
}

static void LoadAnimation( GLTFRenderData * model, const cgltf_animation * animation, u8 index = 0 ) {
	float duration = 0.0f;
	for( size_t i = 0; i < animation->channels_count; i++ ) {
		const cgltf_animation_channel * chan = &animation->channels[ i ];

		u8 node_idx = GetNodeIdx( chan->target_node );
		Assert( node_idx != U8_MAX );

		float channel_duration = 0.0f;
		if( chan->target_path == cgltf_animation_path_type_translation ) {
			channel_duration = LoadChannel( chan, &model->nodes[ node_idx ].animations[ index ].translations );
		}
		else if( chan->target_path == cgltf_animation_path_type_rotation ) {
			channel_duration = LoadChannel( chan, &model->nodes[ node_idx ].animations[ index ].rotations );
		}
		else if( chan->target_path == cgltf_animation_path_type_scale ) {
			channel_duration = LoadScaleChannel( chan, &model->nodes[ node_idx ].animations[ index ].scales );
		}
		duration = Max2( channel_duration, duration );
	}
	model->animations[ index ].name = StringHash( animation->name );
	model->animations[ index ].duration = duration;
}

static void LoadSkin( GLTFRenderData * model, const cgltf_skin * skin ) {
	model->skin = AllocSpan< GLTFRenderData::Joint >( sys_allocator, skin->joints_count );

	for( size_t i = 0; i < skin->joints_count; i++ ) {
		GLTFRenderData::Joint * joint = &model->skin[ i ];
		joint->node_idx = GetNodeIdx( skin->joints[ i ] );

		cgltf_bool ok = cgltf_accessor_read_float( skin->inverse_bind_matrices, i, joint->joint_to_bind.ptr(), 16 );
		Assert( ok != 0 );
	}
}

bool NewGLTFRenderData( GLTFRenderData * render_data, cgltf_data * gltf, const char * path ) {
	TracyZoneScoped;

	// TODO: check nodes fit into u8 etc

	*render_data = { };
	render_data->name = CopyString( sys_allocator, path );
	render_data->bounds = MinMax3::Empty();

	bool ok = false;
	defer {
		if( !ok ) {
			DeleteGLTFRenderData( render_data );
		}
	};

	constexpr Mat4 y_up_to_z_up(
		1, 0, 0, 0,
		0, 0, -1, 0,
		0, 1, 0, 0,
		0, 0, 0, 1
	);
	render_data->transform = y_up_to_z_up;

	render_data->nodes = AllocSpan< GLTFRenderData::Node >( sys_allocator, gltf->nodes_count );
	memset( render_data->nodes.ptr, 0, render_data->nodes.num_bytes() );

	u8 node_idx = 0;
	for( size_t i = 0; i < gltf->scene->nodes_count; i++ ) {
		LoadNode( render_data, gltf, gltf->scene->nodes[ i ], &node_idx );
		render_data->nodes[ GetNodeIdx( gltf->scene->nodes[ i ] ) ].sibling = U8_MAX;
	}

	if( gltf->animations_count > 0 ) {
		if( gltf->skins_count > 0 ) {
			LoadSkin( render_data, &gltf->skins[ 0 ] );
		}

		render_data->animations = AllocSpan< GLTFRenderData::Animation >( sys_allocator, gltf->animations_count );

		for( size_t i = 0; i < render_data->nodes.n; i++ ) {
			render_data->nodes[ i ].animations = AllocSpan< GLTFRenderData::NodeAnimation >( sys_allocator, gltf->animations_count );
			memset( render_data->nodes[ i ].animations.ptr, 0, render_data->nodes[ i ].animations.num_bytes() );
		}

		for( size_t i = 0; i < render_data->animations.n; i++ ) {
			LoadAnimation( render_data, &gltf->animations[ i ], i );
		}
	}

	for( size_t i = 0; i < gltf->nodes_count; i++ ) {
		if( gltf->nodes[ i ].camera != NULL ) {
			render_data->camera_node = GetNodeIdx( &gltf->nodes[ i ] );
		}
	}

	ok = true;

	return true;
}

void DeleteGLTFRenderData( GLTFRenderData * render_data ) {
	for( GLTFRenderData::Node node : render_data->nodes ) {
		for( GLTFRenderData::NodeAnimation animation : node.animations ) {
			Free( sys_allocator, animation.rotations.samples.ptr );
			Free( sys_allocator, animation.translations.samples.ptr );
			Free( sys_allocator, animation.scales.samples.ptr );
		}
		DeleteMesh( node.mesh );
		Free( sys_allocator, node.animations.ptr );
	}

	Free( sys_allocator, render_data->name );
	Free( sys_allocator, render_data->nodes.ptr );
	Free( sys_allocator, render_data->skin.ptr );
	Free( sys_allocator, render_data->animations.ptr );
}

template< typename T, typename F >
static T SampleAnimationChannel( const GLTFRenderData::AnimationChannel< T > & channel, float t, T def, F lerp ) {
	if( channel.samples.ptr == NULL )
		return def;
	if( channel.samples.n == 1 )
		return channel.samples[ 0 ].value;

	t = Clamp( channel.samples[ 0 ].time, t, channel.samples[ channel.samples.n - 1 ].time );

	u32 sample = 0;
	for( u32 i = 1; i < channel.samples.n; i++ ) {
		if( channel.samples[ i ].time >= t ) {
			sample = i - 1;
			break;
		}
	}

	// TODO: cubic
	if( channel.interpolation == GLTFInterpolationMode_Step ) {
		return channel.samples[ sample ].value;
	}

	float lerp_frac = Unlerp( channel.samples[ sample ].time, t, channel.samples[ sample + 1 ].time );
	return lerp( channel.samples[ sample ].value, lerp_frac, channel.samples[ sample + 1 ].value );
}

// can't use overloaded function as a template parameter
static Vec3 LerpVec3( Vec3 a, float t, Vec3 b ) { return Lerp( a, t, b ); }
static float LerpFloat( float a, float t, float b ) { return Lerp( a, t, b ); }

Span< TRS > SampleAnimation( Allocator * a, const GLTFRenderData * render_data, float t, u8 animation ) {
	TracyZoneScoped;

	Span< TRS > local_poses = AllocSpan< TRS >( a, render_data->nodes.n );

	for( u8 i = 0; i < render_data->nodes.n; i++ ) {
		const GLTFRenderData::Node * node = &render_data->nodes[ i ];
		local_poses[ i ].rotation = SampleAnimationChannel( node->animations[ animation ].rotations, t, node->local_transform.rotation, NLerp );
		local_poses[ i ].translation = SampleAnimationChannel( node->animations[ animation ].translations, t, node->local_transform.translation, LerpVec3 );
		local_poses[ i ].scale = SampleAnimationChannel( node->animations[ animation ].scales, t, node->local_transform.scale, LerpFloat );
	}

	return local_poses;
}

static Mat4 TRSToMat4( const TRS & trs ) {
	Quaternion q = trs.rotation;
	Vec3 t = trs.translation;
	float s = trs.scale;

	// return t * q * s;
	return Mat4(
		( 1.0f - 2 * q.y * q.y - 2.0f * q.z * q.z ) * s,
		( 2.0f * q.x * q.y - 2.0f * q.z * q.w ) * s,
		( 2.0f * q.x * q.z + 2.0f * q.y * q.w ) * s,
		t.x,

		( 2.0f * q.x * q.y + 2.0f * q.z * q.w ) * s,
		( 1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z ) * s,
		( 2.0f * q.y * q.z - 2.0f * q.x * q.w ) * s,
		t.y,

		( 2.0f * q.x * q.z - 2.0f * q.y * q.w ) * s,
		( 2.0f * q.y * q.z + 2.0f * q.x * q.w ) * s,
		( 1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y ) * s,
		t.z,

		0.0f, 0.0f, 0.0f, 1.0f
	);
}

MatrixPalettes ComputeMatrixPalettes( Allocator * a, const GLTFRenderData * render_data, Span< const TRS > local_poses ) {
	TracyZoneScoped;

	Assert( local_poses.n == render_data->nodes.n );

	MatrixPalettes palettes = { };
	palettes.node_transforms = AllocSpan< Mat4 >( a, render_data->nodes.n );
	if( render_data->skin.n != 0 ) {
		palettes.skinning_matrices = AllocSpan< Mat4 >( a, render_data->skin.n );
	}

	for( u8 i = 0; i < render_data->nodes.n; i++ ) {
		u8 parent = render_data->nodes[ i ].parent;
		if( parent == U8_MAX ) {
			palettes.node_transforms[ i ] = TRSToMat4( local_poses[ i ] );
		}
		else {
			palettes.node_transforms[ i ] = palettes.node_transforms[ parent ] * TRSToMat4( local_poses[ i ] );
		}
	}

	for( u8 i = 0; i < render_data->skin.n; i++ ) {
		u8 node_idx = render_data->skin[ i ].node_idx;
		palettes.skinning_matrices[ i ] = palettes.node_transforms[ node_idx ] * render_data->skin[ i ].joint_to_bind;
	}

	return palettes;
}

bool FindNodeByName( const GLTFRenderData * render_data, StringHash name, u8 * idx ) {
	for( u8 i = 0; i < render_data->nodes.n; i++ ) {
		if( render_data->nodes[ i ].name == name ) {
			*idx = i;
			return true;
		}
	}

	return false;
}

bool FindAnimationByName( const GLTFRenderData * render_data, StringHash name, u8 * idx ) {
	for( u8 i = 0; i < render_data->animations.n; i++ ) {
		if( render_data->animations[ i ].name == name ) {
			*idx = i;
			return true;
		}
	}

	return false;
}

static void MergePosesRecursive( Span< TRS > lower, Span< const TRS > upper, const GLTFRenderData * render_data, u8 i ) {
	lower[ i ] = upper[ i ];

	const GLTFRenderData::Node * node = &render_data->nodes[ i ];
	if( node->sibling != U8_MAX )
		MergePosesRecursive( lower, upper, render_data, node->sibling );
	if( node->first_child != U8_MAX )
		MergePosesRecursive( lower, upper, render_data, node->first_child );
}

void MergeLowerUpperPoses( Span< TRS > lower, Span< const TRS > upper, const GLTFRenderData * render_data, u8 upper_root_node ) {
	lower[ upper_root_node ] = upper[ upper_root_node ];

	const GLTFRenderData::Node * node = &render_data->nodes[ upper_root_node ];
	if( node->first_child != U8_MAX )
		MergePosesRecursive( lower, upper, render_data, node->first_child );
}

static void DrawVfxNode( DrawModelConfig::DrawModel config, const GLTFRenderData::Node * node, Mat4 & transform, const Vec4 & color ) {
	TracyZoneScoped;
	if( !config.enabled || node->vfx_type == ModelVfxType_None )
		return;

	// TODO: idk about this cheers
	Vec3 scale = Vec3( Length( transform.col0.xyz() ), Length( transform.col1.xyz() ), Length( transform.col2.xyz() ) );
	float size = Min2( Min2( Abs( scale.x ), Abs( scale.y ) ), Abs( scale.z ) );

	if( size <= 0.01f )
		return;

	Vec3 origin = transform.col3.xyz();
	Vec3 normal = SafeNormalize( transform.col1.xyz() );
	switch( node->vfx_type ) {
		case ModelVfxType_Vfx:
			DoVisualEffect( node->vfx_node.name, origin, normal, size, node->vfx_node.color * color );
			break;
		case ModelVfxType_DynamicLight:
			DrawDynamicLight( origin, node->dlight_node.color, node->dlight_node.intensity * size );
			break;
		case ModelVfxType_Decal:
			DrawDecal( origin, normal, node->decal_node.radius * size, node->decal_node.angle, node->decal_node.name, node->decal_node.color );
			break;
	}
}

static void DrawModelNode( DrawModelConfig::DrawModel config, const StaticMesh mesh, Mat4 & transform, GPUMaterial gpu_material, GPUSkinningInstance skinning_instance, bool skinned ) {
	TracyZoneScoped;
	if( !config.enabled )
		return;

	GPUModelInstance instance = { };
	instance.transform = Mat3x4( transform );
	instance.material = gpu_material;

	if( skinned ) {
		DrawStaticMesh( mesh, instance, skinning_instance );
	}
	else {
		DrawStaticMesh( mesh, instance );
	}
}

static void DrawShadowsNode( DrawModelConfig::DrawShadows config, const StaticMesh mesh, Mat4 & transform, GPUMaterial gpu_material, GPUSkinningInstance skinning_instance, bool skinned ) {
	TracyZoneScoped;
	if( !config.enabled )
		return;

	GPUModelShadowsInstance instance = { };
	instance.transform = Mat3x4( transform );

	if( skinned ) {
		DrawStaticMesh( mesh, instance, skinning_instance );
	}
	else {
		DrawStaticMesh( mesh, instance );
	}
}

static void DrawOutlinesNode( DrawModelConfig::DrawOutlines config, const StaticMesh mesh, Mat4 & transform, GPUMaterial gpu_material, GPUSkinningInstance skinning_instance, bool skinned ) {
	TracyZoneScoped;
	if( !config.enabled )
		return;

	GPUModelOutlinesInstance instance = { };
	instance.color = config.outline_color;
	instance.height = config.outline_height;
	instance.transform = Mat3x4( transform );

	if( skinned ) {
		DrawStaticMesh( mesh, instance, skinning_instance );
	}
	else {
		DrawStaticMesh( mesh, instance );
	}
}

static void DrawSilhouetteNode( DrawModelConfig::DrawSilhouette config, const StaticMesh mesh, Mat4 & transform, GPUMaterial gpu_material, GPUSkinningInstance skinning_instance, bool skinned ) {
	TracyZoneScoped;
	if( !config.enabled )
		return;

	GPUModelSilhouetteInstance instance = { };
	instance.color = config.silhouette_color;
	instance.transform = Mat3x4( transform );

	if( skinned ) {
		DrawStaticMesh( mesh, instance, skinning_instance );
	}
	else {
		DrawStaticMesh( mesh, instance );
	}
}

void DrawGLTFModel( const DrawModelConfig & config, const GLTFRenderData * render_data, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes ) {
	TracyZoneScoped;
	if( render_data == NULL )
		return;

	bool animated = palettes.node_transforms.ptr != NULL;
	bool any_skinned = render_data->skin.n > 0;

	GPUSkinningInstance skinning_instance = { };
	if( any_skinned ) {
		skinning_instance = UploadSkinningMatrices( palettes );
	}

	for( u8 i = 0; i < render_data->nodes.n; i++ ) {
		TracyZoneScopedN( "Render node" );

		const GLTFRenderData::Node * node = &render_data->nodes[ i ];
		if( node->static_mesh.num_indices == 0 && node->vfx_type == ModelVfxType_None )
			continue;

		bool skinned = animated && node->skinned;

		Mat4 node_transform;
		if( skinned ) {
			node_transform = Mat4::Identity();
		}
		else if( animated ) {
			node_transform = palettes.node_transforms[ i ];
		}
		else {
			node_transform = node->global_transform;
		}
		node_transform = transform * render_data->transform * node_transform;

		DrawVfxNode( config.draw_model, node, node_transform, color );

		if( node->static_mesh.num_indices == 0 )
			continue;
		// if( node->mesh.num_vertices == 0 )
		// 	continue;

		GPUMaterial gpu_material = GetGPUMaterial( FindMaterial( node->material ), color );

		StaticMesh mesh = node->static_mesh;

		DrawModelNode( config.draw_model, mesh, node_transform, gpu_material, skinning_instance, skinned );
		DrawShadowsNode( config.draw_shadows, mesh, node_transform, gpu_material, skinning_instance, skinned );
		DrawOutlinesNode( config.draw_outlines, mesh, node_transform, gpu_material, skinning_instance, skinned );
		DrawSilhouetteNode( config.draw_silhouette, mesh, node_transform, gpu_material, skinning_instance, skinned );
	}
}
