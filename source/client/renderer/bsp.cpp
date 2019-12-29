#include <algorithm> // std::sort

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/assets.h"
#include "qcommon/array.h"
#include "qcommon/string.h"
#include "qcommon/span2d.h"
#include "client/renderer/renderer.h"

#include "meshoptimizer/meshoptimizer.h"
#include "zstd/zstd.h"

enum BSPLump {
	BSPLump_Entities,
	BSPLump_Materials,
	BSPLump_Planes_Unused,
	BSPLump_Nodes_Unused,
	BSPLump_Leaves_Unused,
	BSPLump_LeafFaces_Unused,
	BSPLump_LeafBrushes_Unused,
	BSPLump_Models,
	BSPLump_Brushes_Unused,
	BSPLump_BrushSides_Unused,
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
	u32 flags;
	u32 contents;
};

struct BSPModel {
	MinMax3 bounds;
	u32 first_face;
	u32 num_faces;
	u32 first_brush;
	u32 num_brushes;
};

struct BSPVertex {
	Vec3 position;
	Vec2 uv;
	Vec2 lightmap_uv;
	Vec3 normal;
	RGBA8 vertex_light;
};

struct RavenBSPVertex {
	Vec3 position;
	Vec2 uv;
	Vec2 lightmap_uv[ 4 ];
	Vec3 normal;
	RGBA8 vertex_light[ 4 ];
};

typedef s32 BSPIndex;

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

struct RavenBSPFace {
	u32 material;
	s32 fog;
	s32 type;

	u32 first_vertex;
	u32 num_vertices;
	u32 first_index;
	u32 num_indices;

	u8 lightmap_styles[ 4 ];
	u8 vertex_styles[ 4 ];

	s32 lightmap[ 4 ];
	s32 lightmap_offset[ 4 ][ 2 ];
	s32 lightmap_size[ 2 ];

	Vec3 origin;
	MinMax3 bounds;
	Vec3 normal;

	u32 patch_width;
	u32 patch_height;
};

struct BSPVisbilityHeader {
	u32 num_clusters;
	u32 cluster_size;
};

struct BSPSpans {
	Span< const char > entities;
	Span< const BSPMaterial > materials;
	Span< const BSPModel > models;
	Span< const BSPVertex > vertices;
	Span< const RavenBSPVertex > raven_vertices;
	Span< const BSPIndex > indices;
	Span< const BSPFace > faces;
	Span< const RavenBSPFace > raven_faces;

	Span< const u8 > pvs;
	u32 cluster_size;

	bool idbsp;
};

template< typename T >
static bool ParseLump( Span< T > * span, Span< const u8 > data, BSPLump lump ) {
	const BSPHeader * header = ( const BSPHeader * ) data.ptr;
	BSPLumpLocation location = header->lumps[ lump ];

	if( location.offset + location.length > data.n )
		return false;

	if( location.length % sizeof( T ) != 0 )
		return false;

	*span = ( data + location.offset ).slice( 0, location.length ).cast< const T >();
	return true;
}

static bool ParsePVS( BSPSpans * bsp, Span< const u8 > data ) {
	const BSPHeader * header = ( const BSPHeader * ) data.ptr;
	BSPLumpLocation location = header->lumps[ BSPLump_Visibility ];

	if( location.length == 0 ) {
		bsp->pvs = Span< const u8 >();
		bsp->cluster_size = 0;
		return true;
	}

	if( location.offset + location.length > data.n )
		return false;

	const BSPVisbilityHeader * vis_header = ( const BSPVisbilityHeader * ) ( data.ptr + location.offset );

	u32 pvs_size = vis_header->num_clusters * vis_header->cluster_size;
	if( location.length != sizeof( BSPVisbilityHeader ) + pvs_size )
		return false;

	bsp->pvs = Span< const u8 >( ( const u8 * ) ( vis_header + 1 ), pvs_size );
	bsp->cluster_size = vis_header->cluster_size;

	return true;
}

