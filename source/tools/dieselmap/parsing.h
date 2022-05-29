#pragma once

#include <vector>

#include "qcommon/types.h"
#include "qcommon/array.h"

template< typename T, size_t N >
struct StaticArray {
	T elems[ N ];
	size_t n;

	bool full() const {
		return n == N;
	}

	void add( const T & x ) {
		if( full() ) {
			Fatal( "StaticArray::add" );
		}
		elems[ n ] = x;
		n++;
	}

	Span< T > span() {
		return Span< T >( elems, n );
	}

	Span< const T > span() const {
		return Span< const T >( elems, n );
	}
};

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
	int id;
	int line_number;

	StaticArray< ParsedBrushFace, 32 > faces;
};

struct ParsedControlPoint {
	Vec3 position;
	Vec2 uv;
};

struct ParsedPatch {
	int id;
	int line_number;

	Span< const char > material;
	u64 material_hash;
	int w, h;
	ParsedControlPoint control_points[ 1024 ];
};

struct ParsedEntity {
	int id;

	StaticArray< ParsedKeyValue, 16 > kvs;
	NonRAIIDynamicArray< ParsedBrush > brushes;
	NonRAIIDynamicArray< ParsedPatch > patches;
	u32 model_id;
};

std::vector< ParsedEntity > ParseEntities( Span< char > map );
