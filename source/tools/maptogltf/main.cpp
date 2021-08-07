#include <algorithm>

#include "parsing.h"

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"

void Sys_ShowErrorMessage( const char * msg ) {
	printf( "%s\n", msg );
}

constexpr const char * whitespace_chars = " \r\n\t";

template< typename T, size_t N >
struct StaticArray {
	T elems[ N ];
	size_t n;

	bool full() const {
		return n == N;
	}

	void add( const T & x ) {
		assert( !full() );
		elems[ n ] = x;
		n++;
	}

	Span< T > span() {
		return Span< T >( elems, n );
	}
};

template< typename T, size_t N, typename F >
Span< const char > ParseUpToN( StaticArray< T, N > * array, Span< const char > str, F parser ) {
	return ParseUpToN( array->elems, &array->n, N, str, parser );
}

static Span< const char > ParseNOrMoreDigits( size_t n, Span< const char > str ) {
	return PEGNOrMore( str, n, []( Span< const char > str ) {
		return PEGRange( str, '0', '9' );
	} );
}

static Span< const char > SkipWhitespace( Span< const char > str ) {
	return PEGNOrMore( str, 0, []( Span< const char > str ) {
		return PEGSet( str, whitespace_chars );
	} );
}

static Span< const char > ParseWord( Span< const char > * capture, Span< const char > str ) {
	str = SkipWhitespace( str );
	return PEGCapture( capture, str, []( Span< const char > str ) {
		return PEGNOrMore( str, 1, []( Span< const char > str ) {
			return PEGNotSet( str, whitespace_chars );
		} );
	} );
}

static Span< const char > PEGCapture( int * capture, Span< const char > str ) {
	Span< const char > capture_str;
	Span< const char > res = PEGCapture( &capture_str, str, []( Span< const char > str ) {
		return ParseNOrMoreDigits( 1, str );
	} );

	if( res.ptr == NULL )
		return NullSpan;

	if( !TrySpanToInt( capture_str, capture ) )
		return NullSpan;

	return res;
}

static Span< const char > PEGCapture( float * capture, Span< const char > str ) {
	Span< const char > capture_str;
	Span< const char > res = PEGCapture( &capture_str, str, []( Span< const char > str ) {
		str = PEGOptional( str, []( Span< const char > str ) { return PEGLiteral( str, "-" ); } );
		str = ParseNOrMoreDigits( 1, str );
		str = PEGOptional( str, []( Span< const char > str ) {
			str = PEGLiteral( str, "." );
			str = ParseNOrMoreDigits( 0, str );
			return str;
		} );
		return str;
	} );

	if( res.ptr == NULL )
		return NullSpan;

	if( !TrySpanToFloat( capture_str, capture ) )
		return NullSpan;

	return res;
}

static Span< const char > SkipToken( Span< const char > str, const char * token ) {
	str = SkipWhitespace( str );
	str = PEGLiteral( str, token );
	return str;
}

static Span< const char > ParseFloat( float * x, Span< const char > str ) {
	str = SkipWhitespace( str );
	str = PEGCapture( x, str );
	return str;
}

static Span< const char > ParseVec3( Vec3 * v, Span< const char > str ) {
	str = SkipToken( str, "(" );
	for( size_t i = 0; i < 3; i++ ) {
		str = ParseFloat( &v->ptr()[ i ], str );
	}
	str = SkipToken( str, ")" );
	return str;
}

struct Face {
	Vec3 plane[ 3 ];
	Span< const char > material;
	Mat3 texcoords_transform;
};

struct KeyValue {
	Span< const char > key;
	Span< const char > value;
};

constexpr size_t MAX_BRUSH_FACES = 32;

struct Brush {
	StaticArray< Face, MAX_BRUSH_FACES > faces;
};

struct Patch {
};

struct Entity {
	StaticArray< KeyValue, 16 > kvs;
	NonRAIIDynamicArray< Brush > brushes;
	NonRAIIDynamicArray< Patch > patches;
};

static Span< const char > ParsePlane( Vec3 * points, Span< const char > str ) {
	for( size_t i = 0; i < 3; i++ ) {
		str = ParseVec3( &points[ i ], str );
	}
	return str;
}

static Span< const char > SkipFlags( Span< const char > str ) {
	str = SkipToken( str, "0" );
	str = SkipToken( str, "0" );
	str = SkipToken( str, "0" );
	return str;
}

static Span< const char > ParseQ1Face( Face * face, Span< const char > str ) {
	str = ParsePlane( face->plane, str );
	str = ParseWord( &face->material, str );

	float u, v, angle, scale_x, scale_y;
	str = ParseFloat( &u, str );
	str = ParseFloat( &v, str );
	str = ParseFloat( &angle, str );
	str = ParseFloat( &scale_x, str );
	str = ParseFloat( &scale_y, str );

	// TODO: convert to transform

	str = SkipFlags( str );

	return str;
}