static bool ParseBSP( BSPSpans * bsp, Span< const u8 > data ) {
	if( data.n < sizeof( BSPHeader ) )
		return false;

	const BSPHeader * header = ( const BSPHeader * ) data.ptr;
	bsp->idbsp = memcmp( &header->magic, IDBSPHEADER, sizeof( header->magic ) ) == 0;

	bool ok = true;
	ok = ok && ParseLump( &bsp->entities, data, BSPLump_Entities );
	ok = ok && ParseLump( &bsp->materials, data, BSPLump_Materials );
	ok = ok && ParseLump( &bsp->models, data, BSPLump_Models );
	ok = ok && ParseLump( &bsp->indices, data, BSPLump_Indices );

	if( bsp->idbsp ) {
		ok = ok && ParseLump( &bsp->vertices, data, BSPLump_Vertices );
		ok = ok && ParseLump( &bsp->faces, data, BSPLump_Faces );
		bsp->raven_vertices = { };
		bsp->raven_faces = { };
	}
	else {
		bsp->vertices = { };
		bsp->faces = { };
		ok = ok && ParseLump( &bsp->raven_vertices, data, BSPLump_Vertices );
		ok = ok && ParseLump( &bsp->raven_faces, data, BSPLump_Faces );
	}

	ok = ok && ParsePVS( bsp, data );

	return ok;
}

static float ParseFogStrength( const BSPSpans * bsp ) {
	ZoneScoped;

	float default_fog_strength = 0.000015f;

	Span< const char > cursor = bsp->entities;

	if( ParseToken( &cursor, Parse_DontStopOnNewLine ) != "{" )
		return default_fog_strength;

	while( true ) {
		Span< const char > key = ParseToken( &cursor, Parse_DontStopOnNewLine );
		Span< const char > value = ParseToken( &cursor, Parse_DontStopOnNewLine );

		if( key == "" || value == "" || key == "}" )
			break;

		if( key == "fog_strength" ) {
			float f;
			if( SpanToFloat( value, &f ) ) {
				return f;
			}
		}
	}

	return default_fog_strength;
}

struct BSPDrawCall {
	u32 base_vertex;
	u32 index_offset;
	u32 num_vertices;
	const Material * material;

	bool patch;
	u32 patch_width;
	u32 patch_height;
};

struct BSPModelVertex {
	Vec3 position;
	Vec3 normal;
	Vec2 uv;
};

