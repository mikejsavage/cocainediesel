#pragma once

#include <vector>

#include "qcommon/types.h"
#include "qcommon/array.h"

struct ParsedKeyValue {
	Span< const char > key;
	Span< const char > value;
};

struct ParsedBrushFace {
	Vec3 plane[ 3 ];
	Span< const char > material;
	u64 material_hash;
	Mat3 texcoords_transform;
};

struct ParsedBrush {
	size_t entity_id;
	size_t id;
	const char * first_char;
	size_t line_number;

	BoundedDynamicArray< ParsedBrushFace, 32 > faces;
};

struct ParsedControlPoint {
	Vec3 position;
	Vec2 uv;
};

struct ParsedPatch {
	size_t entity_id;
	size_t id;
	const char * first_char;
	size_t line_number;

	Span< const char > material;
	u64 material_hash;
	u32 w, h;
	ParsedControlPoint control_points[ 1024 ];
};

struct ParsedEntity {
	BoundedDynamicArray< ParsedKeyValue, 16 > kvs;
	std::vector< ParsedBrush > brushes;
	std::vector< ParsedPatch > patches;
	u32 model_id;
};

std::vector< ParsedEntity > ParseEntities( Span< char > map );
