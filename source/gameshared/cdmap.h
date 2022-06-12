#pragma once

#include "qcommon/types.h"
#include "gameshared/editor_materials.h"

enum MapSectionType {
	MapSection_Entities,
	MapSection_EntityData,
	MapSection_EntityKeyValues,

	MapSection_Models,

	MapSection_Nodes,
	MapSection_Brushes,
	MapSection_BrushIndices,
	MapSection_BrushPlanes,

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
	u64 format_version;
	MapSection sections[ MapSection_Count ];
};

constexpr const char CDMAP_MAGIC[ sizeof( MapHeader::magic ) ] = "cdmap";
constexpr u64 CDMAP_FORMAT_VERSION = 1;

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
	static constexpr u32 LEAF = 3;

	static bool is_leaf( MapKDTreeNode node ) {
		return node.leaf.is_leaf == LEAF;
	}

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
	MinMax3 bounds;
	u16 first_plane;
	u8 num_planes;
	SolidBits solidity;
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

	Span< const MapMesh > meshes;
	Span< const MapVertex > vertices;
	Span< const u32 > vertex_indices;
};

enum DecodeMapResult {
	DecodeMapResult_Ok,
	DecodeMapResult_NotAMap,
	DecodeMapResult_WrongFormatVersion,
};

DecodeMapResult DecodeMap( MapData * map, Span< const u8 > data );
