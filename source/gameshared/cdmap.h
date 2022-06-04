#pragma once

#include "qcommon/types.h"

enum MapSectionType {
	MapSection_Entities,
	MapSection_EntityData,
	MapSection_EntityKeyValues,

	MapSection_Models,

	MapSection_Nodes,
	MapSection_Brushes,
	MapSection_BrushIndices,
	MapSection_BrushPlanes,
	MapSection_BrushPlaneIndices,

	MapSection_Meshes,
	MapSection_Vertices,
	MapSection_VertexIndices,

	MapSection_Count
};

struct MapSection {
	u32 offset, size;
};

struct MapHeader {
	char magic[ 8 ];
	u32 format_version;
	MapSection sections[ MapSection_Count ];
};

constexpr const char CDMAP_MAGIC[ sizeof( MapHeader::magic ) ] = "cdmap";
constexpr u32 CDMAP_FORMAT_VERSION = 1;

struct MapEntity {
	u32 first_key_value;
	u32 num_key_values;
};

struct MapEntityKeyValue {
	u32 offset;
	u32 key_size;
	u32 value_size;
};

struct MapModel {
	MinMax3 bounds;
	u32 root_node;
	u32 first_mesh;
	u32 num_meshes;
};

struct MapKDTreeNode {
	union {
		struct {
			float splitting_plane_distance;
			u32 is_leaf_and_splitting_plane_axis : 2;
			u32 front_child : 30;
		} node;

		struct {
			u32 first_brush;
			u32 is_leaf : 2;
			u32 num_brushes : 30;
		} leaf;
	};
};

STATIC_ASSERT( sizeof( MapKDTreeNode ) == 8 );

struct MapBrush {
	u16 first_plane;
	u8 num_planes;
	u8 solidity;
};

struct MapMesh {
	u64 material;
	u32 first_vertex_index;
	u32 num_vertices;
};

struct MapVertex {
	Vec3 position;
	Vec3 normal;
};

struct MapData {
	Span< const MapEntity > entities;
	Span< const char > entity_data;
	Span< const MapEntityKeyValue > entity_kvs;

	Span< const MapModel > models;

	Span< const MapKDTreeNode > nodes;
	Span< const MapBrush > brushes;
	Span< const u32 > brush_indices;
	Span< const Plane > brush_planes;
	Span< const u32 > brush_plane_indices;

	Span< const MapMesh > meshes;
	Span< const MapVertex > vertices;
	Span< const u32 > vertex_indices;
};

enum DecodeMapResult {
	DecodeMapResult_Ok,
	DecodeMapResult_NotAMap,
	DecodeMapResult_WrongFormatVersion,
};

template< typename T >
static bool DecodeMapSection( Span< T > * span, Span< const u8 > data, MapSectionType type ) {
	const MapHeader * header = ( const MapHeader * ) data.ptr;
	MapSection section = header->sections[ type ];

	if( section.offset + section.size > data.n )
		return false;

	if( section.size % sizeof( T ) != 0 )
		return false;

	*span = ( data + section.offset ).slice( 0, section.size ).cast< const T >();
	return true;
}

inline DecodeMapResult DecodeMap( MapData * map, Span< const u8 > data ) {
	if( data.n < sizeof( MapHeader ) )
		return DecodeMapResult_NotAMap;

	const MapHeader * header = ( const MapHeader * ) data.ptr;
	if( memcmp( header->magic, CDMAP_MAGIC, sizeof( header->magic ) ) != 0 )
		return DecodeMapResult_NotAMap;

	if( header->format_version != CDMAP_FORMAT_VERSION )
		return DecodeMapResult_WrongFormatVersion;

	bool ok = true;
	ok = ok && DecodeMapSection( &map->entities, data, MapSection_Entities );
	ok = ok && DecodeMapSection( &map->entity_data, data, MapSection_EntityData );
	ok = ok && DecodeMapSection( &map->entity_kvs, data, MapSection_EntityKeyValues );
	ok = ok && DecodeMapSection( &map->models, data, MapSection_Models );
	ok = ok && DecodeMapSection( &map->nodes, data, MapSection_Nodes );
	ok = ok && DecodeMapSection( &map->brushes, data, MapSection_Brushes );
	ok = ok && DecodeMapSection( &map->brush_indices, data, MapSection_BrushIndices );
	ok = ok && DecodeMapSection( &map->brush_planes, data, MapSection_BrushPlanes );
	ok = ok && DecodeMapSection( &map->meshes, data, MapSection_Meshes );
	ok = ok && DecodeMapSection( &map->vertices, data, MapSection_Vertices );
	ok = ok && DecodeMapSection( &map->vertex_indices, data, MapSection_VertexIndices );

	return ok ? DecodeMapResult_Ok : DecodeMapResult_NotAMap;
}
