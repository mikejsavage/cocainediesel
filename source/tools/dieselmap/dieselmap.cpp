#include <algorithm>
#include <math.h>

#include "parsing.h"
#include "materials.h"

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/qfiles.h"
#include "qcommon/span2d.h"
#include "qcommon/string.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"

void ShowErrorAndAbortImpl( const char * msg, const char * file, int line ) {
	printf( "%s\n", msg );
	abort();
}

enum BSPLump {
	BSPLump_Entities,
	BSPLump_Materials,
	BSPLump_Planes,
	BSPLump_Nodes,
	BSPLump_Leaves,
	BSPLump_LeafFaces_Unused,
	BSPLump_LeafBrushes,
	BSPLump_Models,
	BSPLump_Brushes,
	BSPLump_BrushSides,
	BSPLump_Vertices,
	BSPLump_Indices,
	BSPLump_Fogs_Unused,
	BSPLump_Faces,
	BSPLump_Lighting_Unused,
	BSPLump_LightGrid_Unused,
	BSPLump_Visibility,
	BSPLump_LightArray_Unused,

	BSPLump_Count
};

static const char * lump_names[] = {
	"BSPLump_Entities",
	"BSPLump_Materials",
	"BSPLump_Planes",
	"BSPLump_Nodes",
	"BSPLump_Leaves",
	"BSPLump_LeafFaces_Unused",
	"BSPLump_LeafBrushes",
	"BSPLump_Models",
	"BSPLump_Brushes",
	"BSPLump_BrushSides",
	"BSPLump_Vertices",
	"BSPLump_Indices",
	"BSPLump_Fogs_Unused",
	"BSPLump_Faces",
	"BSPLump_Lighting_Unused",
	"BSPLump_LightGrid_Unused",
	"BSPLump_Visibility",
	"BSPLump_LightArray_Unused",
};

struct BSPLumpLocation {
	u32 offset;
	u32 length;
};

struct BSPHeader {
	u32 magic;
	u32 version;
	BSPLumpLocation lumps[ BSPLump_Count ];
};

struct BSPMaterial {
	char name[ 64 ];
	u32 surface_flags;
	u32 content_flags;
};

struct BSPPlane {
	Vec3 normal;
	float dist;
};

struct BSPNode {
	int planenum;
	int children[2];
	MinMax3 bounds;
};

struct BSPLeaf {
	int cluster;
	int area;
	MinMax3 bounds;
	int firstLeafFace;
	int numLeafFaces;
	int firstLeafBrush;
	int numLeafBrushes;
};

struct BSPModel {
	MinMax3 bounds;
	u32 first_face;
	u32 num_faces;
	u32 first_brush;
	u32 num_brushes;
};

struct BSPBrush {
	u32 first_face;
	u32 num_faces;
	u32 material;
};

struct BSPBrushFace {
	u32 planenum;
	u32 material;
};

struct BSPVertex {
	Vec3 position;
	Vec2 uv;
	Vec2 lightmap_uv;
	Vec3 normal;
	RGBA8 vertex_light;
};

struct BSPTriangle {
	u32 a, b, c;
};

struct BSPFace {
	u32 material;
	s32 fog;
	s32 type;

	u32 first_vertex;
	u32 num_vertices;
	u32 first_index;
	u32 num_indices;

	s32 lightmap;
	s32 lightmap_offset[ 2 ];
	s32 lightmap_size[ 2 ];

	Vec3 origin;

	MinMax3 bounds;
	Vec3 normal;

	u32 patch_width;
	u32 patch_height;
};

struct BSP {
	DynamicString * entities;
	DynamicArray< BSPMaterial > * materials;
	DynamicArray< BSPVertex > * vertices;
	DynamicArray< BSPTriangle > * triangles;
	DynamicArray< BSPFace > * faces;
	DynamicArray< BSPModel > * models;

	DynamicArray< Plane > * planes;

	DynamicArray< BSPNode > * nodes;
	DynamicArray< BSPLeaf > * leaves;

	DynamicArray< BSPBrush > * brushes;
	DynamicArray< u32 > * brush_ids;

	DynamicArray< BSPBrushFace > * brush_faces;
};

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

template< typename T, size_t N, typename F >
Span< const char > ParseUpToN( StaticArray< T, N > * array, Span< const char > str, F parser ) {
	return ParseUpToN( array->elems, &array->n, N, str, parser );
}

