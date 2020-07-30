#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

void ExplosionParticles( Vec3 origin, Vec3 normal, Vec3 team_color ) {
	{
		ParticleEmitter emitter = { };

		emitter.position = origin;
		emitter.position_distribution.type = RandomDistribution3DType_Sphere;
		emitter.position_distribution.sphere.radius = 48.0f;

		emitter.start_speed = 20.0f;
		emitter.end_speed = 50.0f;

		emitter.start_color = Vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		emitter.end_color = Vec3( 0.0f, 0.0f, 0.0f );

		emitter.start_size = 32.0f;
		emitter.end_size = 0.0f;
		emitter.size_distribution.type = RandomDistributionType_Uniform;
		emitter.size_distribution.uniform = 8.0f;

		emitter.lifetime = 0.25f;
		emitter.lifetime_distribution.type = RandomDistributionType_Uniform;
		emitter.lifetime_distribution.uniform = 0.2f;

		emitter.n = 64;

		EmitParticles( &cgs.smoke, emitter );
	}
	{
		ParticleEmitter emitter = { };

		emitter.position = origin;

		emitter.use_cone_direction = true;
		emitter.direction_cone.normal = normal;
		emitter.direction_cone.theta = 180.0f;

		emitter.start_speed = 1000.0f;
		emitter.end_speed = 0.0f;

		emitter.start_color = Vec4( team_color, 1.0f );
		emitter.end_color = Vec3( 0.0f, 0.0f, 0.0f );

		emitter.start_size = 12.0f;
		emitter.end_size = 0.0f;
		emitter.size_distribution.type = RandomDistributionType_Uniform;
		emitter.size_distribution.uniform = 8.0f;

		emitter.lifetime = 0.55f;
		emitter.lifetime_distribution.uniform = 0.25f;

		emitter.n = 64;

		EmitParticles( &cgs.fire, emitter );
	}
	{
		ParticleEmitter emitter = { };

		emitter.position = origin;
		emitter.position_distribution.type = RandomDistribution3DType_Sphere;
		emitter.position_distribution.sphere.radius = 8.0f;

		emitter.use_cone_direction = true;
		emitter.direction_cone.normal = normal;
		emitter.direction_cone.theta = 90.0f;

		emitter.start_speed = 0.0f;
		emitter.end_speed = 100.0f;

		emitter.start_color = Vec4( team_color, 1.0f );
		emitter.end_color = Vec3( 0.0f, 0.0f, 0.0f );

		emitter.start_size = 165.0f;
		emitter.end_size = 128.0f;
		emitter.size_distribution.type = RandomDistributionType_Uniform;
		emitter.size_distribution.uniform = 8.0f;

		emitter.lifetime = 0.25f;
		emitter.lifetime_distribution.uniform = 0.15f;

		emitter.n = 32;

		EmitParticles( &cgs.explosion, emitter );
	}
	{
		ParticleEmitter emitter = { };

		emitter.position = origin;
		emitter.position_distribution.type = RandomDistribution3DType_Sphere;
		emitter.position_distribution.sphere.radius = 48.0f;

		emitter.start_speed = 20.0f;
		emitter.end_speed = 100.0f;

		emitter.start_color = Vec4( 0.25f, 0.25f, 0.25f, 1.20f );
		emitter.end_color = Vec3( 0.0f, 0.0f, 0.0f );

		emitter.start_size = 32.0f;
		emitter.end_size = 0.0f;
		emitter.size_distribution.type = RandomDistributionType_Uniform;
		emitter.size_distribution.uniform = 8.0f;

		emitter.lifetime = 0.45f;
		emitter.lifetime_distribution.type = RandomDistributionType_Uniform;
		emitter.lifetime_distribution.uniform = 0.2f;

		emitter.n = 64;

		EmitParticles( &cgs.smoke2, emitter );
	}
}

void PlasmaImpactParticles( Vec3 origin, Vec3 normal, Vec3 team_color ) {
	ParticleEmitter emitter = { };
	emitter.position = origin;

	emitter.use_cone_direction = true;
	emitter.direction_cone.normal = normal;
	emitter.direction_cone.theta = 90.0f;

	emitter.start_speed = 125.0f;
	emitter.end_speed = 25.0f;

	emitter.start_color = Vec4( team_color, 0.5f );
	emitter.end_color = team_color;

	emitter.start_size = 32.0f;
	emitter.end_size = 32.0f;
	emitter.size_distribution.type = RandomDistributionType_Uniform;
	emitter.size_distribution.uniform = 8.0f;

	emitter.lifetime = 1.0f;
	emitter.lifetime_distribution.type = RandomDistributionType_Uniform;
	emitter.lifetime_distribution.uniform = 0.1f;

	emitter.n = 8;

	EmitParticles( &cgs.sparks, emitter );
}

void BubbleImpactParticles( Vec3 origin, Vec3 team_color ) {
	ParticleEmitter emitter = { };
	emitter.position = origin;

	emitter.start_speed = 200.0f;
	emitter.end_speed = 0.0f;

	emitter.start_color = Vec4( team_color, 1.0f );
	emitter.end_color = team_color;

	emitter.start_size = 1.5f;
	emitter.end_size = 0.0f;
	emitter.size_distribution.type = RandomDistributionType_Uniform;
	emitter.size_distribution.uniform = 1.5f;

	emitter.lifetime = 0.5f;
	emitter.lifetime_distribution.type = RandomDistributionType_Uniform;
	emitter.lifetime_distribution.uniform = 0.5f;

	emitter.red_distribution.uniform = 0.5f;
	emitter.blue_distribution.uniform = 0.5f;

	emitter.n = 64;

	EmitParticles( &cgs.sparks, emitter );
}

void RailTrailParticles( Vec3 start, Vec3 end, Vec4 color ) {
	ParticleEmitter emitter = { };
	emitter.position = start;
	emitter.position_distribution.type = RandomDistribution3DType_Line;
	emitter.position_distribution.line.end = end;

	emitter.start_speed = 4.0f;
	emitter.end_speed = 0.0f;

	emitter.start_color = color;
	emitter.end_color = color.xyz();

	RandomDistribution color_dist;
	color_dist.type = RandomDistributionType_Uniform;
	color_dist.uniform = 0.1f;
	emitter.red_distribution = color_dist;
	emitter.green_distribution = color_dist;
	emitter.blue_distribution = color_dist;
	emitter.alpha_distribution = color_dist;

	emitter.start_size = 1.0f;
	emitter.end_size = 1.0f;
	emitter.size_distribution.type = RandomDistributionType_Uniform;
	emitter.size_distribution.uniform = 0.1f;

	emitter.lifetime = 0.9f;
	emitter.lifetime_distribution.type = RandomDistributionType_Uniform;
	emitter.lifetime_distribution.uniform = 0.3f;

	constexpr int max_ions = 256;
	float distance_between_particles = 4.0f;

	float len = Length( end - start );

	emitter.n = Min2( len / distance_between_particles + 1.0f, float( max_ions ) );

	EmitParticles( &cgs.ions, emitter );
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
