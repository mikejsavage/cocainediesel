#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/span2d.h"
#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

static TextureBuffer decal_buffer;
static TextureBuffer decal_index_buffer;
static TextureBuffer decal_tile_buffer;

static u32 last_viewport_width, last_viewport_height;

// gets copied directly to GPU so packing order is important
struct Decal {
	Vec3 origin;
	float radius;
	Vec3 normal;
	float angle;
	Vec4 color;
	Vec4 uvwh;
};

STATIC_ASSERT( sizeof( Decal ) == 4 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( Decal ) % alignof( Decal ) == 0 );

static constexpr u32 MAX_DECALS = 100000;
static constexpr u32 MAX_DECALS_PER_TILE = 100;

static Decal decals[ MAX_DECALS ];
static u32 num_decals;

void InitDecals() {
	decal_buffer = NewTextureBuffer( TextureBufferFormat_Floatx4, MAX_DECALS * sizeof( Decal ) / sizeof( Vec4 ) );

	last_viewport_width = U32_MAX;
	last_viewport_height = U32_MAX;
}

void ShutdownDecals() {
	DeleteTextureBuffer( decal_buffer );
	DeleteTextureBuffer( decal_index_buffer );
	DeleteTextureBuffer( decal_tile_buffer );
}

void AddDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color ) {
	if( num_decals >= ARRAY_COUNT( decals ) )
		return;

	Decal * decal = &decals[ num_decals ];

	if( !TryFindDecal( name, &decal->uvwh ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	decal->origin = origin;
	decal->normal = normal;
	decal->radius = radius;
	decal->angle = angle;
	decal->color = color;

	num_decals++;
}

struct DecalTile {
	u32 indices[ MAX_DECALS_PER_TILE ];
	u32 num_decals;
};

struct GPUDecalTile {
	u32 first_decal;
	u32 num_decals;
};

/*
 * implementation of "2D Polyhedral Bounds of a Clipped, Perspective-Projected
 * 3D Sphere" in JCGT
 *
 * https://pdfs.semanticscholar.org/9e5f/117618c96175ce683e9b708bacdfb8252e38.pdf
 */
static MinMax3 ScreenSpaceBoundsForAxis( Vec2 axis, Vec3 view_space_origin, float radius ) {
	float z_near = -frame_static.near_plane;
	bool fully_infront_of_near_plane = view_space_origin.z + radius < z_near;

	Vec2 C = Vec2( Dot( Vec3( axis, 0.0f ), view_space_origin ), view_space_origin.z );

	float tSquared = LengthSquared( C ) - radius * radius;
	bool camera_outside_sphere = tSquared > 0.0f;

	Vec2 min, max;

	if( camera_outside_sphere ) {
		float t = sqrtf( tSquared );
		float cos_theta = t / Length( C );
		float sin_theta = radius / Length( C );

		Mat2 Rtheta = Mat2Rotation( cos_theta, sin_theta );
		Mat2 Rmintheta = Mat2Rotation( cos_theta, -sin_theta );

		max = cos_theta * ( Rtheta * C );
		min = cos_theta * ( Rmintheta * C );
	}

	if( !fully_infront_of_near_plane ) {
		float chord_half_length = sqrtf( radius * radius - Square( z_near - C.y ) );

		if( !camera_outside_sphere || max.y > z_near ) {
			max.x = C.x + chord_half_length;
			max.y = z_near;
		}

		if( !camera_outside_sphere || min.y > z_near ) {
			min.x = C.x - chord_half_length;
			min.y = z_near;
		}
	}

	return MinMax3(
		Vec3( min.x * axis, min.y ),
		Vec3( max.x * axis, max.y )
	);
}

static MinMax2 SphereScreenSpaceBounds( Vec3 origin, float radius ) {
	Vec3 view_space_origin = ( frame_static.V * Vec4( origin, 1.0f ) ).xyz();

	bool fully_behind_near_plane = view_space_origin.z - radius >= -frame_static.near_plane;
	if( fully_behind_near_plane ) {
		return MinMax2( Vec2( -1.0f ), Vec2( -1.0f ) );
	}

	MinMax3 x_bounds = ScreenSpaceBoundsForAxis( Vec2( 1.0f, 0.0f ), view_space_origin, radius );
	MinMax3 y_bounds = ScreenSpaceBoundsForAxis( Vec2( 0.0f, 1.0f ), view_space_origin, radius );

	Mat4 P = frame_static.P;
	float min_x = Dot( x_bounds.mins, P.row0().xyz() ) / Dot( x_bounds.mins, P.row3().xyz() );
	float max_x = Dot( x_bounds.maxs, P.row0().xyz() ) / Dot( x_bounds.maxs, P.row3().xyz() );
	float min_y = Dot( y_bounds.mins, P.row1().xyz() ) / Dot( y_bounds.mins, P.row3().xyz() );
	float max_y = Dot( y_bounds.maxs, P.row1().xyz() ) / Dot( y_bounds.maxs, P.row3().xyz() );

	return MinMax2( Vec2( min_x, min_y ), Vec2( max_x, max_y ) );
}

void UploadDecalBuffers() {
	ZoneScoped;

	u32 rows = ( frame_static.viewport_height + TILE_SIZE - 1 ) / TILE_SIZE;
	u32 cols = ( frame_static.viewport_width + TILE_SIZE - 1 ) / TILE_SIZE;

	if( frame_static.viewport_width != last_viewport_width || frame_static.viewport_height != last_viewport_height ) {
		ZoneScopedN( "Reallocate TBOs" );

		decal_tile_buffer = NewTextureBuffer( TextureBufferFormat_U32x2, rows * cols );
		decal_index_buffer = NewTextureBuffer( TextureBufferFormat_U32, rows * cols * MAX_DECALS_PER_TILE );

		last_viewport_width = frame_static.viewport_width;
		last_viewport_height = frame_static.viewport_height;
	}

	Span2D< DecalTile > tiles = ALLOC_SPAN2D( sys_allocator, DecalTile, cols, rows );
	memset( tiles.ptr, 0, tiles.num_bytes() );
	defer { FREE( sys_allocator, tiles.ptr ); };

	for( u32 i = 0; i < num_decals; i++ ) {
		MinMax2 bounds = SphereScreenSpaceBounds( decals[ i ].origin, decals[ i ].radius );
		bounds.mins.y = -bounds.mins.y;
		bounds.maxs.y = -bounds.maxs.y;
		Swap2( &bounds.mins.y, &bounds.maxs.y );

		if( bounds.maxs.x <= -1.0f || bounds.maxs.y <= -1.0f || bounds.mins.x >= 1.0f || bounds.mins.y >= 1.0f ) {
			continue;
		}

		Vec2 mins = ( bounds.mins + 1.0f ) * 0.5f * frame_static.viewport;
		mins = Clamp( Vec2( 0.0f ), mins, frame_static.viewport - 1.0f ) / float( TILE_SIZE );

		Vec2 maxs = ( bounds.maxs + 1.0f ) * 0.5f * frame_static.viewport;
		maxs = Clamp( Vec2( 0.0f ), maxs, frame_static.viewport - 1.0f ) / float( TILE_SIZE );

		for( u32 y = mins.y; y <= maxs.y; y++ ) {
			for( u32 x = mins.x; x <= maxs.x; x++ ) {
				DecalTile * tile = &tiles( x, y );
				if( tile->num_decals < ARRAY_COUNT( tile->indices ) ) {
					tile->indices[ tile->num_decals ] = i;
					tile->num_decals++;
				}
			}
		}
	}

	// pack tile buffer
	Span2D< GPUDecalTile > gpu_tiles = ALLOC_SPAN2D( sys_allocator, GPUDecalTile, cols, rows );
	defer { FREE( sys_allocator, gpu_tiles.ptr ); };

	u32 indices[ MAX_DECALS ];
	u32 num_indices = 0;
	for( u32 y = 0; y < rows; y++ ) {
		for( u32 x = 0; x < cols; x++ ) {
			const DecalTile * tile = &tiles( x, y );

			gpu_tiles( x, y ).first_decal = num_indices;
			gpu_tiles( x, y ).num_decals = tile->num_decals;

			for( u32 i = 0; i < tile->num_decals; i++ ) {
				indices[ num_indices ] = tile->indices[ i ];
				num_indices++;
			}
		}
	}

	{
		ZoneScopedN( "Upload TBOs" );
		WriteTextureBuffer( decal_buffer, decals, num_decals * sizeof( decals[ 0 ] ) );
		WriteTextureBuffer( decal_index_buffer, indices, num_indices * sizeof( indices[ 0 ] ) );
		WriteTextureBuffer( decal_tile_buffer, gpu_tiles.ptr, gpu_tiles.num_bytes() );
	}

	num_decals = 0;
}

void AddDecalsToPipeline( PipelineState * pipeline ) {
	pipeline->set_uniform( "u_Decal", UploadUniformBlock( s32( num_decals ) ) );
	pipeline->set_texture_buffer( "u_DecalData", decal_buffer );
	pipeline->set_texture_buffer( "u_DecalIndices", decal_index_buffer );
	pipeline->set_texture_buffer( "u_DecalTiles", decal_tile_buffer );
}