static Span< const char > ParseQ1Brush( Brush * brush, Span< const char > str ) {
	str = SkipToken( str, "{" );
	str = ParseUpToN( &brush->faces, str, ParseQ1Face );
	str = SkipToken( str, "}" );
	return brush->faces.n >= 4 ? str : NullSpan;
}

static Span< const char > ParseQ3Face( Face * face, Span< const char > str ) {
	str = ParsePlane( face->plane, str );

	Vec3 uv_row1 = { };
	Vec3 uv_row2 = { };
	str = SkipToken( str, "(" );
	str = ParseVec3( &uv_row1, str );
	str = ParseVec3( &uv_row2, str );
	str = SkipToken( str, ")" );

	face->texcoords_transform = Mat3(
		uv_row1.x, uv_row1.y, uv_row1.z,
		uv_row2.x, uv_row2.y, uv_row2.z,
		0.0f, 0.0f, 1.0f
	);

	str = ParseWord( &face->material, str );
	str = SkipFlags( str );

	return str;
}

static Span< const char > ParseQ3Brush( Brush * brush, Span< const char > str ) {
	str = SkipToken( str, "{" );
	str = SkipToken( str, "brushDef" );
	str = SkipToken( str, "{" );
	str = ParseUpToN( &brush->faces, str, ParseQ3Face );
	str = SkipToken( str, "}" );
	str = SkipToken( str, "}" );
	return brush->faces.n >= 4 ? str : NullSpan;
}

static Span< const char > ParsePatchDef( Span< const char > str ) {
	str = SkipToken( str, "{" );
	str = SkipToken( str, "patchDef2" );
	str = SkipToken( str, "{" );

	Span< const char > material;
	str = ParseWord( &material, str );

	str = SkipToken( str, "(" );

	int w = 0;
	int h = 0;
	str = SkipWhitespace( str );
	str = PEGCapture( &w, str );
	str = SkipWhitespace( str );
	str = PEGCapture( &h, str );
	str = SkipFlags( str );

	str = SkipToken( str, ")" );

	str = SkipToken( str, "(" );

	for( int x = 0; x < w; x++ ) {
		str = SkipToken( str, "(" );
		for( int y = 0; y < h; y++ ) {
			str = SkipToken( str, "(" );

			float dont_care;
			for( int i = 0; i < 5; i++ ) {
				str = SkipWhitespace( str );
				str = PEGCapture( &dont_care, str );
			}

			str = SkipToken( str, ")" );
		}
		str = SkipToken( str, ")" );
	}

	str = SkipToken( str, ")" );

	str = SkipToken( str, "}" );
	str = SkipToken( str, "}" );

	return str;
}

static Span< const char > ParseQuoted( Span< const char > * quoted, Span< const char > str ) {
	str = SkipToken( str, "\"" );
	str = PEGCapture( quoted, str, []( Span< const char > str ) {
		return PEGNOrMore( str, 0, []( Span< const char > str ) {
			return PEGOr( str,
				[]( Span< const char > str ) { return PEGNotSet( str, "\\\"" ); },
				[]( Span< const char > str ) { return PEGLiteral( str, "\\\"" ); },
				[]( Span< const char > str ) { return PEGLiteral( str, "\\\\" ); }
			);
		} );
	} );
	str = PEGLiteral( str, "\"" );
	return str;
}

static Span< const char > ParseKeyValue( KeyValue * kv, Span< const char > str ) {
	str = ParseQuoted( &kv->key, str );
	str = ParseQuoted( &kv->value, str );
	return str;
}

static Span< const char > ParseEntity( Entity * entity, Span< const char > str ) {
	ZoneScoped;

	str = SkipToken( str, "{" );

	str = ParseUpToN( &entity->kvs, str, ParseKeyValue );

	entity->brushes.init( sys_allocator );
	entity->patches.init( sys_allocator );

	while( true ) {
		Brush brush;
		Span< const char > res = PEGOr( str,
			[&]( Span< const char > str ) { return ParseQ1Brush( &brush, str ); },
			[&]( Span< const char > str ) { return ParseQ3Brush( &brush, str ); },
			[]( Span< const char > str ) { return ParsePatchDef( str ); }
		);

		if( res.ptr == NULL )
			break;

		str = res;
		entity->brushes.add( brush );
	}

	str = SkipToken( str, "}" );

	if( str.ptr == NULL ) {
		entity->brushes.shutdown();
		entity->patches.shutdown();
	}

	return str;
}

