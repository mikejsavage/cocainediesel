#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"
#include "qcommon/hashtable.h"
#include "qcommon/time.h"

#define OPTIMIZED_TRAILS

template< typename T, size_t N >
class RingBuffer {
	T buffer[ N ];
	size_t head;

public:
	size_t n;

	void clear() {
		head = 0;
		n = 0;
	}

	T & operator[]( size_t i ) {
		Assert( i < n );
		return buffer[ ( head + i ) % N ];
	}

	const T & operator[]( size_t i ) const {
		Assert( i < n );
		return buffer[ ( head + i ) % N ];
	}

	void push( T element ) {
		if( n == N ) {
			buffer[ head ] = element;
			head = ( head + 1 ) % N;
			return;
		}
		buffer[ ( head + n ) % N ] = element;
		n++;
	}

	void shift() {
		Assert( n > 0 );
		head = ( head + 1 ) % N;
		n--;
	}
};

struct TrailPoint {
	Vec3 p;
	Time t;
};

struct Trail {
	u64 unique_id;
	RingBuffer< TrailPoint, 512 > points;
	float width;
	Vec4 color;
	StringHash material;
	Time duration;
	float offset;
};

constexpr size_t MAX_TRAILS = 512;

static Trail trails[ MAX_TRAILS ];
static Hashtable< MAX_TRAILS * 2 > trails_hashtable;
static u32 num_trails;

void InitTrails() {
	num_trails = 0;
}

void DrawTrail( u64 unique_id, Vec3 point, float width, Vec4 color, StringHash material, Time duration ) {
	u64 idx = num_trails;
	if( !trails_hashtable.get( unique_id, &idx ) ) {
		if( num_trails == MAX_TRAILS ) {
			Com_Printf( S_COLOR_YELLOW "Warning: Too many trails\n" );
			return;
		}
		trails_hashtable.add( unique_id, idx );
		num_trails++;
		Trail &trail = trails[ idx ];
		trail.unique_id = unique_id;
		trail.width = width;
		trail.material = material;
		trail.duration = duration;
		trail.offset = 0.0f;

		trail.points.clear();
	}
	Trail &trail = trails[ idx ];
	trail.color = color;

	TrailPoint new_point;
	new_point.p = point;
	new_point.t = cls.game_time;

	float dist = 1.0f;
	if( trail.points.n > 0 ) {
		dist = Length( trail.points[ trail.points.n - 1 ].p - new_point.p );
	}

	if( dist < 0.001f ) { // "same" point
		trail.points[ trail.points.n - 1 ] = new_point;
	}
	else {

#ifdef OPTIMIZED_TRAILS
		constexpr float epsilon = 1.0f;
		if( trail.points.n > 3 ) {
			Vec3 a = trail.points[ trail.points.n - 3 ].p;
			Vec3 b = trail.points[ trail.points.n - 2 ].p;
			Vec3 c = trail.points[ trail.points.n - 1 ].p;
			float d = Length( Cross( b - c, a - c ) ) * 0.5f;
			if( d / dist < epsilon ) { // replace the last point with new one
				trail.points[ trail.points.n - 1 ] = new_point;
				return;
			}
		}
#endif
		trail.points.push( new_point );
	}
}

static bool UpdateTrail( Trail & trail ) {
	Time tail_time = cls.game_time - Min2( cls.game_time, trail.duration );
	for( size_t i = 0; i < trail.points.n; i++ ) {
		if( trail.points[ i ].t > tail_time ) {
			break;
		}

		if( i + 1 < trail.points.n ) {
			if( trail.points[ i + 1 ].t > tail_time ) {
				// move tail over
				float fract = Unlerp01( trail.points[ i ].t, tail_time, trail.points[ i + 1 ].t );
				trail.offset += fract * Length( trail.points[ i ].p - trail.points[ i + 1 ].p );
				trail.points[ i ].p = Lerp( trail.points[ i ].p, fract, trail.points[ i + 1 ].p );
				trail.points[ i ].t = Lerp( trail.points[ i ].t, fract, trail.points[ i + 1 ].t );
				break;
			}
			trail.offset += Length( trail.points[ i ].p - trail.points[ i + 1 ].p );
		}
		trail.points.shift();
	}

	return trail.points.n > 0;
}

