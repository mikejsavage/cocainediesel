#include "qcommon/base.h"
#include "client/renderer/renderer.h"
#include "client/renderer/backend.h"

constexpr u32 MAP_MAGIC_BYTES = U32( 133769420 );

struct Lump {
	u32 offset;
	u32 n;
};

struct MapHeader {
	u32 magic;

	Lump entities;
	Lump keyvalues;
	Lump chars;

	Lump models;
	Lump meshes;
	Lump vertices;
	Lump indices;

	Lump nodes;
	Lump leaves;
	Lump brushes;
	Lump faces;
};

struct MapEntity {
	u32 first_keyvalue;
	u32 num_keyvalues;
};

struct MapKeyValue {
	u16 key_offset;
	u16 key_length;
	u16 value_offset;
	u16 value_length;
};

struct MapMesh {
	u64 material_hash;
	// add base vertex and use u16 indices?
	u32 first_index;
	u32 num_indices;
};

struct MapModel {
	u16 first_mesh;
	u16 num_meshes;
};

struct MapVertex {
	Vec3 position;
	Vec2 uv;
};

struct MapPlane {
	Vec3 normal;
	float distance;
};

struct MapNode {
	float distance;
	u32 back_child : 29;
	u32 axis : 2;
	u32 is_leaf : 1;
};

struct MapLeaf {
	u32 first_brush;
	u32 num_brushes : 29;
	u32 unused : 2;
	u32 is_leaf : 1;
};

struct MapNodeOrLeaf {
	u64 data;
};

struct MapFace {
	u64 material_hash;
	MapPlane plane;
};

struct MapBrush {
	u32 contents;
	u16 first_face;
	u16 num_face;
};

STATIC_ASSERT( sizeof( MapMesh ) == 2 * sizeof( u64 ) );
STATIC_ASSERT( sizeof( MapModel ) == 2 * sizeof( u16 ) );
STATIC_ASSERT( sizeof( MapNode ) == sizeof( MapNodeOrLeaf ) );
STATIC_ASSERT( sizeof( MapLeaf ) == sizeof( MapNodeOrLeaf ) );
STATIC_ASSERT( sizeof( MapFace ) == 3 * sizeof( u64 ) );
STATIC_ASSERT( sizeof( MapBrush ) == 2 * sizeof( u32 ) );

struct MapSpans {
	Span< const MapEntity > entities;
	Span< const MapKeyValue > keyvalues;
	Span< const char > chars;

	Span< const MapModel > models;
	Span< const MapMesh > meshes;
	Span< const MapVertex > vertices;
	Span< const u32 > indices;

	Span< const MapNodeOrLeaf > nodes;
	Span< const MapBrush > brushes;
	Span< const MapFace > faces;
};

template< typename T >
bool ParseLump( Span< T > * span, Span< const u8 > data, Lump lump ) {
	size_t one_past_end = lump.offset + lump.n * sizeof( T );
	if( one_past_end > data.n )
		return false;

	*span = data.slice( lump.offset, one_past_end ).cast< T >();
	return true;
}

void GetMapKeyValue( const MapSpans & spans, size_t i, Span< const char > * key, Span< const char > * value ) {
	const MapKeyValue & kv = spans.keyvalues[ i ];
	*key = spans.chars.slice( kv.key_offset, kv.key_offset + kv.key_length );
	*value = spans.chars.slice( kv.value_offset, kv.value_offset + kv.value_length );
}

bool ParseMap( MapSpans * spans, Span< const u8 > data ) {
	if( data.n < sizeof( MapHeader ) )
		return false;

	const MapHeader * header = ( const MapHeader * ) data.ptr;
	if( header->magic != MAP_MAGIC_BYTES )
		return false;

	bool ok = true;

	ok = ok && ParseLump( &spans->entities, data, header->entities );
	ok = ok && ParseLump( &spans->keyvalues, data, header->keyvalues );
	ok = ok && ParseLump( &spans->chars, data, header->chars );

	ok = ok && ParseLump( &spans->models, data, header->models );
	ok = ok && ParseLump( &spans->meshes, data, header->meshes );
	ok = ok && ParseLump( &spans->vertices, data, header->vertices );
	ok = ok && ParseLump( &spans->indices, data, header->indices );

	ok = ok && ParseLump( &spans->nodes, data, header->nodes );
	ok = ok && ParseLump( &spans->brushes, data, header->brushes );
	ok = ok && ParseLump( &spans->faces, data, header->faces );

	return ok;
}

struct Material;

struct MapRenderData {
	struct DrawCall {
		StringHash material;
		u32 first_index;
		u32 num_indices;
	};

	struct Model {
		Span< DrawCall > draw_calls;
	};

	Mesh mesh;
	Span< Model > models;
	float fog_strength;
};

struct MapGPUCollisionData {
	TextureBuffer nodeBuffer;
	TextureBuffer leafBuffer;
	TextureBuffer brushBuffer;
	TextureBuffer planeBuffer;
};

MapRenderData NewMapRenderData( const MapSpans & spans ) {
	MapRenderData data;

	MeshConfig mesh_config;
	mesh_config.unified_buffer = NewVertexBuffer( spans.vertices );
	mesh_config.indices = NewIndexBuffer( spans.indices );

	data.mesh = NewMesh( mesh_config );

	data.models = ALLOC_SPAN( sys_allocator, MapRenderData::Model, spans.models.n );
	for( size_t i = 0; i < spans.models.n; i++ ) {
		Span< const MapMesh > meshes = spans.meshes.slice( spans.models[ i ].first_mesh, spans.models[ i ].num_meshes );
		data.models[ i ].draw_calls = ALLOC_SPAN( sys_allocator, MapRenderData::DrawCall, spans.models[ i ].num_meshes );

		for( size_t j = 0; j < meshes.n; j++ ) {
			MapRenderData::DrawCall * dc = &data.models[ i ].draw_calls[ j ];
			dc->material = StringHash( meshes[ j ].material_hash );
			dc->first_index = meshes[ j ].first_index;
			dc->num_indices = meshes[ j ].num_indices;
		}
	}

	data.fog_strength = 12345; // TODO

	return data;
}

void DeleteMapRenderData( MapRenderData data ) {
	DeleteMesh( data.mesh );

	for( MapRenderData::Model model : data.models ) {
		FREE( sys_allocator, model.draw_calls.ptr );
	}

	FREE( sys_allocator, data.models.ptr );
}