struct Plane {
	Vec3 normal;
	float distance;
};

struct FaceVert {
	Vec3 pos;
	Vec2 uv;
};

constexpr size_t MAX_FACE_VERTS = 32;

using FaceVerts = StaticArray< FaceVert, MAX_FACE_VERTS >;

static bool PlaneFrom3Points( Plane * plane, Vec3 a, Vec3 b, Vec3 c ) {
	Vec3 ab = b - a;
	Vec3 ac = c - a;

	Vec3 normal = SafeNormalize( Cross( ac, ab ) );
	if( normal == Vec3( 0.0f ) )
		return false;

	plane->normal = normal;
	plane->distance = Dot( a, normal );

	return true;
}

static bool Intersect3PlanesPoint( Vec3 * p, Plane plane1, Plane plane2, Plane plane3 ) {
	constexpr float epsilon = 0.001f;

	Vec3 n2xn3 = Cross( plane2.normal, plane3.normal );
	float n1_n2xn3 = Dot( plane1.normal, n2xn3 );

	if( fabsf( n1_n2xn3 ) < epsilon )
		return false;

	Vec3 n3xn1 = Cross( plane3.normal, plane1.normal );
	Vec3 n1xn2 = Cross( plane1.normal, plane2.normal );

	*p = ( plane1.distance * n2xn3 + plane2.distance * n3xn1 + plane3.distance * n1xn2 ) / n1_n2xn3;
	return true;
}

static bool PointInsideBrush( Span< const Plane > planes, Vec3 p ) {
	constexpr float epsilon = 0.001f;
	for( Plane plane : planes ) {
		if( Dot( p, plane.normal ) - plane.distance > epsilon ) {
			return false;
		}
	}

	return true;
}

static void format( FormatBuffer * fb, const Plane & plane, const FormatOpts & opts ) {
	ggformat_impl( fb, "{.5}.X = {.1}", plane.normal, plane.distance );
}

static bool FaceToVerts( FaceVerts * verts, const Brush & brush, const Plane * planes, size_t face ) {
	for( size_t i = 0; i < brush.faces.n; i++ ) {
		if( i == face )
			continue;

		for( size_t j = i + 1; j < brush.faces.n; j++ ) {
			if( j == face )
				continue;

			Vec3 p;
			if( !Intersect3PlanesPoint( &p, planes[ face ], planes[ i ], planes[ j ] ) )
				continue;

			// ggprint( "{} x {} x {} = {}\n", planes[ face ], planes[ i ], planes[ j ], p );

			if( !PointInsideBrush( Span< const Plane >( planes, brush.faces.n ), p ) )
				continue;

			verts->add( { p, Vec2() } );
		}
	}

	return true;
}

static Vec2 ProjectFaceVert( Vec3 centroid, Vec3 tangent, Vec3 bitangent, Vec3 p ) {
	Vec3 d = p - centroid;
	return Vec2( Dot( d, tangent ), Dot( d, bitangent ) );
}

struct ObjWriter {
	DynamicString * str;
	size_t brush_id;
	size_t vertex_id;
	size_t normal_id;
};

ObjWriter NewObjectWriter( DynamicString * str ) {
	return { str, 0, 1, 1 };
}

void AddBrush( ObjWriter * writer ) {
	writer->str->append( "# Brush {}\n", writer->brush_id );
	writer->brush_id++;
}

void AddVertex( ObjWriter * writer, Vec3 pos ) {
	pos *= 0.01f;
	writer->str->append( "v {} {} {} # vertex ID {}\n", pos.x, -pos.z, pos.y, writer->vertex_id );
	writer->vertex_id++;
}

void AddFace( ObjWriter * writer, Vec3 normal, size_t num_verts ) {
	// if( num_verts < 3 ) {
	// 	ggprint( "rej\n" );
	// 	return;
	// }

	writer->str->append( "vn {} {} {} # normal ID {}\n", normal.x, -normal.z, normal.y, writer->normal_id );
	size_t base_vert = writer->vertex_id - num_verts;
	for( size_t i = 0; i < num_verts - 2; i++ ) {
		writer->str->append( "f {}//{} {}//{} {}/{}\n",
			base_vert, writer->normal_id,
			base_vert + i + 2, writer->normal_id,
			base_vert + i + 1, writer->normal_id
		);
	}
	writer->normal_id++;
}

