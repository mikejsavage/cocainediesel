#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

void ExplosionParticles( Vec3 origin, Vec3 normal, Vec3 team_color ) {
	EmitParticles( FindParticleEmitter( "explosion1" ), ParticleEmitterSphere( origin, normal, 180.0f, 48.0f ), 1.0f );
	EmitParticles( FindParticleEmitter( "explosion2" ), ParticleEmitterSphere( origin, normal, 180.0f ), 1.0f, Vec4( team_color, 1.0f ) );
	EmitParticles( FindParticleEmitter( "explosion3" ), ParticleEmitterSphere( origin, normal, 180.0f, 8.0f ), 1.0f, Vec4( team_color, 1.0f ) );
	EmitParticles( FindParticleEmitter( "explosion4" ), ParticleEmitterSphere( origin, normal, 180.0f, 48.0f ), 1.0f );
}

void PlasmaImpactParticles( Vec3 origin, Vec3 normal, Vec3 team_color ) {
	EmitParticles( FindParticleEmitter( "plasmaImpact"), ParticleEmitterSphere( origin, normal, 90.0f ), 8.0f, Vec4( team_color, 0.5f ) );
}

void BubbleImpactParticles( Vec3 origin, Vec3 team_color ) {
	EmitParticles( FindParticleEmitter( "bubbleImpact"), ParticleEmitterSphere( origin, Vec3( 0.0f, 0.0f, 1.0f ) ), 64.0f, Vec4( team_color, 1.0f ) );
}

void RailTrailParticles( Vec3 start, Vec3 end, Vec4 color ) {
	constexpr int max_ions = 256;
	float distance_between_particles = 4.0f;
	float len = Length( end - start );
	float count = Min2( len / distance_between_particles + 1.0f, float( max_ions ) );
	EmitParticles( FindParticleEmitter( "railtrail" ), ParticleEmitterLine( start, end ), count, color );
}

void DrawBeam( Vec3 start, Vec3 end, float width, Vec4 color, const Material * material ) {
	if( material == NULL )
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
	Mat4 VP = frame_static.P * frame_static.V;

	float start_w = ( VP * Vec4( start, 1.0f ) ).w;
	float start_width = Max2( width, 8.0f * start_w * pixel_scale );

	float end_w = ( VP * Vec4( end, 1.0f ) ).w;
	float end_width = Max2( width, 8.0f * end_w * pixel_scale );

	Vec3 positions[] = {
		start + start_width * beam_across * 0.5f,
		start - start_width * beam_across * 0.5f,
		end + end_width * beam_across * 0.5f,
		end - end_width * beam_across * 0.5f,
	};

	float texture_aspect_ratio = float( material->texture->width ) / float( material->texture->height );
	float beam_aspect_ratio = Length( end - start ) / width;
	float repetitions = beam_aspect_ratio / texture_aspect_ratio;

	Vec2 half_pixel = HalfPixelSize( material );
	Vec2 uvs[] = {
		Vec2( half_pixel.x, half_pixel.y ),
		Vec2( half_pixel.x, 1.0f - half_pixel.y ),
		Vec2( repetitions - half_pixel.x, half_pixel.y ),
		Vec2( repetitions - half_pixel.x, 1.0f - half_pixel.y ),
	};

	RGBA8 colors[] = {
		RGBA8( 255, 255, 255, 255 * width / start_width ),
		RGBA8( 255, 255, 255, 255 * width / start_width ),
		RGBA8( 255, 255, 255, 255 * width / end_width ),
		RGBA8( 255, 255, 255, 255 * width / end_width ),
	};

	u16 base_index = DynamicMeshBaseIndex();
	u16 indices[] = { 0, 1, 2, 1, 3, 2 };
	for( u16 & idx : indices ) {
		idx += base_index;
	}

	PipelineState pipeline = MaterialToPipelineState( material, color );
	pipeline.shader = &shaders.standard_vertexcolors;
	pipeline.blend_func = BlendFunc_Add;
	pipeline.set_uniform( "u_View", frame_static.view_uniforms );
	pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );

	DynamicMesh mesh = { };
	mesh.positions = positions;
	mesh.uvs = uvs;
	mesh.colors = colors;
	mesh.indices = indices;
	mesh.num_vertices = 4;
	mesh.num_indices = 6;

	DrawDynamicMesh( pipeline, mesh );
}

struct PersistentBeam {
	Vec3 start, end;
	float width;
	Vec4 color;
	const Material * material;

	s64 spawn_time;
	float duration;
	float start_fade_time;
};

static constexpr size_t MaxPersistentBeams = 256;
static PersistentBeam persistent_beams[ MaxPersistentBeams ];
static size_t num_persistent_beams;

void InitPersistentBeams() {
	num_persistent_beams = 0;
}

void AddPersistentBeam( Vec3 start, Vec3 end, float width, Vec4 color, const Material * material, float duration, float fade_time ) {
	if( num_persistent_beams == ARRAY_COUNT( persistent_beams ) )
		return;

	PersistentBeam & beam = persistent_beams[ num_persistent_beams ];
	num_persistent_beams++;

	beam.start = start;
	beam.end = end;
	beam.width = width;
	beam.color = color;
	beam.material = material;
	beam.spawn_time = cl.serverTime;
	beam.duration = duration;
	beam.start_fade_time = duration - fade_time;
}

void DrawPersistentBeams() {
	ZoneScoped;

	for( size_t i = 0; i < num_persistent_beams; i++ ) {
		PersistentBeam & beam = persistent_beams[ i ];
		float t = ( cl.serverTime - beam.spawn_time ) / 1000.0f;
		float alpha;
		if( beam.start_fade_time != beam.duration ) {
			alpha = 1.0f - Unlerp01( beam.start_fade_time, t, beam.duration );
		}
		else {
			alpha = t < beam.duration ? 1.0f : 0.0f;
		}

		if( alpha <= 0 ) {
			num_persistent_beams--;
			Swap2( &beam, &persistent_beams[ num_persistent_beams ] );
			i--;
			continue;
		}

		Vec4 color = beam.color;
		color.w *= alpha;
		DrawBeam( beam.start, beam.end, beam.width, color, beam.material );
	}
}