static void DrawActualTrail( const Trail & trail ) {
	if( trail.points.n <= 1 ) {
		return;
	}

	struct TrailVertex {
		Vec3 position;
		Vec2 uv;
		RGBA8 color;
	};

	TempAllocator temp = cls.frame_arena.temp();
	Span< TrailVertex > vertices = AllocSpan< TrailVertex >( &temp, trail.points.n * 2 );
	Span< u16 > indices = AllocSpan< u16 >( &temp, ( trail.points.n - 1 ) * 6 );

	const Material * material = FindMaterial( trail.material );
	float texture_aspect_ratio = float( TextureWidth( material->texture ) ) / float( TextureHeight( material->texture ) );
	float distance = trail.offset / trail.width / texture_aspect_ratio;

	for( size_t i = 0; i < trail.points.n; i++ ) {
		TrailPoint a = trail.points[ i ];
		Vec3 dir;
		if( i < trail.points.n - 1 ) {
			Vec3 b = trail.points[ i + 1 ].p;
			dir = b - a.p;
		}
		else {
			Vec3 prev_a = trail.points[ i - 1 ].p;
			dir = a.p - prev_a;
		}
		float fract = Unlerp01( cls.game_time - trail.duration, a.t, cls.game_time );

		float len = Length( dir );
		dir /= len;
		Vec3 forward = SafeNormalize( a.p - frame_static.position );
		Vec3 bitangent = SafeNormalize( Cross( -forward, dir ) );

		vertices[ i * 2 + 0 ].position = a.p + bitangent * trail.width * 0.5f * fract;
		vertices[ i * 2 + 1 ].position = a.p - bitangent * trail.width * 0.5f * fract;

		float u = distance;

		vertices[ i * 2 + 0 ].uv = Vec2( u, 0.0f );
		vertices[ i * 2 + 1 ].uv = Vec2( u, 1.0f );

		float beam_aspect_ratio = len / trail.width;
		float repetitions = beam_aspect_ratio / texture_aspect_ratio;
		distance += repetitions;

		float alpha = fract;

		vertices[ i * 2 + 0 ].color = RGBA8( 255, 255, 255, 255 * alpha );
		vertices[ i * 2 + 1 ].color = RGBA8( 255, 255, 255, 255 * alpha );

		if( i < trail.points.n - 1 ) {
			indices[ i * 6 + 0 ] = i * 2 + 0;
			indices[ i * 6 + 1 ] = i * 2 + 1;
			indices[ i * 6 + 2 ] = i * 2 + 2;
			indices[ i * 6 + 3 ] = i * 2 + 1;
			indices[ i * 6 + 4 ] = i * 2 + 3;
			indices[ i * 6 + 5 ] = i * 2 + 2;
		}
	}

	PipelineState pipeline = {
		.shader = shaders.standard_vertexcolors_add,
	};

	Mesh mesh = { };
	mesh.vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, offsetof( TrailVertex, position ) };
	mesh.vertex_descriptor.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, offsetof( TrailVertex, uv ) };
	mesh.vertex_descriptor.attributes[ VertexAttribute_Color ] = VertexAttribute { VertexFormat_U8x4_01, 0, offsetof( TrailVertex, color ) };
	mesh.vertex_descriptor.buffer_strides[ 0 ] = sizeof( TrailVertex );
	mesh.num_vertices = indices.n;
	mesh.vertex_buffers[ 0 ] = NewTempBuffer( vertices );
	mesh.index_buffer = NewTempBuffer( indices );

	Draw( RenderPass_Transparent, pipeline, mesh, {
		{ "u_Model", frame_static.identity_model_transform_uniforms },
		{ "u_Color", NewTempBuffer( EvaluateMaterialColor( *material, trail.color ) ) },
	} );
}

void DrawTrails() {
	for( u32 i = 0; i < num_trails; i++ ) {
		if( !UpdateTrail( trails[ i ] ) ) {
			trails_hashtable.remove( trails[ i ].unique_id );
			if( num_trails > 1 ) {
				Swap2( &trails[ i ], &trails[ num_trails - 1 ] );
				trails_hashtable.update( trails[ i ].unique_id, i );
			}
			i--;
			num_trails--;
			continue;
		}
		DrawActualTrail( trails[ i ] );
	}
}