static bool BrushToVerts( ObjWriter * writer, const Brush & brush ) {
	AddBrush( writer );

	Plane planes[ MAX_BRUSH_FACES ];

	for( size_t i = 0; i < brush.faces.n; i++ ) {
		const Face & face = brush.faces.elems[ i ];
		if( !PlaneFrom3Points( &planes[ i ], face.plane[ 0 ], face.plane[ 1 ], face.plane[ 2 ] ) ) {
			return false;
		}
	}

	for( size_t i = 0; i < brush.faces.n; i++ ) {
		// generate arbitrary list of points
		FaceVerts verts = { };
		if( !FaceToVerts( &verts, brush, planes, i ) ) {
			return false;
		}

		// find centroid and project verts into 2d
		Vec3 centroid = Vec3( 0.0f );
		for( FaceVert v : verts.span() ) {
			centroid += v.pos;
		}

		centroid /= verts.n;

		Vec3 tangent, bitangent;
		OrthonormalBasis( planes[ i ].normal, &tangent, &bitangent );

		struct ProjectedVert {
			Vec2 pos;
			float theta;
		};

		ProjectedVert projected[ MAX_FACE_VERTS ];
		for( size_t j = 0; j < verts.n; j++ ) {
			projected[ j ].pos = ProjectFaceVert( centroid, tangent, bitangent, verts.elems[ j ].pos );
			projected[ j ].theta = atan2f( projected[ j ].pos.y, projected[ j ].pos.x );
		}

		std::sort( projected, projected + verts.n, []( ProjectedVert a, ProjectedVert b ) {
			return a.theta < b.theta;
		} );

		for( size_t j = 0; j < verts.n; j++ ) {
			Vec3 unprojected = centroid + projected[ j ].pos.x * tangent + projected[ j ].pos.y * bitangent;
			AddVertex( writer, unprojected );
		}

		AddFace( writer, planes[ i ].normal, verts.n );
	}

	return true;
}

Span< const char > ParseComment( Span< const char > * comment, Span< const char > str ) {
	return PEGCapture( comment, str, []( Span< const char > str ) {
		str = PEGLiteral( str, "//" );
		str = PEGNOrMore( str, 0, []( Span< const char > str ) {
			return PEGNotSet( str, "\n" );
		} );
		return str;
	} );
}

int main() {
	ZoneScoped;

	constexpr size_t arena_size = 1024 * 1024;
	ArenaAllocator arena( ALLOC_SIZE( sys_allocator, arena_size, 16 ), arena_size );
	TempAllocator temp = arena.temp();

	size_t carfentanil_len;
	char * carfentanil = ReadFileString( sys_allocator, "source/tools/maptogltf/carfentanil.map", &carfentanil_len );
	assert( carfentanil != NULL );
	defer { FREE( sys_allocator, carfentanil ); };

	Span< const char > map( carfentanil, carfentanil_len );

	for( size_t i = 0; i < map.n; i++ ) {
		Span< const char > comment;
		Span< const char > res = ParseComment( &comment, map + i );
		if( res.ptr == NULL )
			continue;

		memset( const_cast< char * >( comment.ptr ), ' ', comment.n );
		i += comment.n;
	}

	DynamicArray< Entity > entities( sys_allocator );
	defer {
		for( Entity & entity : entities ) {
			entity.brushes.shutdown();
			entity.patches.shutdown();
		}
	};

	map = CaptureNOrMore( &entities, map, 0, ParseEntity );
	map = SkipWhitespace( map );

	bool ok = map.ptr != NULL && map.n == 0;
	if( !ok ) {
		ggprint( "failed\n" );
		return 1;
	}

	DynamicString obj( sys_allocator );
	ObjWriter writer = NewObjectWriter( &obj );

	for( const Entity & entity : entities ) {
		for( const Brush & brush : entity.brushes.span() ) {
			bool asd = BrushToVerts( &writer, brush );
			ggprint( "{}\n", asd );
		}
	}

	WriteFile( &temp, "source/tools/maptogltf/test.obj", obj.c_str(), obj.length() );

	FREE( sys_allocator, arena.get_memory() );

	// if( ok ) {
	// 	for( Entity & entity : entities ) {
	// 		ggprint( "entity\n" );
	// 		for( size_t i = 0; i < entity.num_kvs; i++ ) {
	// 			ggprint( "  {} = {}\n", entity.kvs[ i ].key, entity.kvs[ i ].value );
	// 		}
        //
	// 		for( const Brush & brush : entity.brushes ) {
	// 			ggprint( "  brush\n" );
	// 			for( size_t i = 0; i < brush.num_faces; i++ ) {
	// 				const Face & face = brush.faces[ i ];
	// 				ggprint( "    face {.1} {.1} {.1} {}\n", face.plane[ 0 ], face.plane[ 1 ], face.plane[ 2 ], face.material );
	// 			}
	// 		}
	// 	}
	// }

	return 0;
}