BSPModelVertex Lerp( const BSPModelVertex & a, float t, const BSPModelVertex & b ) {
	BSPModelVertex res;
	res.position = Lerp( a.position, t, b.position );
	res.normal = Normalize( Lerp( a.normal, t, b.normal ) );
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

static bool SortByMaterial( const BSPDrawCall & a, const BSPDrawCall & b ) {
	return a.material < b.material;
}

static void LoadBSPModel( DynamicArray< BSPModelVertex > & vertices, const BSPSpans & bsp, u64 base_hash, size_t model_idx ) {
	ZoneScoped;

	const BSPModel & bsp_model = bsp.models[ model_idx ];
	if( bsp_model.num_faces == 0 )
		return;

	DynamicArray< BSPDrawCall > draw_calls( sys_allocator );
	if( bsp.idbsp ) {
		for( u32 i = 0; i < bsp_model.num_faces; i++ ) {
			const BSPFace * face = &bsp.faces[ i + bsp_model.first_face ];
			BSPDrawCall dc;

			dc.base_vertex = face->first_vertex;
			dc.index_offset = face->first_index;
			dc.num_vertices = face->num_indices;
			dc.material = FindMaterial( bsp.materials[ face->material ].name, &world_material );

			dc.patch = face->type == FaceType_Patch;
			dc.patch_width = face->patch_width;
			dc.patch_height = face->patch_height;

			draw_calls.add( dc );
		}
	}
	else {
		for( u32 i = 0; i < bsp_model.num_faces; i++ ) {
			const RavenBSPFace * face = &bsp.raven_faces[ i + bsp_model.first_face ];
			BSPDrawCall dc;

			dc.base_vertex = face->first_vertex;
			dc.index_offset = face->first_index;
			dc.num_vertices = face->num_indices;
			dc.material = FindMaterial( bsp.materials[ face->material ].name, &world_material );

			dc.patch = face->type == FaceType_Patch;
			dc.patch_width = face->patch_width;
			dc.patch_height = face->patch_height;

			draw_calls.add( dc );
		}
	}

	std::sort( draw_calls.begin(), draw_calls.end(), SortByMaterial );

	// generate patch geometry and merge draw calls
	// TODO: this generates terrible geometry then relies on meshopt to fix it up. maybe it could be done better
	DynamicArray< u32 > indices( sys_allocator );

	DynamicArray< Model::Primitive > primitives( sys_allocator );
	Model::Primitive first;
	first.first_index = 0;
	first.num_vertices = 0;
	first.material = draw_calls[ 0 ].material; // TODO: first material may have discard
	primitives.add( first );

	for( const BSPDrawCall & dc : draw_calls ) {
		if( dc.material->discard )
			continue;

		if( dc.material != primitives.top().material ) {
			Model::Primitive prim;
			prim.first_index = primitives.top().first_index + primitives.top().num_vertices;
			prim.num_vertices = 0;
			prim.material = dc.material;
			primitives.add( prim );
		}

		if( dc.patch ) {
			ZoneScopedN( "Generate patch" );

			u32 num_patches_x = ( dc.patch_width - 1 ) / 2;
			u32 num_patches_y = ( dc.patch_height - 1 ) / 2;

			for( u32 patch_y = 0; patch_y < num_patches_y; patch_y++ ) {
				for( u32 patch_x = 0; patch_x < num_patches_x; patch_x++ ) {
					u32 control_base = ( patch_y * 2 * dc.patch_width + patch_x * 2 ) + dc.base_vertex;

					float max_error = 1.0f;
					int tess_x = 0;
					int tess_y = 0;
					{
						Span2D< const BSPModelVertex > control( &vertices[ control_base ], 3, 3, dc.patch_width );
						for( int j = 0; j < 3; j++ ) {
							tess_x = Max2( tess_x, Order2BezierSubdivisions( control( 0, j ).position, control( 1, j ).position, control( 2, j ).position, max_error ) );
							tess_y = Max2( tess_y, Order2BezierSubdivisions( control( j, 0 ).position, control( j, 1 ).position, control( j, 2 ).position, max_error ) );
						}
					}

					u32 base_vert = vertices.size();

					for( int y = 0; y <= tess_y; y++ ) {
						for( int x = 0; x <= tess_x; x++ ) {
							// add can reallocate vertices so we need to recreate this every iteration
							Span2D< const BSPModelVertex > control( &vertices[ control_base ], 3, 3, dc.patch_width );
							float tx = float( x ) / tess_x;
							float ty = float( y ) / tess_y;
							vertices.add( Order2Bezier2D( tx, ty, control ) );
						}
					}

					for( int y = 0; y < tess_y; y++ ) {
						for( int x = 0; x < tess_x; x++ ) {
							u32 bl = ( y + 0 ) * ( tess_x + 1 ) + x + 0 + base_vert;
							u32 br = ( y + 0 ) * ( tess_x + 1 ) + x + 1 + base_vert;
							u32 tl = ( y + 1 ) * ( tess_x + 1 ) + x + 0 + base_vert;
							u32 tr = ( y + 1 ) * ( tess_x + 1 ) + x + 1 + base_vert;

							indices.add( bl );
							indices.add( tl );
							indices.add( br );

							indices.add( br );
							indices.add( tl );
							indices.add( tr );

							primitives.top().num_vertices += 6;
						}
					}
				}
			}
		}
		else {
			for( u32 j = 0; j < dc.num_vertices; j++ ) {
				u32 index = dc.base_vertex + bsp.indices[ j + dc.index_offset ];
				indices.add( index );
			}

			primitives.top().num_vertices += dc.num_vertices;
		}
	}

	// TODO: meshopt

	String< 16 > suffix( "*{}", model_idx );
	Model * model = NewModel( Hash64( suffix.c_str(), suffix.len(), base_hash ) );
	*model = { };
	model->transform = Mat4::Identity();

	model->primitives = ALLOC_MANY( sys_allocator, Model::Primitive, primitives.size() );
	model->num_primitives = primitives.size();
	memcpy( model->primitives, primitives.ptr(), primitives.num_bytes() );

	MeshConfig mesh_config;
	mesh_config.ccw_winding = false;
	mesh_config.unified_buffer = NewVertexBuffer( vertices.ptr(), vertices.num_bytes() );
	mesh_config.stride = sizeof( vertices[ 0 ] );
	mesh_config.positions_offset = offsetof( BSPModelVertex, position );
	mesh_config.normals_offset = offsetof( BSPModelVertex, normal );
	mesh_config.tex_coords_offset = offsetof( BSPModelVertex, uv );
	mesh_config.num_vertices = indices.size();

	// if( num_verts <= U16_MAX ) {
	// 	DynamicArray< u16 > indices_u16( sys_allocator, indices.size() );
	// 	for( u32 i = 0; i < indices.size(); i++ ) {
	// 		indices_u16.add( indices[ i ] );
	// 	}
	// 	mesh_config.indices = NewIndexBuffer( indices.ptr(), indices.num_bytes() );
	// }
	// else {
		mesh_config.indices = NewIndexBuffer( indices.ptr(), indices.num_bytes() );
		mesh_config.indices_format = IndexFormat_U32;
	// }

	model->mesh = NewMesh( mesh_config );
}

bool LoadBSPMap( MapMetadata * map, const char * path ) {
	ZoneScoped;
	ZoneText( path, strlen( path ) );

	StringHash hash = StringHash( path );
	Span< const u8 > compressed = AssetBinary( hash );
	Span< u8 > decompressed;
	defer { FREE( sys_allocator, decompressed.ptr ); };

	if( compressed.n < 4 )
		return false;

	u32 zstd_magic = ZSTD_MAGICNUMBER;
	if( memcmp( compressed.ptr, &zstd_magic, sizeof( zstd_magic ) ) == 0 ) {
		unsigned long long const decompressed_size = ZSTD_getDecompressedSize( compressed.ptr, compressed.n );
		if( decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN ) {
			// ri.Com_Error( ERR_DROP, "Corrupt BSP" );
			return false;
		}

		decompressed = ALLOC_SPAN( sys_allocator, u8, decompressed_size );
		{
			ZoneScopedN( "ZSTD_decompress" );
			size_t r = ZSTD_decompress( decompressed.ptr, decompressed.n, compressed.ptr, compressed.n );
			if( r != decompressed_size ) {
				// ri.Com_Error( ERR_DROP, "Failed to decompress BSP: %s", ZSTD_getErrorName( r ) );
				return false;
			}
		}
	}

	BSPSpans bsp;
	if( !ParseBSP( &bsp, decompressed.ptr == NULL ? compressed : decompressed ) )
		return false;

	// create common vertex data
	u32 num_verts = bsp.idbsp ? bsp.vertices.n : bsp.raven_vertices.n;
	DynamicArray< BSPModelVertex > vertices( sys_allocator, num_verts );

	if( bsp.idbsp ) {
		for( u32 i = 0; i < num_verts; i++ ) {
			BSPModelVertex v;
			v.position = bsp.vertices[ i ].position;
			v.normal = bsp.vertices[ i ].normal;
			v.uv = bsp.vertices[ i ].uv;
			vertices.add( v );
		}
	}
	else {
		for( u32 i = 0; i < num_verts; i++ ) {
			BSPModelVertex v;
			v.position = bsp.raven_vertices[ i ].position;
			v.normal = bsp.raven_vertices[ i ].normal;
			v.uv = bsp.raven_vertices[ i ].uv;
			vertices.add( v );
		}
	}

	Span< const char > ext = FileExtension( path );
	u64 base_hash = Hash64( path, strlen( path ) - ext.n );
	for( size_t i = 0; i < bsp.models.n; i++ ) {
		LoadBSPModel( vertices, bsp, base_hash, i );
	}

	map->base_hash = base_hash;
	map->num_models = bsp.models.n;
	if( bsp.pvs.ptr != NULL ) {
		map->pvs = ALLOC_SPAN( sys_allocator, u8, bsp.pvs.n );
		memcpy( map->pvs.ptr, bsp.pvs.ptr, bsp.pvs.num_bytes() );
	}
	else {
		map->pvs = Span< u8 >();
	}
	map->cluster_size = bsp.cluster_size;
	map->fog_strength = ParseFogStrength( &bsp );

	return true;
}
