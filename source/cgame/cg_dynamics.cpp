#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/span2d.h"
#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"
#include "qcommon/array.h"

static GPUBuffer decal_tiles_buffer;
static GPUBuffer decals_buffer;
static GPUBuffer dlight_tiles_buffer;
static GPUBuffer dlights_buffer;
static GPUBuffer dynamic_count;

static u32 last_viewport_width, last_viewport_height;

// gets copied directly to GPU so packing order is important
struct Decal {
	Vec3 origin_normal; // floor( origin ) + ( normal * 0.49 + 0.5 )
	// NOTE(msc): normal can't go to 0.0 or 1.0
	float radius_angle; // floor( radius ) + ( angle / 2 / PI )
	Vec4 color_uvwh_height; // vec4( u + layer, v + floor( r * 255 ) + floor( height ) * 256, w + floor( g * 255 ), h + floor( b * 255 ) )
	// NOTE(msc): uvwh should all be < 1.0
};

struct PersistentDecal {
	Decal decal;
	s64 spawn_time;
	s64 duration;
};

STATIC_ASSERT( sizeof( Decal ) == 2 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( Decal ) % alignof( Decal ) == 0 );

// gets copied directly to GPU so packing order is important
struct DynamicLight {
	Vec3 origin_color; // floor( origin ) + ( color * 0.9 )
	float radius;
};

struct PersistentDynamicLight {
	DynamicLight dlight;
	float start_intensity;
	s64 spawn_time;
	s64 duration;
};

STATIC_ASSERT( sizeof( DynamicLight ) == 1 * 4 * sizeof( float ) );
STATIC_ASSERT( sizeof( DynamicLight ) % alignof( DynamicLight ) == 0 );

static constexpr u32 MAX_DECALS = 100000;
static constexpr u32 MAX_DECALS_PER_TILE = 50;

static constexpr u32 MAX_DLIGHTS = 100000;
static constexpr u32 MAX_DLIGHTS_PER_TILE = 50;

static constexpr u32 MAX_DYNAMICS_PER_SET = 500;

static Decal decals[ MAX_DECALS ];
static u32 num_decals;

static DynamicLight dlights[ MAX_DLIGHTS ];
static u32 num_dlights;

static PersistentDecal persistent_decals[ MAX_DECALS ];
static u32 num_persistent_decals;

static PersistentDynamicLight persistent_dlights[ MAX_DLIGHTS ];
static u32 num_persistent_dlights;

struct DecalTile {
	u32 decals[ MAX_DECALS_PER_TILE ];
};

struct DynamicLightTile {
	u32 dlights[ MAX_DLIGHTS_PER_TILE ];
};

static Span2D< DecalTile > gpu_decal_tiles;
static Span2D< DynamicLightTile > gpu_dlight_tiles;

struct DynamicCount {
	u32 decal_count;
	u32 dlight_count;
};

static Span2D< DynamicCount > gpu_dynamic_counts;

void InitDecals() {
	num_persistent_decals = 0;
	num_persistent_dlights = 0;

	last_viewport_width = U32_MAX;
	last_viewport_height = U32_MAX;

	decals_buffer = NewGPUBuffer( sizeof( decals ), "Decals" );
	dlights_buffer = NewGPUBuffer( sizeof( dlights ), "Dynamic lights" );
}

void ShutdownDecals() {
	FREE( sys_allocator, gpu_decal_tiles.ptr );
	gpu_decal_tiles.ptr = NULL;
	DeferDeleteGPUBuffer( decal_tiles_buffer );
	DeferDeleteGPUBuffer( decals_buffer );

	FREE( sys_allocator, gpu_dlight_tiles.ptr );
	gpu_dlight_tiles.ptr = NULL;
	DeferDeleteGPUBuffer( dlight_tiles_buffer );
	DeferDeleteGPUBuffer( dlights_buffer );

	FREE( sys_allocator, gpu_dynamic_counts.ptr );
	gpu_dynamic_counts.ptr = NULL;
	DeferDeleteGPUBuffer( dynamic_count );
}

void DrawDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color, float height ) {
	if( num_decals == ARRAY_COUNT( decals ) )
		return;

	Decal * decal = &decals[ num_decals ];

	Vec4 uvwh;
	if( !TryFindDecal( name, &uvwh ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	Vec3 c = Floor( color.xyz() * 255.0f );
	c.x += floorf( height ) * 256.0f;

	decal->origin_normal = Floor( origin ) + ( normal * 0.49f + 0.5f );
	decal->radius_angle = floorf( radius ) + angle / 2.0f / PI;
	decal->color_uvwh_height = Vec4( uvwh.x, uvwh.y + c.x, uvwh.z + c.y, uvwh.w + c.z );

	num_decals++;
}

void AddPersistentDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color, s64 duration, float height ) {
	if( num_persistent_decals == ARRAY_COUNT( persistent_decals ) )
		return;

	PersistentDecal * decal = &persistent_decals[ num_persistent_decals ];

	Vec4 uvwh;
	if( !TryFindDecal( name, &uvwh ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Material {} should have decal key", name );
		return;
	}

	Vec3 c = Floor( color.xyz() * 255.0f );
	c.x += floorf( height ) * 256.0f;

	decal->decal.origin_normal = Floor( origin ) + ( normal * 0.4f + 0.5f );
	decal->decal.radius_angle = floorf( radius ) + angle / 2.0f / PI;
	decal->decal.color_uvwh_height = Vec4( uvwh.x, uvwh.y + c.x, uvwh.z + c.y, uvwh.w + c.z );
	decal->spawn_time = cl.serverTime;
	decal->duration = duration;

	num_persistent_decals++;
}

void DrawPersistentDecals() {
	for( u32 i = 0; i < num_persistent_decals; i++ ) {
		if( num_decals == ARRAY_COUNT( decals ) )
			break;

		PersistentDecal * decal = &persistent_decals[ i ];
		if( decal->spawn_time + decal->duration < cl.serverTime ) {
			num_persistent_decals--;
			Swap2( decal, &persistent_decals[ num_persistent_decals ] );
			i--;
			continue;
		}

		decals[ num_decals ] = decal->decal;
		num_decals++;
	}
}

void DrawDynamicLight( Vec3 origin, Vec4 color, float intensity ) {
	if( num_dlights == ARRAY_COUNT( dlights ) )
		return;

	DynamicLight * dlight = &dlights[ num_dlights ];

	dlight->origin_color = Floor( origin ) + color.xyz() * 0.9f;
	dlight->radius = sqrtf( intensity / DLIGHT_CUTOFF );

	num_dlights++;
}

void AddPersistentDynamicLight( Vec3 origin, Vec4 color, float intensity, s64 duration ) {
	if( num_persistent_dlights == ARRAY_COUNT( persistent_dlights ) )
		return;

	PersistentDynamicLight * dlight = &persistent_dlights[ num_persistent_dlights ];

	dlight->dlight.origin_color = Floor( origin ) + color.xyz() * 0.9f;
	dlight->dlight.radius = sqrtf( intensity / DLIGHT_CUTOFF );
	dlight->start_intensity = intensity;
	dlight->spawn_time = cl.serverTime;
	dlight->duration = duration;

	num_persistent_dlights++;
}

void DrawPersistentDynamicLights() {
	for( u32 i = 0; i < num_persistent_dlights; i++ ) {
		if( num_dlights == ARRAY_COUNT( dlights ) )
			break;

		PersistentDynamicLight * dlight = &persistent_dlights[ i ];
		if( dlight->spawn_time + dlight->duration < cl.serverTime ) {
			num_persistent_dlights--;
			Swap2( dlight, &persistent_dlights[ num_persistent_dlights ] );
			i--;
			continue;
		}

		// TODO: add better curves maybe
		float fract = float( cl.serverTime - dlight->spawn_time ) / float( dlight->duration );
		float intensity = Lerp( dlight->start_intensity, fract, 0.0f );
		dlight->dlight.radius = sqrtf( intensity / DLIGHT_CUTOFF );
		dlights[ num_dlights ] = dlight->dlight;
		num_dlights++;
	}
}

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

	Vec2 min = Vec2( 0.0f );
	Vec2 max = Vec2( 0.0f );

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

void AllocateDecalBuffers() {
	u32 rows = ( frame_static.viewport_height + TILE_SIZE - 1 ) / TILE_SIZE;
	u32 cols = ( frame_static.viewport_width + TILE_SIZE - 1 ) / TILE_SIZE;

	if( frame_static.viewport_width != last_viewport_width || frame_static.viewport_height != last_viewport_height ) {
		ZoneScopedN( "Reallocate TBOs" );

		FREE( sys_allocator, gpu_decal_tiles.ptr );
		gpu_decal_tiles = ALLOC_SPAN2D( sys_allocator, DecalTile, cols, rows );
		decal_tiles_buffer = NewGPUBuffer( gpu_decal_tiles.num_bytes(), "Decal tile indices" );

		FREE( sys_allocator, gpu_dlight_tiles.ptr );
		gpu_dlight_tiles = ALLOC_SPAN2D( sys_allocator, DynamicLightTile, cols, rows );
		dlight_tiles_buffer = NewGPUBuffer( gpu_dlight_tiles.num_bytes(), "Dynamic light tile indices" );

		FREE( sys_allocator, gpu_dynamic_counts.ptr );
		gpu_dynamic_counts = ALLOC_SPAN2D( sys_allocator, DynamicCount, cols, rows );
		dynamic_count = NewGPUBuffer( gpu_dynamic_counts.num_bytes(), "Dynamics tile counts" );

		last_viewport_width = frame_static.viewport_width;
		last_viewport_height = frame_static.viewport_height;
	}
}

enum DynamicType {
	DynamicType_Decal,
	DynamicType_Light,
};

struct DynamicRect {
	DynamicType type;
	u32 minx, miny, maxx, maxy;
	u32 idx;
};

struct DynamicSet {
	u32 indices[ MAX_DYNAMICS_PER_SET ];
	DynamicType types[ MAX_DYNAMICS_PER_SET ];
	u32 num;
};

void UploadDecalBuffers() {
	ZoneScoped;

	u32 rows = ( frame_static.viewport_height + TILE_SIZE - 1 ) / TILE_SIZE;
	u32 cols = ( frame_static.viewport_width + TILE_SIZE - 1 ) / TILE_SIZE;

	DynamicArray< DynamicRect > rects( sys_allocator );

	for( u32 i = 0; i < num_dlights; i++ ) {
		u32 index = num_dlights - i - 1;
		MinMax2 bounds = SphereScreenSpaceBounds( Floor( dlights[ index ].origin_color ), dlights[ index ].radius );
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

		DynamicRect rect;
		rect.type = DynamicType_Light;
		rect.minx = mins.x;
		rect.miny = mins.y;
		rect.maxx = maxs.x;
		rect.maxy = maxs.y;
		rect.idx = num_decals + index;
		rects.add( rect );
	}

	for( u32 i = 0; i < num_decals; i++ ) {
		u32 index = num_decals - i - 1;
		MinMax2 bounds = SphereScreenSpaceBounds( Floor( decals[ index ].origin_normal ), floorf( decals[ index ].radius_angle ) );
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

		DynamicRect rect;
		rect.type = DynamicType_Decal;
		rect.minx = mins.x;
		rect.miny = mins.y;
		rect.maxx = maxs.x;
		rect.maxy = maxs.y;
		rect.idx = index;
		rects.add( rect );
	}

	{
		ZoneScopedN( "Fill buffers" );

		Span< DynamicSet > rows_coverage = ALLOC_SPAN( sys_allocator, DynamicSet, rows );
		Span< DynamicSet > cols_coverage = ALLOC_SPAN( sys_allocator, DynamicSet, cols );

		memset( rows_coverage.ptr, 0, rows_coverage.num_bytes() );
		memset( cols_coverage.ptr, 0, cols_coverage.num_bytes() );

		for( u32 i = 0; i < rects.size(); i++ ) {
			DynamicRect & rect = rects[ i ];
			for( u32 x = rect.minx; x <= rect.maxx; x++ ) {
				DynamicSet & set = cols_coverage[ x ];
				if( set.num < MAX_DYNAMICS_PER_SET ) {
					set.indices[ set.num ] = rect.idx;
					set.types[ set.num ] = rect.type;
					set.num++;
				}
			}
			for( u32 y = rect.miny; y <= rect.maxy; y++ ) {
				DynamicSet & set = rows_coverage[ y ];
				if( set.num < MAX_DYNAMICS_PER_SET ) {
					set.indices[ set.num ] = rect.idx;
					set.types[ set.num ] = rect.type;
					set.num++;
				}
			}
		}

		memset( gpu_dynamic_counts.ptr, 0, gpu_dynamic_counts.num_bytes() );

		for( u32 y = 0; y < rows; y++ ) {
			DynamicSet & y_set = rows_coverage[ y ];
			for( u32 x = 0; x < cols; x++ ) {
				DynamicSet & x_set = cols_coverage[ x ];
				u32 x_idx = 0;
				u32 y_idx = 0;
				DecalTile & decal_tile = gpu_decal_tiles( x, y );
				DynamicLightTile & dlight_tile = gpu_dlight_tiles( x, y );

				// NOTE(msc): decals guaranteed to sorted front to back / high to low
				while( x_idx < x_set.num && y_idx < y_set.num ) {
					u32 x_instance = x_set.indices[ x_idx ];
					u32 y_instance = y_set.indices[ y_idx ];
					// NOTE(msc): instance in both column & row, must be active in this cell
					if( x_instance == y_instance ) {
						DynamicType type = x_set.types[ x_idx ];
						DynamicCount &count = gpu_dynamic_counts( x, y );
						if( type == DynamicType_Decal ) {
							if( count.decal_count < MAX_DECALS_PER_TILE ) {
								decal_tile.decals[ count.decal_count ] = x_instance;
								count.decal_count++;
							}
						}
						else if( type == DynamicType_Light ) {
							if( count.dlight_count < MAX_DLIGHTS_PER_TILE ) {
								dlight_tile.dlights[ count.dlight_count ] = x_instance - num_decals;
								count.dlight_count++;
							}
						}
						if( count.decal_count == MAX_DECALS_PER_TILE && count.dlight_count == MAX_DLIGHTS_PER_TILE ) {
							break;
						}
						x_idx++;
						y_idx++;
					} else if( x_instance > y_instance ) {
						// NOTE(msc): indices ordered high to low
						x_idx++;
					} else {
						y_idx++;
					}
				}
			}
		}

		FREE( sys_allocator, rows_coverage.ptr );
		FREE( sys_allocator, cols_coverage.ptr );
	}

	{
		ZoneScopedN( "Upload decals/dlights" );
		WriteGPUBuffer( decal_tiles_buffer, gpu_decal_tiles.span() );
		WriteGPUBuffer( decals_buffer, decals, num_decals * sizeof( Decal ) );
		WriteGPUBuffer( dlight_tiles_buffer, gpu_dlight_tiles.span() );
		WriteGPUBuffer( dlights_buffer, dlights, num_dlights * sizeof( DynamicLight ) );
		WriteGPUBuffer( dynamic_count, gpu_dynamic_counts.span() );
	}

	num_decals = 0;
	num_dlights = 0;
}

void AddDynamicsToPipeline( PipelineState * pipeline ) {
	pipeline->set_buffer( "b_DecalTiles", decal_tiles_buffer );
	pipeline->set_buffer( "b_Decals", decals_buffer );

	pipeline->set_buffer( "b_DynamicLightTiles", dlight_tiles_buffer );
	pipeline->set_buffer( "b_DynamicLights", dlights_buffer );

	pipeline->set_buffer( "b_DynamicTiles", dynamic_count );
}