static Span< const char > ParseNOrMoreDigits( size_t n, Span< const char > str ) {
	return PEGNOrMore( str, n, []( Span< const char > str ) {
		return PEGRange( str, '0', '9' );
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

struct ControlPoint {
	Vec3 position;
	Vec2 uv;
};

struct Patch {
	Span< const char > material;
	int w, h;
	ControlPoint control_points[ 1024 ];
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

	str = PEGOptional( str, SkipFlags );

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

static Span< const char > ParsePatch( Patch * patch, Span< const char > str ) {
	str = SkipToken( str, "{" );
	str = SkipToken( str, "patchDef2" );
	str = SkipToken( str, "{" );

	str = ParseWord( &patch->material, str );

	str = SkipToken( str, "(" );

	str = SkipWhitespace( str );
	str = PEGCapture( &patch->w, str );
	str = SkipWhitespace( str );
	str = PEGCapture( &patch->h, str );
	str = SkipFlags( str );

	str = SkipToken( str, ")" );

	str = SkipToken( str, "(" );

	Span2D< ControlPoint > control_points( patch->control_points, patch->w, patch->h );

	for( int x = 0; x < patch->w; x++ ) {
		str = SkipToken( str, "(" );
		for( int y = 0; y < patch->h; y++ ) {
			str = SkipToken( str, "(" );

			ControlPoint cp;

			for( int i = 0; i < 3; i++ ) {
				str = ParseFloat( &cp.position[ i ], str );
			}

			for( int i = 0; i < 2; i++ ) {
				str = ParseFloat( &cp.uv[ i ], str );
			}

			control_points( x, y ) = cp;

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
		Patch patch;
		bool is_patch = false;

		Span< const char > res = PEGOr( str,
			[&]( Span< const char > str ) { return ParseQ1Brush( &brush, str ); },
			[&]( Span< const char > str ) { return ParseQ3Brush( &brush, str ); },
			[&]( Span< const char > str ) {
				is_patch = true;
				return ParsePatch( &patch, str );
			}
		);

		if( res.ptr == NULL )
			break;

		str = res;

		if( is_patch ) {
			entity->patches.add( patch );
		}
		else {
			entity->brushes.add( brush );
		}
	}

	str = SkipToken( str, "}" );

	if( str.ptr == NULL ) {
		entity->brushes.shutdown();
		entity->patches.shutdown();
	}

	return str;
}

struct FaceVert {
	Vec3 pos;
	Vec2 uv;
};

constexpr size_t MAX_FACE_VERTS = 32;

using FaceVerts = StaticArray< FaceVert, MAX_FACE_VERTS >;

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

static u32 AddMaterial( BSP * bsp, Span< const char > name ) {
	for( size_t i = 0; i < bsp->materials->size(); i++ ) {
		if( StrEqual( name, ( *bsp->materials )[ i ].name ) ) {
			return i;
		}
	}

	BSPMaterial material = { };
	GetMaterialFlags( name, &material.surface_flags, &material.content_flags );
	assert( name.n + 1 < sizeof( material.name ) );
	memcpy( material.name, name.ptr, name.n );
	return bsp->materials->add( material );
}

static bool IsNearlyAxial( Vec3 v ) {
	for( int i = 0; i < 3; i++ ) {
		if( Abs( v[ i ] ) >= 0.99999f ) {
			return true;
		}
	}

	return false;
}

static void AddBSPPlane( BSP * bsp, Plane plane, u32 material, bool bevel = false ) {
	if( !bevel && IsNearlyAxial( plane.normal ) )
		return;

	BSPBrushFace bspface;
	bspface.planenum = bsp->planes->size();
	bspface.material = material;
	bsp->brush_faces->add( bspface );
	bsp->planes->add( plane );
}

static void AddBevelPlanes( BSP * bsp, const MinMax3 & bounds, u32 material ) {
	for( int i = 0; i < 6; i++ ) {
		Plane plane = { };
		float sign = i % 2 == 0 ? -1.0f : 1.0f;
		plane.normal[ i / 2 ] = sign;
		plane.distance = sign * ( i % 2 == 0 ? bounds.mins : bounds.maxs )[ i / 2 ];
		AddBSPPlane( bsp, plane, material, true );
	}
}

static void AddBrush( BSP * bsp, size_t first_face, u32 material ) {
	BSPBrush brush;
	brush.first_face = first_face;
	brush.material = material;
	brush.num_faces = bsp->brush_faces->size() - first_face;
	bsp->brushes->add( brush );
}

static bool FaceToVerts( FaceVerts * verts, Span< const Plane > planes, size_t face ) {
	for( size_t i = 0; i < planes.n; i++ ) {
		if( i == face )
			continue;

		for( size_t j = i + 1; j < planes.n; j++ ) {
			if( j == face )
				continue;

			Vec3 p;
			if( !Intersect3PlanesPoint( &p, planes[ face ], planes[ i ], planes[ j ] ) )
				continue;

			if( !PointInsideBrush( planes, p ) )
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

static bool ProcessBrush( BSP * bsp, MinMax3 * bounds, const Brush & brush ) {
	ZoneScoped;

	Plane planes[ MAX_BRUSH_FACES ];
	*bounds = MinMax3::Empty();

	for( size_t i = 0; i < brush.faces.n; i++ ) {
		const Face & face = brush.faces.elems[ i ];
		if( !PlaneFrom3Points( &planes[ i ], face.plane[ 0 ], face.plane[ 1 ], face.plane[ 2 ] ) ) {
			return false;
		}
	}

	for( size_t i = 0; i < brush.faces.n; i++ ) {
		// generate arbitrary list of points
		FaceVerts verts = { };
		if( !FaceToVerts( &verts, Span< const Plane >( planes, brush.faces.n ), i ) ) {
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

			if( !IsNodrawMaterial( brush.faces.span()[ i ].material ) ) {
				BSPVertex vertex = { };
				vertex.position = unprojected;
				vertex.normal = planes[ i ].normal;
				bsp->vertices->add( vertex );
			}

			*bounds = Union( *bounds, unprojected );
		}

		if( !IsNodrawMaterial( brush.faces.span()[ i ].material ) ) {
			size_t base_vert = bsp->vertices->size() - verts.n;
			for( size_t j = 0; j < verts.n - 2; j++ ) {
				bsp->triangles->add( { u32( base_vert ), u32( base_vert + j + 2 ), u32( base_vert + j + 1 ) } );
			}
		}
	}

	{
		size_t first_face = bsp->brush_faces->size();

		u32 brush_material = 0;
		u32 content_flags = 0;

		for( size_t i = 0; i < brush.faces.n; i++ ) {
			brush_material = AddMaterial( bsp, brush.faces.span()[ i ].material );
			u32 face_flags = ( *bsp->materials )[ brush_material ].content_flags;
			if( i == 0 ) {
				content_flags = face_flags;
				break; // TODO
			}
			else if( face_flags != content_flags ) {
				Fatal( "no" );
			}
		}

		AddBevelPlanes( bsp, *bounds, brush_material );

		for( size_t i = 0; i < brush.faces.n; i++ ) {
			u32 material = AddMaterial( bsp, brush.faces.span()[ i ].material );
			AddBSPPlane( bsp, planes[ i ], material );
		}

		AddBrush( bsp, first_face, brush_material );
	}

	return true;
}

static ControlPoint Lerp( const ControlPoint & a, float t, const ControlPoint & b ) {
	ControlPoint res;
	res.position = Lerp( a.position, t, b.position );
	res.uv = Lerp( a.uv, t, b.uv );
	return res;
}

template< typename T >
static T Order2Bezier( float t, const T & a, const T & b, const T & c ) {
	T d = Lerp( a, t, b );
	T e = Lerp( b, t, c );
	return Lerp( d, t, e );
}

template< typename T >
static T Order2Bezier2D( float tx, float ty, Span2D< const T > control ) {
	T a = Order2Bezier( ty, control( 0, 0 ), control( 0, 1 ), control( 0, 2 ) );
	T b = Order2Bezier( ty, control( 1, 0 ), control( 1, 1 ), control( 1, 2 ) );
	T c = Order2Bezier( ty, control( 2, 0 ), control( 2, 1 ), control( 2, 2 ) );
	return Order2Bezier( tx, a, b, c );
}

static float PointSegmentDistance( Vec3 start, Vec3 end, Vec3 p ) {
	return Length( p - ClosestPointOnSegment( start, end, p ) );
}

static int Order2BezierSubdivisions( Vec3 control0, Vec3 control1, Vec3 control2, float max_error, Vec3 p0, Vec3 p1, float t0, float t1 ) {
	float midpoint_t0t1 = ( t0 + t1 ) * 0.5f;
	Vec3 p = Order2Bezier( midpoint_t0t1, control0, control1, control2 );
	if( PointSegmentDistance( p0, p1, p ) <= max_error )
		return 1;

	int l = Order2BezierSubdivisions( control0, control1, control2, max_error, p0, p, t0, midpoint_t0t1 );
	int r = Order2BezierSubdivisions( control0, control1, control2, max_error, p, p1, midpoint_t0t1, t1 );
	return 1 + Max2( l, r );
}

static int Order2BezierSubdivisions( Vec3 control0, Vec3 control1, Vec3 control2, float max_error ) {
	if( control0 == control1 || control1 == control2 || control0 == control2 )
		return 1;
	return Order2BezierSubdivisions( control0, control1, control2, max_error, control0, control2, 0.0f, 1.0f );
}

static BSPVertex SampleBezierSurface( float tx, float ty, Span2D< const ControlPoint > control ) {
	/*
	 * there are useful bezier surfaces with singularities at (0,0) and
	 * (1,1), we couldn't find a nice way to solve this so just +/- epsilon
	 */
	float ctx = Clamp( 0.000001f, tx, 0.999999f );
	float cty = Clamp( 0.000001f, ty, 0.999999f );

	// https://www.gamedev.net/tutorials/programming/math-and-physics/practical-guide-to-bezier-surfaces-r3170/
	constexpr Mat3 M(
		1.0f, -2.0f, 1.0f,
		-2.0f, 2.0f, 0.0f,
		1.0f, 0.0f, 0.0f
	);
	Vec3 powers_cx( ctx * ctx, ctx, 1.0f );
	Vec3 powers_cy( cty * cty, cty, 1.0f );
	Vec3 powers_dcx( 2.0f * ctx, 1.0f, 0.0f );
	Vec3 powers_dcy( 2.0f * cty, 1.0f, 0.0f );
	Vec3 tangent( 0.0f );
	Vec3 bitangent( 0.0f );
	for( int i = 0; i < 3; i++ ) {
		Mat3 curve(
			control( 0, 0 ).position[ i ], control( 1, 0 ).position[ i ], control( 2, 0 ).position[ i ],
			control( 0, 1 ).position[ i ], control( 1, 1 ).position[ i ], control( 2, 1 ).position[ i ],
			control( 0, 2 ).position[ i ], control( 1, 2 ).position[ i ], control( 2, 2 ).position[ i ]
		);
		Vec3 res_dx = ( M * curve * M * powers_dcx ) * powers_cy;
		Vec3 res_dy = ( M * curve * M * powers_cx ) * powers_dcy;
		tangent[ i ] = res_dx.x + res_dx.y + res_dx.z;
		bitangent[ i ] = res_dy.x + res_dy.y + res_dy.z;
	}

	ControlPoint cp = Order2Bezier2D( tx, ty, control );

	BSPVertex v = { };
	v.position = cp.position;
	v.uv = cp.uv;
	v.normal = Normalize( Cross( tangent, bitangent ) );

	return v;
}

static void PatchToVerts( BSP * bsp, const Patch & patch ) {
	u32 num_patches_x = ( patch.w - 1 ) / 2;
	u32 num_patches_y = ( patch.h - 1 ) / 2;

	constexpr float max_error = 0.5f;
	s32 tess_x = 2;
	s32 tess_y = 2;

	Span2D< const ControlPoint > control_points( patch.control_points, patch.w, patch.h );

	for( u32 patch_y = 0; patch_y < num_patches_y; patch_y++ ) {
		for( u32 patch_x = 0; patch_x < num_patches_x; patch_x++ ) {
			Span2D< const ControlPoint > points = control_points.slice( patch_x * 2, patch_y * 2, 3, 3 );
			for( int j = 0; j < 3; j++ ) {
				tess_x = Max2( tess_x, Order2BezierSubdivisions( points( 0, j ).position, points( 1, j ).position, points( 2, j ).position, max_error ) );
				tess_y = Max2( tess_y, Order2BezierSubdivisions( points( j, 0 ).position, points( j, 1 ).position, points( j, 2 ).position, max_error ) );
			}
		}
	}

	for( u32 patch_y = 0; patch_y < num_patches_y; patch_y++ ) {
		for( u32 patch_x = 0; patch_x < num_patches_x; patch_x++ ) {
			Span2D< const ControlPoint > points = control_points.slice( patch_x * 2, patch_y * 2, 3, 3 );
			u32 base_vert = bsp->vertices->size();

			for( int y = 0; y <= tess_y; y++ ) {
				for( int x = 0; x <= tess_x; x++ ) {
					float tx = float( x ) / float( tess_x );
					float ty = float( y ) / float( tess_y );
					bsp->vertices->add( SampleBezierSurface( tx, ty, points ) );
				}
			}

			for( int y = 0; y < tess_y; y++ ) {
				for( int x = 0; x < tess_x; x++ ) {
					u32 bl = ( y + 0 ) * ( tess_x + 1 ) + x + 0 + base_vert;
					u32 br = ( y + 0 ) * ( tess_x + 1 ) + x + 1 + base_vert;
					u32 tl = ( y + 1 ) * ( tess_x + 1 ) + x + 0 + base_vert;
					u32 tr = ( y + 1 ) * ( tess_x + 1 ) + x + 1 + base_vert;

					bsp->triangles->add( { bl, tl, br } );
					bsp->triangles->add( { br, tl, tr } );
				}
			}
		}
	}
}

static void AddPatchBrushes( BSP * bsp, DynamicArray< MinMax3 > * brush_bounds, u32 material, size_t first_patch_tri ) {
	ZoneScoped;

	for( size_t i = first_patch_tri; i < bsp->triangles->size(); i++ ) {
		BSPTriangle tri = ( *bsp->triangles )[ i ];
		Vec3 p0 = ( *bsp->vertices )[ tri.a ].position;
		Vec3 p1 = ( *bsp->vertices )[ tri.b ].position;
		Vec3 p2 = ( *bsp->vertices )[ tri.c ].position;

		Plane plane;
		if( !PlaneFrom3Points( &plane, p0, p1, p2 ) )
			continue;

		size_t first_face = bsp->brush_faces->size();

		/*
		 * for axis aligned brushes the back facing plane will get
		 * rejected, and we need to make sure the brush has non-zero
		 * volume to make sure it's not inside-out. so extend the
		 * initial bounds by 1 unit backwards to make sure the bounding
		 * box has non-zero volume
		 */
		Vec3 back_face_point = p0 - plane.normal;

		MinMax3 bounds = MinMax3::Empty();
		bounds = Union( bounds, p0 );
		bounds = Union( bounds, p1 );
		bounds = Union( bounds, p2 );
		bounds = Union( bounds, back_face_point );
		AddBevelPlanes( bsp, bounds, material );

		AddBSPPlane( bsp, plane, material );

		Plane back_plane = { -plane.normal, -plane.distance + 1.0f };
		AddBSPPlane( bsp, back_plane, material );

		Vec3 points[] = { p0, p1, p2 };
		for( int e = 0; e < 3; e++ ) {
			Vec3 edge = points[ ( e + 1 ) % 3 ] - points[ e ];
			Vec3 edge_normal = Normalize( Cross( edge, plane.normal ) );
			Plane edge_plane = { -edge_normal, -Dot( points[ e ], edge_normal ) }; // TODO: flip these when we switch to CCW
			AddBSPPlane( bsp, edge_plane, material );
		}

		AddBrush( bsp, first_face, material );
		brush_bounds->add( bounds );
	}
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

struct KDTreeNode {
	bool leaf;

	// node stuff
	u8 axis;
	float distance;
	u32 above_child;

	// leaf stuff
	u32 first_brush;
	u32 num_brushes;
};

struct CandidatePlane {
	float distance;
	u32 brush_id;
	bool start_edge;
};

struct CandidatePlanes {
	Span< CandidatePlane > axes[ 3 ];
};

int MaxAxis( MinMax3 bounds ) {
	Vec3 dims = bounds.maxs - bounds.mins;
	if( dims.x > dims.y && dims.x > dims.z )
		return 0;
	return dims.y > dims.z ? 1 : 2;
}

float SurfaceArea( MinMax3 bounds ) {
	Vec3 dims = bounds.maxs - bounds.mins;
	return 2.0f * ( dims.x * dims.y + dims.x * dims.z + dims.y * dims.z );
}

CandidatePlanes BuildCandidatePlanes( Allocator * a, Span< const u32 > brush_ids, Span< const MinMax3 > brush_bounds ) {
	ZoneScoped;

	CandidatePlanes planes;

	for( int i = 0; i < 3; i++ ) {
		ZoneScopedN( "iter" );
		Span< CandidatePlane > axis = ALLOC_SPAN( a, CandidatePlane, brush_ids.n * 2 );
		planes.axes[ i ] = axis;

		for( u32 j = 0; j < brush_ids.n; j++ ) {
			u32 brush_id = brush_ids[ j ];
			axis[ j * 2 + 0 ] = { brush_bounds[ brush_id ].mins[ i ], brush_id, true };
			axis[ j * 2 + 1 ] = { brush_bounds[ brush_id ].maxs[ i ], brush_id, false };
		}

		{
			ZoneScopedN( "sort" );
			std::sort( axis.begin(), axis.end(), []( const CandidatePlane & a, const CandidatePlane & b ) {
				if( a.distance == b.distance )
					return a.start_edge < b.start_edge;
				return a.distance < b.distance;
			} );
		}
	}

	return planes;
}

void Split( MinMax3 bounds, int axis, float distance, MinMax3 * below, MinMax3 * above ) {
	*below = bounds;
	*above = bounds;

	below->maxs[ axis ] = distance;
	above->mins[ axis ] = distance;
}

static MinMax3 HugeBounds() {
	return MinMax3( Vec3( -FLT_MAX ), Vec3( FLT_MAX ) );
}

static s32 MakeLeaf( BSP * bsp, Span< const u32 > brush_ids ) {
	ZoneScoped;

	BSPLeaf leaf = { };
	leaf.bounds = HugeBounds();
	leaf.firstLeafBrush = bsp->brush_ids->size();
	leaf.numLeafBrushes = brush_ids.n;

	bsp->brush_ids->add_many( brush_ids );

	size_t leaf_id = bsp->leaves->add( leaf );
	return -s32( leaf_id + 1 );
}

s32 BuildKDTreeRecursive( TempAllocator * temp, BSP * bsp, Span< const u32 > brush_ids, Span< const MinMax3 > brush_bounds, MinMax3 node_bounds, u32 max_depth ) {
	ZoneScoped;

	if( brush_ids.n <= 1 || max_depth == 0 ) {
		return MakeLeaf( bsp, brush_ids );
	}

	float best_cost = INFINITY;
	int best_axis = 0;
	size_t best_plane = 0;

	float node_surface_area = SurfaceArea( node_bounds );

	CandidatePlanes candidate_planes = BuildCandidatePlanes( temp, brush_ids, brush_bounds );

	{
		ZoneScopedN( "Find best split" );

		for( int i = 0; i < 3; i++ ) {
			int axis = ( MaxAxis( node_bounds ) + i ) % 3;

			size_t num_below = 0;
			size_t num_above = brush_ids.n;

			for( size_t j = 0; j < candidate_planes.axes[ axis ].n; j++ ) {
				const CandidatePlane & plane = candidate_planes.axes[ axis ][ j ];

				if( !plane.start_edge ) {
					num_above--;
				}

				if( plane.distance > node_bounds.mins[ axis ] && plane.distance < node_bounds.maxs[ axis ] ) {
					MinMax3 below_bounds, above_bounds;
					Split( node_bounds, axis, plane.distance, &below_bounds, &above_bounds );

					float frac_below = SurfaceArea( below_bounds ) / node_surface_area;
					float frac_above = SurfaceArea( above_bounds ) / node_surface_area;

					float empty_bonus = num_below == 0 || num_above == 0 ? 0.5f : 1.0f;

					constexpr float traversal_cost = 1.0f;
					constexpr float intersect_cost = 80.0f;
					float cost = traversal_cost + intersect_cost * empty_bonus * ( frac_below * num_below + frac_above * num_above );

					if( cost < best_cost ) {
						best_cost = cost;
						best_axis = axis;
						best_plane = j;
					}
				}

				if( plane.start_edge ) {
					num_below++;
				}
			}

			if( best_cost != INFINITY ) {
				break;
			}
		}
	}

	if( best_cost == INFINITY ) {
		return MakeLeaf( bsp, brush_ids );
	}

	// make node
	float distance = candidate_planes.axes[ best_axis ][ best_plane ].distance;

	Plane split;
	split.normal = Vec3( 0.0f );
	split.normal[ best_axis ] = 1.0f;
	split.distance = distance;
	size_t split_id = bsp->planes->add( split );

	size_t node_id = bsp->nodes->add( { } );

	BSPNode node;
	node.planenum = split_id;
	node.bounds = HugeBounds();

	MinMax3 below_bounds, above_bounds;
	Split( node_bounds, best_axis, distance, &below_bounds, &above_bounds );

	DynamicArray< u32 > below_brushes( temp );
	DynamicArray< u32 > above_brushes( temp );

	{
		ZoneScopedN( "Classify above/below" );

		for( size_t i = 0; i < best_plane; i++ ) {
			if( candidate_planes.axes[ best_axis ][ i ].start_edge ) {
				below_brushes.add( candidate_planes.axes[ best_axis ][ i ].brush_id );
			}
		}

		for( size_t i = best_plane + 1; i < candidate_planes.axes[ best_axis ].n; i++ ) {
			if( !candidate_planes.axes[ best_axis ][ i ].start_edge ) {
				above_brushes.add( candidate_planes.axes[ best_axis ][ i ].brush_id );
			}
		}
	}

	node.children[ 1 ] = BuildKDTreeRecursive( temp, bsp, below_brushes.span(), brush_bounds, below_bounds, max_depth - 1 );
	node.children[ 0 ] = BuildKDTreeRecursive( temp, bsp, above_brushes.span(), brush_bounds, above_bounds, max_depth - 1 );

	( *bsp->nodes )[ node_id ] = node;

	return node_id;
}

u32 Log2( u64 x ) {
	u32 log = 0;
	x >>= 1;

	while( x > 0 ) {
		x >>= 1;
		log++;
	}

	return log;
}

void BuildKDTree( TempAllocator * temp, BSP * bsp, Span< const MinMax3 > brush_bounds ) {
	ZoneScoped;

	MinMax3 tree_bounds = MinMax3::Empty();
	for( const MinMax3 & bounds : brush_bounds ) {
		tree_bounds = Union( bounds, tree_bounds );
	}

	u32 max_depth = roundf( 8.0f + 1.3f * Log2( bsp->brushes->size() ) );

	DynamicArray< u32 > all_brushes( temp );
	for( u32 i = 0; i < bsp->brushes->size(); i++ ) {
		all_brushes.add( i );
	}

	BuildKDTreeRecursive( temp, bsp, all_brushes.span(), brush_bounds, tree_bounds, max_depth );

	if( bsp->nodes->size() == 0 ) {
		Plane split;
		split.normal = Vec3( 0.0f, 0.0f, 1.0f );
		split.distance = 1.0f;
		size_t split_id = bsp->planes->add( split );

		BSPNode node = { };
		node.planenum = split_id;
		node.bounds = HugeBounds();
		node.children[ 0 ] = -1;
		node.children[ 1 ] = -1;
		bsp->nodes->add( node );
	}
}

template< typename T >
void Pack( DynamicArray< u8 > & packed, BSPHeader * header, BSPLump lump, Span< T > data ) {
	size_t padding = packed.num_bytes() % alignof( T );
	if( padding != 0 )
		padding = alignof( T ) - padding;
	size_t before_padding = packed.extend( padding );
	if( padding != 0 ) {
		memset( &packed[ before_padding ], 0, padding );
	}

	size_t offset = packed.extend( data.num_bytes() );
	memcpy( &packed[ offset ], data.ptr, data.num_bytes() );

	header->lumps[ lump ].offset = offset;
	header->lumps[ lump ].length = data.num_bytes();

	ggprint( "lump {-20} is size {.2}MB\n", lump_names[ lump ], data.num_bytes() / 1000.0f / 1000.0f );
}

static void WriteBSP( TempAllocator * temp, BSP * bsp ) {
	ZoneScoped;

	DynamicArray< u8 > packed( temp );

	BSPHeader header = { };
	memcpy( &header.magic, "IBSP", 4 );

	size_t header_offset = packed.extend( sizeof( header ) );

	Pack( packed, &header, BSPLump_Entities, bsp->entities->span() );
	Pack( packed, &header, BSPLump_Materials, bsp->materials->span() );
	Pack( packed, &header, BSPLump_Planes, bsp->planes->span() );
	Pack( packed, &header, BSPLump_Nodes, bsp->nodes->span() );
	Pack( packed, &header, BSPLump_Leaves, bsp->leaves->span() );
	Pack( packed, &header, BSPLump_LeafBrushes, bsp->brush_ids->span() );
	Pack( packed, &header, BSPLump_Models, bsp->models->span() );
	Pack( packed, &header, BSPLump_Brushes, bsp->brushes->span() );
	Pack( packed, &header, BSPLump_BrushSides, bsp->brush_faces->span() );
	Pack( packed, &header, BSPLump_Vertices, bsp->vertices->span() );
	Pack( packed, &header, BSPLump_Indices, bsp->triangles->span() );
	Pack( packed, &header, BSPLump_Faces, bsp->faces->span() );

	memcpy( &packed[ header_offset ], &header, sizeof( header ) );

	WriteFile( temp, "base/maps/gg.bsp", packed.ptr(), packed.num_bytes() );
}

Span< const char > GetKey( Span< const KeyValue > kvs, const char * key ) {
	for( KeyValue kv : kvs ) {
		if( StrCaseEqual( kv.key, key ) ) {
			return kv.value;
		}
	}

	return Span< const char >( NULL, 0 );
}

int main() {
	constexpr size_t arena_size = 1024 * 1024 * 1024; // 1GB
	ArenaAllocator arena( ALLOC_SIZE( sys_allocator, arena_size, 16 ), arena_size );
	TempAllocator temp = arena.temp();

	InitFS();
	InitMaterials();

	size_t carfentanil_len;
	char * carfentanil = ReadFileString( sys_allocator, "source/tools/dieselmap/trivial.map", &carfentanil_len );
	assert( carfentanil != NULL );
	defer { FREE( sys_allocator, carfentanil ); };

	Span< const char > map( carfentanil, carfentanil_len );

	{
		ZoneScopedN( "Erase comments" );

		for( size_t i = 0; i < map.n; i++ ) {
			Span< const char > comment;
			Span< const char > res = ParseComment( &comment, map + i );
			if( res.ptr == NULL )
				continue;

			memset( const_cast< char * >( comment.ptr ), ' ', comment.n );
			i += comment.n;
		}
	}

	DynamicArray< Entity > entities( sys_allocator );
	defer {
		for( Entity & entity : entities ) {
			entity.brushes.shutdown();
			entity.patches.shutdown();
		}
	};

	{
		ZoneScopedN( "Parsing" );
		map = CaptureNOrMore( &entities, map, 1, ParseEntity );
		map = SkipWhitespace( map );
	}

	bool ok = map.ptr != NULL && map.n == 0;
	if( !ok ) {
		ggprint( "failed\n" );
		return 1;
	}

	FrameMark;

	{
		ZoneScopedN( "Flatten func_groups" );

		for( Entity & entity : entities ) {
			if( GetKey( entity.kvs.span(), "classname" ) != "func_group" )
				continue;

			entities[ 0 ].brushes.add_many( entity.brushes.span() );
			entities[ 0 ].patches.add_many( entity.patches.span() );

			entity.brushes.clear();
			entity.patches.clear();
		}
	}

	FrameMark;

	DynamicString entstr( sys_allocator );
	DynamicArray< BSPMaterial > materials( sys_allocator );
	DynamicArray< Plane > planes( sys_allocator );
	DynamicArray< BSPVertex > vertices( sys_allocator );
	DynamicArray< BSPTriangle > triangles( sys_allocator );
	DynamicArray< BSPFace > faces( sys_allocator );
	DynamicArray< BSPModel > models( sys_allocator );
	DynamicArray< BSPNode > nodes( sys_allocator );
	DynamicArray< BSPLeaf > leaves( sys_allocator );
	DynamicArray< BSPBrush > brushes( sys_allocator );
	DynamicArray< u32 > brush_ids( sys_allocator );
	DynamicArray< BSPBrushFace > brush_faces( sys_allocator );

	BSP bsp;
	bsp.entities = &entstr;
	bsp.materials = &materials;
	bsp.planes = &planes;
	bsp.vertices = &vertices;
	bsp.triangles = &triangles;
	bsp.faces = &faces;
	bsp.models = &models;
	bsp.nodes = &nodes;
	bsp.leaves = &leaves;
	bsp.brushes = &brushes;
	bsp.brush_ids = &brush_ids;
	bsp.brush_faces = &brush_faces;

	DynamicArray< MinMax3 > brush_bounds( sys_allocator );

	// TODO: model per entity
	for( Entity & entity : entities ) {
		for( Brush & brush : entity.brushes ) {
			MinMax3 bounds;
			ggprint( "brush id {}\n", brush_bounds.size() + 1 );
			bool asd = ProcessBrush( &bsp, &bounds, brush );
			assert( asd );
			brush_bounds.add( bounds );
		}

		for( const Patch & patch : entity.patches ) {
			u32 material = AddMaterial( &bsp, patch.material );
			size_t patch_first_tri = bsp.triangles->size();
			PatchToVerts( &bsp, patch );
			AddPatchBrushes( &bsp, &brush_bounds, material, patch_first_tri );
		}
	}

	FrameMark;

	BuildKDTree( &temp, &bsp, brush_bounds.span() );

	for( const Entity & entity : entities ) {
		if( GetKey( entity.kvs.span(), "classname" ) == "func_group" )
			continue;

		bsp.entities->append( "{{\n" );
		for( KeyValue kv : entity.kvs.span() ) {
			bsp.entities->append( "\t\"{}\" \"{}\"\n", kv.key, kv.value );
		}
		bsp.entities->append( "}}\n" );
	}

	{
		BSPFace face = { };
		face.type = s32( FaceType_Mesh );
		face.bounds = HugeBounds();
		face.first_vertex = 0;
		face.num_vertices = bsp.vertices->size();
		face.first_index = 0;
		face.num_indices = bsp.triangles->size() * 3;
		bsp.faces->add( face );
	}

	{
		BSPModel model;
		model.bounds = HugeBounds();
		model.first_face = 0;
		model.num_faces = bsp.faces->size();
		model.first_brush = 0;
		model.num_brushes = bsp.brushes->size();

		for( const Entity & entity : entities ) {
			for( const Brush & brush : entity.brushes.span() ) {
				// model.bounds = Union( brush.bounds, model.bounds );
			}
		}

		bsp.models->add( model );
	}

	WriteBSP( &temp, &bsp );

	// TODO: perf
	// - remove sorts
	//
	// TODO: generate render geometry
	// - merge meshes by material/entity
	// - figure out what postprocessing we need e.g. welding
	// - meshopt
	//
	// TODO: parse materials and set solid bits etc
	//
	// - generate shit for all map models
	//
	// TODO: new map format
	// - see bsp2.cpp
	// - flip CW to CCW winding. q3 bsp was CW lol

	FREE( sys_allocator, arena.get_memory() );

	ShutdownMaterials();
	ShutdownFS();

	return 0;
}
