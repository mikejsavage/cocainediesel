#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/time.h"
#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

void DrawBeam( Vec3 start, Vec3 end, float width, Vec4 color, StringHash material_name ) {
	if( start == end || start == frame_static.position )
		return;

	Vec3 dir = Normalize( end - start );
	Vec3 forward = Normalize( start - frame_static.position );

	if( Abs( Dot( dir, forward ) ) == 1.0f )
		return;

	Vec3 beam_across = Normalize( Cross( -forward, dir ) );

	// "phone wire anti-aliasing"
	// we should really do this in the shader so it's accurate across the whole beam.
	// scale width by 8 because our beam textures have their own fade to transparent at the edges.
	float pixel_scale = tanf( 0.5f * Radians( cg.view.fov_y ) ) / frame_static.viewport.y;
	Mat4 VP = frame_static.P * Mat4( frame_static.V );

	float start_w = ( VP * Vec4( start, 1.0f ) ).w;
	float start_width = Max2( width, 8.0f * start_w * pixel_scale );

	float end_w = ( VP * Vec4( end, 1.0f ) ).w;
	float end_width = Max2( width, 8.0f * end_w * pixel_scale );

	const Material * material = FindMaterial( material_name );
	float texture_aspect_ratio = float( material->texture->width ) / float( material->texture->height );
	float beam_aspect_ratio = Length( end - start ) / width;
	float repetitions = beam_aspect_ratio / texture_aspect_ratio;
	Vec2 half_pixel = HalfPixelSize( material );

	struct BeamVertex {
		Vec3 position;
		Vec2 uv;
		RGBA8 color;
	};

	BeamVertex vertices[] = {
		{
			.position = start + start_width * beam_across * 0.5f,
			.uv = Vec2( half_pixel.x, half_pixel.y ),
			.color = RGBA8( 255, 255, 255, 255 * width / start_width ),
		},
		{
			.position = start - start_width * beam_across * 0.5f,
			.uv = Vec2( half_pixel.x, 1.0f - half_pixel.y ),
			.color = RGBA8( 255, 255, 255, 255 * width / start_width ),
		},
		{
			.position = end + end_width * beam_across * 0.5f,
			.uv = Vec2( repetitions - half_pixel.x, half_pixel.y ),
			.color = RGBA8( 255, 255, 255, 255 * width / end_width ),
		},
		{
			.position = end - end_width * beam_across * 0.5f,
			.uv = Vec2( repetitions - half_pixel.x, 1.0f - half_pixel.y ),
			.color = RGBA8( 255, 255, 255, 255 * width / end_width ),
		},
	};

	constexpr u16 indices[] = { 0, 1, 2, 1, 3, 2 };

	PipelineState pipeline = MaterialToPipelineState( material, color );
	pipeline.shader = shaders.standard_vertexcolors;
	pipeline.blend_func = BlendFunc_Add;
	pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
	pipeline.bind_uniform( "u_Model", frame_static.identity_model_uniforms );

	VertexDescriptor vertex_descriptor = { };
	vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, offsetof( BeamVertex, position ) };
	vertex_descriptor.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, offsetof( BeamVertex, uv ) };
	vertex_descriptor.attributes[ VertexAttribute_Color ] = VertexAttribute { VertexFormat_U8x4_01, 0, offsetof( BeamVertex, color ) };
	vertex_descriptor.buffer_strides[ 0 ] = sizeof( BeamVertex );

	DrawDynamicGeometry( pipeline, StaticSpan( vertices ), StaticSpan( indices ), vertex_descriptor );
}

struct PersistentBeam {
	Vec3 start, end;
	float width;
	Vec4 color;
	StringHash material;

	Time spawn_time;
	Time duration;
	Time start_fade_time;
};

static BoundedDynamicArray< PersistentBeam, 256 > persistent_beams;

void InitPersistentBeams() {
	persistent_beams.clear();
}

void AddPersistentBeam( Vec3 start, Vec3 end, float width, Vec4 color, StringHash material, Time duration, Time fade_time ) {
	[[maybe_unused]] bool ok = persistent_beams.add( PersistentBeam {
		.start = start,
		.end = end,
		.width = width,
		.color = color,
		.material = material,
		.spawn_time = cls.game_time,
		.duration = duration,
		.start_fade_time = duration - fade_time,
	} );
}

void DrawPersistentBeams() {
	TracyZoneScoped;

	for( size_t i = 0; i < persistent_beams.size(); i++ ) {
		const PersistentBeam & beam = persistent_beams[ i ];
		Time t = cls.game_time - beam.spawn_time;
		float alpha;
		if( beam.start_fade_time != beam.duration ) {
			alpha = 1.0f - Unlerp01( beam.start_fade_time, t, beam.duration );
		}
		else {
			alpha = t < beam.duration ? 1.0f : 0.0f;
		}

		if( alpha <= 0 ) {
			persistent_beams.remove_swap( i );
			i--;
			continue;
		}

		Vec4 color = beam.color;
		color.w *= alpha;
		DrawBeam( beam.start, beam.end, beam.width, color, beam.material );
	}
}
