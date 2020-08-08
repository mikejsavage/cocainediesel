#include "qcommon/fs.h"
#include "qcommon/serialization.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "cgame/cg_local.h"

#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "gameshared/q_shared.h"

#include "imgui/imgui.h"

static ParticleSystem particleSystems[ MAX_PARTICLE_SYSTEMS ];
static u32 num_particleSystems;
static Hashtable< MAX_PARTICLE_SYSTEMS * 2 > particleSystems_hashtable;

static ParticleEmitter particleEmitters[ MAX_PARTICLE_EMITTERS ];
static u32 num_particleEmitters;
static Hashtable< MAX_PARTICLE_EMITTERS * 2 > particleEmitters_hashtable;

bool ParseParticleSystemEvents( Span< const char > * data, ParticleSystemEvents * event ) {
	while( true ) {
		Span< const char > opening_brace = ParseToken( data, Parse_DontStopOnNewLine );
		if( opening_brace == "" )
			break;

		if( opening_brace != "{" ) {
			Com_Printf( S_COLOR_YELLOW "Expected {\n" );
			return false;
		}

		while( true ) {
			Span< const char > key = ParseToken( data, Parse_DontStopOnNewLine );

			if( key == "}" ) {
				return true;
			}

			if( key == "" ) {
				Com_Printf( S_COLOR_YELLOW "Missing event\n" );
				return false;
			}

			ParticleSystemEvent e = { };
			if( key == "despawn" ) {
				e.type = ParticleSystemEventType_Despawn;
				event->events[ event->num_events ] = e;
				event->num_events++;
			}
			else if( key == "emitter" ) {
				e.type = ParticleSystemEventType_Emitter;
				Span< const char > value = ParseToken( data, Parse_DontStopOnNewLine );
				e.event_name = StringHash( Hash64( value.ptr, value.n ) );
				event->events[ event->num_events ] = e;
				event->num_events++;
			}
			else if( key == "decal" ) {
				e.type = ParticleSystemEventType_Decal;
				Span< const char > value = ParseToken( data, Parse_DontStopOnNewLine );
				e.event_name = StringHash( Hash64( value.ptr, value.n ) );
				event->events[ event->num_events ] = e;
				event->num_events++;
			}

		}
	}

	return true;
}

RandomDistribution ParseRandomDistribution( Span< const char > * data, ParseStopOnNewLine stop ) {
	RandomDistribution d = { };
	Span< const char > type = ParseToken( data, stop );
	if( type == "uniform" ) {
		d.type = RandomDistributionType_Uniform;
		d.uniform = ParseFloat( data, 0.0f, stop );
	}
	else if( type == "normal" ) {
		d.type = RandomDistributionType_Normal;
		d.sigma = ParseFloat( data, 0.0f, stop );
	}
	return d;
}

void DeleteParticleSystem( Allocator * a, ParticleSystem * ps );

void InitParticleSystem( Allocator * a, ParticleSystem * ps ) {
	DeleteParticleSystem( a, ps );

	ps->gradient = FindMaterial( "$whiteimage" );

	ps->particles = ALLOC_SPAN( a, GPUParticle, ps->max_particles );
	ps->gpu_instances = ALLOC_SPAN( a, u32, ps->max_particles );
	if( ps->feedback ) {
		ps->particles_feedback = ALLOC_SPAN( a, GPUParticleFeedback, ps->max_particles );
		memset( ps->particles_feedback.ptr, 0, ps->particles_feedback.num_bytes() );
		ps->vb_feedback = NewVertexBuffer( ps->particles_feedback.begin(), ps->max_particles * sizeof( GPUParticleFeedback ) );
	}
	else {
		ps->gpu_instances_time = ALLOC_SPAN( a, s64, ps->max_particles );
	}
	ps->ibo = NewIndexBuffer( ps->max_particles * sizeof( ps->gpu_instances[ 0 ] ) );
	ps->vb = NewParticleVertexBuffer( ps->max_particles );
	ps->vb2 = NewParticleVertexBuffer( ps->max_particles );

	if( ps->material ) {
		{
			constexpr Vec2 verts[] = {
				Vec2( -0.5f, -0.5f ),
				Vec2( 0.5f, -0.5f ),
				Vec2( -0.5f, 0.5f ),
				Vec2( 0.5f, 0.5f ),
			};

			Vec2 half_pixel = 0.5f / Vec2( ps->material->texture->width, ps->material->texture->height );
			Vec2 uvs[] = {
				half_pixel,
				Vec2( 1.0f - half_pixel.x, half_pixel.y ),
				Vec2( half_pixel.x, 1.0f - half_pixel.y ),
				1.0f - half_pixel,
			};

			constexpr u16 indices[] = { 0, 1, 2, 3 };

			MeshConfig mesh_config;
			mesh_config.positions = NewVertexBuffer( verts, sizeof( verts ) );
			mesh_config.positions_format = VertexFormat_Floatx2;
			mesh_config.tex_coords = NewVertexBuffer( uvs, sizeof( uvs ) );
			mesh_config.indices = NewIndexBuffer( indices, sizeof( indices ) );
			mesh_config.num_vertices = ARRAY_COUNT( indices );
			mesh_config.primitive_type = PrimitiveType_TriangleStrip;

			ps->mesh = NewMesh( mesh_config );
		}
	}
	
	{
		MeshConfig mesh_config = { };
		mesh_config.positions = NewVertexBuffer( NULL, 0 );
		mesh_config.indices = ps->ibo;
		mesh_config.indices_format = IndexFormat_U32;
		mesh_config.num_vertices = 1;
		mesh_config.primitive_type = PrimitiveType_Points;

		ps->update_mesh = NewMesh( mesh_config );
	}

	ps->initialized = true;
}

static bool ParseParticleSystem( ParticleSystem * system, Span< const char > name, Span< const char > * data ) {
	while( true ) {
		Span< const char > opening_brace = ParseToken( data, Parse_DontStopOnNewLine );
		if( opening_brace == "" )
			break;

		if( opening_brace != "{" ) {
			Com_Printf( S_COLOR_YELLOW "Expected {\n" );
			return false;
		}

		while( true ) {
			Span< const char > key = ParseToken( data, Parse_DontStopOnNewLine );

			if( key == "}" ) {
				return true;
			}

			if( key == "" ) {
				Com_Printf( S_COLOR_YELLOW "Missing key\n" );
				return false;
			}

			if( key == "material" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				system->material = FindMaterial( StringHash( Hash64( value.ptr, value.n ) ) );
			}
			else if( key == "model" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				system->model = FindModel( StringHash( Hash64( value.ptr, value.n ) ) );
			}
			else if( key == "max" ) {
				system->max_particles = ParseInt( data, 0, Parse_StopOnNewLine );
			}
			else if( key == "collision" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value == "point" ) {
					system->collision = ParticleCollisionType_Point;
				}
				else if( value == "sphere" ) {
					system->collision = ParticleCollisionType_Sphere;
				}
			}
			else if( key == "radius" ) {
				system->radius = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "blendfunc" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value == "blend" ) {
					system->blend_func = BlendFunc_Blend;
				}
				else if( value == "add" ) {
					system->blend_func = BlendFunc_Add;
				}
			}
			else if( key == "acceleration" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value == "$gravity" ) {
					system->acceleration = Vec3( 0.0f, 0.0f, -GRAVITY );
				} else {
					float x = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
					float y = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
					float z = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
					system->acceleration = Vec3( x, y, z );
				}
			}
			else if( key == "on_collision" ) {
				ParseParticleSystemEvents( data, &system->on_collision );
				system->feedback = true;
			}
			else if( key == "on_age" ) {
				ParseParticleSystemEvents( data, &system->on_age );
				system->feedback = true;
			}
		}
	}

	return true;
}

static bool ParseParticleEmitter( ParticleEmitter * emitter, Span< const char > name, Span< const char > * data ) {
	while( true ) {
		Span< const char > opening_brace = ParseToken( data, Parse_DontStopOnNewLine );
		if( opening_brace == "" )
			break;

		if( opening_brace != "{" ) {
			Com_Printf( S_COLOR_YELLOW "Expected {" );
			return false;
		}

		while( true ) {
			Span< const char > key = ParseToken( data, Parse_DontStopOnNewLine );

			if( key == "}" ) {
				return true;
			}

			if( key == "" ) {
				Com_Printf( S_COLOR_YELLOW "Missing key" );
				return false;
			}

			if( key == "system" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				emitter->pSystem = StringHash( Hash64( value.ptr, value.n ) );
			}
			else if( key == "speed" ) {
				emitter->speed = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "speed_distribution" ) {
				emitter->speed_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "size" ) {
				emitter->start_size = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				emitter->end_size = ParseFloat( data, emitter->start_size, Parse_StopOnNewLine );
			}
			else if( key == "size_distribution" ) {
				emitter->size_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "lifetime" ) {
				emitter->lifetime = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "lifetime_distribution" ) {
				emitter->lifetime_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "color" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value[ 0 ] == '$' ) {
					// TODO variables
				}
				else {
					float r = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
					float g = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
					float b = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
					float a = ParseFloat( data, 1.0f, Parse_StopOnNewLine );
					emitter->start_color = Vec4( r, g, b, a );
					emitter->end_color = Vec4( r, g, b, 0.0f );
				}
			}
			else if( key == "end_color" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value[ 0 ] == '$' ) {
					// TODO variables
				}
				else {
					float r = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
					float g = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
					float b = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
					emitter->end_color = Vec4( r, g, b, 0.0f );
				}
			}
			else if( key == "red_distribution" ) {
				emitter->red_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "green_distribution" ) {
				emitter->green_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "blue_distribution" ) {
				emitter->blue_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "alpha_distribution" ) {
				emitter->alpha_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "count" ) {
				emitter->count = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "emission" ) {
				emitter->emission = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
		}
	}

	return true;
}

static void LoadParticleFile( const char * path ) {
	Span< const char > data = AssetString( path );

	while( data.ptr < data.end() ) {
		Span< const char > type = ParseToken( &data, Parse_DontStopOnNewLine );
		Span< const char > name = ParseToken( &data, Parse_StopOnNewLine );
		u64 hash = Hash64( name );

		if( type == "system" ) {
			ParticleSystem pSystem = { };
			if( !ParseParticleSystem( &pSystem, name, &data ) ) {
				Com_Printf( S_COLOR_YELLOW "Couldn't load %s\n", path );
				return;
			}
			u64 idx = num_particleSystems;
			if( !particleSystems_hashtable.get( hash, &idx ) ) {
				particleSystems_hashtable.add( hash, idx );
				num_particleSystems++;
			}
			InitParticleSystem( sys_allocator, &pSystem );
			particleSystems[ idx ] = pSystem;
		}
		else if( type == "emitter" ) {
			ParticleEmitter particleEmitter = { };
			if( !ParseParticleEmitter( &particleEmitter, name, &data ) ) {
				Com_Printf( S_COLOR_YELLOW "Couldn't load %s\n", path );
				return;
			}
			u64 idx = num_particleEmitters;
			if( !particleEmitters_hashtable.get( hash, &idx ) ) {
				particleEmitters_hashtable.add( hash, idx );
				num_particleEmitters++;
			}
			particleEmitters[ idx ] = particleEmitter;
		}
	}
}

void InitParticles() {
	ZoneScoped;

	for( const char * path : AssetPaths() ) {
		if( FileExtension( path ) == ".cdvfx" ) {
			LoadParticleFile( path );
		}
	}
}

void HotloadParticles() {
	ZoneScoped;

	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".cdvfx" ) {
			LoadParticleFile( path );
		}
	}
}

ParticleSystem * FindParticleSystem( StringHash name ) {
	u64 idx;
	if( !particleSystems_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &particleSystems[ idx ];
}

ParticleSystem * FindParticleSystem( const char * name ) {
	return FindParticleSystem( StringHash( name ) );
}

ParticleEmitter * FindParticleEmitter( StringHash name ) {
	u64 idx;
	if( !particleEmitters_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &particleEmitters[ idx ];
}

ParticleEmitter * FindParticleEmitter( const char * name ) {
	return FindParticleEmitter( StringHash( name ) );
}

void DeleteParticleSystem( Allocator * a, ParticleSystem * ps ) {
	if( !ps->initialized ) {
		return;
	}
	FREE( a, ps->particles.ptr );
	FREE( a, ps->particles_feedback.ptr );
	FREE( a, ps->gpu_instances.ptr );
	FREE( a, ps->gpu_instances_time.ptr );
	DeleteIndexBuffer( ps->ibo );
	DeleteVertexBuffer( ps->vb );
	DeleteVertexBuffer( ps->vb2 );
	DeleteVertexBuffer( ps->vb_feedback );

	DeleteMesh( ps->mesh );
	DeleteMesh( ps->update_mesh );

	ps->initialized = false;
}

void ShutdownParticles() {
	for( size_t i = 0; i < num_particleSystems; i++ ) {
		DeleteParticleSystem( sys_allocator, &particleSystems[ i ] );
	}
}

static float EvaluateEasingDerivative( EasingFunction func, float t ) {
	switch( func ) {
		case EasingFunction_Linear: return 1.0f;
		case EasingFunction_Quadratic: return t < 0.5f ? 4.0f * t : -4.0f * t + 4.0f;
		case EasingFunction_QuadraticEaseIn: return 2.0f * t;
		case EasingFunction_QuadraticEaseOut: return -2.0f * t + 2.0f;
	}

	return 0.0f;
}

#define FEEDBACK_NONE 0
#define FEEDBACK_AGE 1
#define FEEDBACK_COLLISION 2

bool ParticleFeedback( ParticleSystem * ps, GPUParticleFeedback * feedback ) {
	StringHash despawn = StringHash( "despawn" );
	bool result = true;

	if( feedback->parm & FEEDBACK_COLLISION ) {
		for( u8 i = 0; i < ps->on_collision.num_events; i++ ) {
			ParticleSystemEvent event = ps->on_collision.events[ i ];
			if( event.type == ParticleSystemEventType_Despawn ) {
				result = false;
			}
			else if( event.type == ParticleSystemEventType_Emitter ) {
				EmitParticles( FindParticleEmitter( event.event_name ), ParticleEmitterSphere( feedback->position, feedback->normal, 90.0f ), 1.0f );
			}
			else if( event.type == ParticleSystemEventType_Decal ) {
				AddPersistentDecal( feedback->position, feedback->normal, 64.0f, 0.0f, event.event_name, Vec4( 1.0f ), 5000 );
			}
		}
	}

	if( feedback->parm & FEEDBACK_AGE ) {
		result = false;
		for( u8 i = 0; i < ps->on_age.num_events; i++ ) {
			ParticleSystemEvent event = ps->on_age.events[ i ];
			if( event.type == ParticleSystemEventType_Despawn ) {
				result = false;
			}
			else if( event.type == ParticleSystemEventType_Emitter ) {
				EmitParticles( FindParticleEmitter( event.event_name ), ParticleEmitterSphere( feedback->position ), 1.0f );
			}
			else if( event.type == ParticleSystemEventType_Decal ) {
				AddPersistentDecal( feedback->position, feedback->normal, 64.0f, 0.0f, event.event_name, Vec4( 1.0f ), 5000 );
			}
		}
	}
	return result;
};

void UpdateParticleSystem( ParticleSystem * ps, float dt ) {
	ZoneScopedN( "Update particles" );
	
	{
		ZoneScopedN( "Despawn expired particles" );

		if( ps->feedback ) {
			ReadVertexBuffer( ps->vb_feedback, ps->particles_feedback.begin(), ps->num_particles * sizeof( GPUParticleFeedback ) );

			for( size_t i = 0; i < ps->num_particles; i++ ) {
				size_t index = ps->gpu_instances[ i ];
				GPUParticleFeedback feedback = ps->particles_feedback[ index ];
				if ( !ParticleFeedback( ps, &feedback ) ) {
					ps->num_particles--;
					Swap2( &ps->gpu_instances[ index ], &ps->gpu_instances[ ps->num_particles ] );
					i--;
				}
			}
		} else {
			for( size_t i = 0; i < ps->num_particles; i++ ) {
				if ( ps->gpu_instances_time[ i ] < cls.monotonicTime ) {
					ps->num_particles--;
					Swap2( &ps->gpu_instances[ i ], &ps->gpu_instances[ ps->num_particles ] );
					Swap2( &ps->gpu_instances_time[ i ], &ps->gpu_instances_time[ ps->num_particles ] );
					i--;
				}
			}
		}
	}

	{
		ZoneScopedN( "Spawn new particles" );
		if( ps->new_particles > 0 ) {
			WriteVertexBuffer( ps->vb, ps->particles.begin(), ps->new_particles * sizeof( GPUParticle ), ps->num_particles * sizeof( GPUParticle ) );
			for( size_t i = 0; i < ps->new_particles; i++ ) {
				ps->gpu_instances[ ps->num_particles + i ] = ps->num_particles + i;
				if( !ps->feedback ) {
					ps->gpu_instances_time[ ps->num_particles + i ] = cls.monotonicTime + ps->particles[ i ].lifetime * 1000.0f;
				}
			}
		}
	}

	{
		ZoneScopedN( "Upload index buffer" );
		WriteIndexBuffer( ps->ibo, ps->gpu_instances.begin(), ( ps->num_particles + ps->new_particles ) * sizeof( ps->gpu_instances[ 0 ] ) );
	}

	{
		ZoneScopedN( "Reset order" );
		for( size_t i = 0; i < ps->num_particles; i++ ) {
			ps->gpu_instances[ i ] = i;
		}
	}

	ps->num_particles += ps->new_particles;
	ps->new_particles = 0;
}

void DrawParticleSystem( ParticleSystem * ps ) {
	DisableFPEScoped;

	if( ps->num_particles == 0 )
		return;

	ZoneScoped;
	float dt = cls.frametime / 1000.0f;

	if( ps->feedback ) {
		UpdateParticlesFeedback( ps->update_mesh, ps->vb, ps->vb2, ps->vb_feedback, ps->collision, ps->radius, ps->acceleration, ps->num_particles, dt );
	}
	else {
		UpdateParticles( ps->update_mesh, ps->vb, ps->vb2, ps->collision, ps->radius, ps->acceleration, ps->num_particles, dt );
	}
	
	if( ps->material ) {
		DrawInstancedParticles( ps->mesh, ps->vb2, ps->material, ps->gradient, ps->blend_func, ps->num_particles );
	}
	else if( ps->model ) {
		DrawInstancedParticles( ps->vb2, ps->model, ps->gradient, ps->num_particles );
	}

	Swap2( &ps->vb, &ps->vb2 );
}

void DrawParticles() {
	float dt = cls.frametime / 1000.0f;

	for( size_t i = 0; i < num_particleSystems; i++ ) {
		if( particleSystems[ i ].initialized ) {
			UpdateParticleSystem( &particleSystems[ i ], dt );
			DrawParticleSystem( &particleSystems[ i ] );
		}
	}

	if( cg_particleDebug->integer ) {
		const ImGuiIO & io = ImGui::GetIO();
		float width_frac = Lerp( 0.25f, Unlerp01( 1024.0f, io.DisplaySize.x, 1920.0f ), 0.15f );
		Vec2 size = io.DisplaySize * Vec2( width_frac, 0.5f );
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs;


		ImGui::SetNextWindowSize( ImVec2( size.x, size.y ) );
		ImGui::SetNextWindowPos( ImVec2( io.DisplaySize.x - size.x, 100.0f ), ImGuiCond_Always );
		ImGui::Begin( "particle statistics", WindowZOrder_Chat, flags );

		for( size_t i = 0; i < num_particleSystems; i++ ) {
			ParticleSystem * ps = &particleSystems[ i ];
			if( ps->initialized ) {
				ImGui::Text( "ps: %i, num: %i / %i", i, ps->num_particles, ps->max_particles );
			}
		}

		ImGui::End();
	}
}

static void EmitParticle( ParticleSystem * ps, float lifetime, Vec3 position, Vec3 velocity, Vec4 start_color, Vec4 end_color, float start_size, float end_size ) {
	ZoneScopedN( "Store Particle" );
	if( ps->num_particles + ps->new_particles == ps->max_particles )
		return;

	GPUParticle & particle = ps->particles[ ps->new_particles ];
	particle.position = position;
	particle.velocity = velocity;
	particle.orientation = UniformSampleSphere( &cls.rng );
	particle.avelocity = UniformSampleSphere( &cls.rng );
	particle.start_color = LinearTosRGB( start_color );
	particle.end_color = LinearTosRGB( end_color );
	particle.start_size = start_size;
	particle.end_size = end_size;
	particle.age = 0.0f;
	particle.lifetime = lifetime;

	ps->new_particles++;
}

static float SampleRandomDistribution( RNG * rng, RandomDistribution dist ) {
	if( dist.type == RandomDistributionType_Uniform ) {
		return random_float11( rng ) * dist.uniform;
	}

	return SampleNormalDistribution( rng ) * dist.sigma;
}

static void EmitParticle( ParticleSystem * ps, const ParticleEmitter * emitter, ParticleEmitterPosition pos, float t, Vec4 start_color, Vec4 end_color, Mat4 dir_transform ) {
	ZoneScopedN( "Emit Particle" );
	float lifetime = Max2( 0.0f, emitter->lifetime + SampleRandomDistribution( &cls.rng, emitter->lifetime_distribution ) );

	float size = Max2( 0.0f, emitter->start_size + SampleRandomDistribution( &cls.rng, emitter->size_distribution ) );

	Vec3 position = pos.origin + size * ps->radius * pos.normal;

	switch( pos.type ) {
		case ParticleEmitterPosition_Sphere: {
			position += UniformSampleInsideSphere( &cls.rng ) * pos.radius;
		} break;

		case ParticleEmitterPosition_Disk: {
			Vec2 p = UniformSampleDisk( &cls.rng );
			position += pos.radius * Vec3( p, 0.0f );
			// TODO: pos.normal;
		} break;

		case ParticleEmitterPosition_Line: {
			position = Lerp( position, random_float01( &cls.rng ), pos.end );
			position += ( dir_transform * Vec4( UniformSampleDisk( &cls.rng ) * pos.radius, 0.0f, 0.0f ) ).xyz();
		} break;
	}

	Vec3 dir;

	if( pos.theta != 0.0f ) {
		dir = ( dir_transform * Vec4( UniformSampleCone( &cls.rng, Radians( pos.theta ) ), 0.0f ) ).xyz();
	}
	else {
		dir = UniformSampleSphere( &cls.rng );
	}

	float speed = emitter->speed + SampleRandomDistribution( &cls.rng, emitter->speed_distribution );

	start_color.x += SampleRandomDistribution( &cls.rng, emitter->red_distribution );
	start_color.y += SampleRandomDistribution( &cls.rng, emitter->green_distribution );
	start_color.z += SampleRandomDistribution( &cls.rng, emitter->blue_distribution );
	start_color.w += SampleRandomDistribution( &cls.rng, emitter->alpha_distribution );
	start_color = Clamp01( start_color );

	EmitParticle( ps, lifetime, position, dir * speed, start_color, end_color, size, emitter->end_size );
}

void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count, Vec4 start_color, Vec4 end_color ) {
	ZoneScoped;

	float dt = cls.frametime / 1000.0f;
	ParticleSystem * ps = FindParticleSystem( emitter->pSystem );
	if( ps == NULL ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Particle emitter doesn't have a system\n" );
		return;
	}

	float p = emitter->count * count + emitter->emission * count * dt;
	u32 n = u32( p );
	float remaining_p = p - n;

	Mat4 dir_transform = Mat4::Identity();
	if( pos.theta != 0.0f ) {
		dir_transform = TransformKToDir( pos.normal );
	}
	else if( pos.type == ParticleEmitterPosition_Line && pos.radius > 0.0f ) {
		dir_transform = TransformKToDir( Normalize( pos.end - pos.origin ) );
	}

	for( u32 i = 0; i < n; i++ ) {
		float t = float( i ) / ( p + 1.0f );
		EmitParticle( ps, emitter, pos, t, start_color, end_color, dir_transform );
	}

	if( random_p( &cls.rng, remaining_p ) ) {
		EmitParticle( ps, emitter, pos, p / ( p + 1.0f ), start_color, end_color, dir_transform );
	}
}

void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count, Vec4 start_color ) {
	EmitParticles( emitter, pos, count, start_color, start_color );
}

void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count ) {
	EmitParticles( emitter, pos, count, emitter->start_color, emitter->end_color );
}

ParticleEmitterPosition ParticleEmitterSphere( Vec3 origin, Vec3 normal, float theta, float radius ) {
	ParticleEmitterPosition pos = { };
	pos.type = ParticleEmitterPosition_Sphere;
	pos.origin = origin;
	pos.normal = normal;
	pos.theta = theta;
	pos.radius = radius;
	return pos;
}

ParticleEmitterPosition ParticleEmitterSphere( Vec3 origin, float radius ) {
	ParticleEmitterPosition pos = { };
	pos.type = ParticleEmitterPosition_Sphere;
	pos.origin = origin;
	pos.normal = Vec3( 0.0f, 0.0f, 1.0f );
	pos.theta = 180.0f;
	pos.radius = radius;
	return pos;
}

ParticleEmitterPosition ParticleEmitterDisk( Vec3 origin, Vec3 normal, float radius ) {
	ParticleEmitterPosition pos = { };
	pos.type = ParticleEmitterPosition_Disk;
	pos.origin = origin;
	pos.normal = normal;
	pos.radius = radius;
	return pos;
}

ParticleEmitterPosition ParticleEmitterLine( Vec3 origin, Vec3 end, float radius ) {
	ParticleEmitterPosition pos = { };
	pos.type = ParticleEmitterPosition_Line;
	pos.origin = origin;
	pos.end = end;
	pos.radius = radius;
	return pos;
}

// enum ParticleEmitterVersion : u32 {
// 	ParticleEmitterVersion_First,
// };

// static void Serialize( SerializationBuffer * buf, SphereDistribution & sphere ) { *buf & sphere.radius; }
// static void Serialize( SerializationBuffer * buf, ConeDistribution & cone ) { *buf & cone.normal & cone.theta; }
// static void Serialize( SerializationBuffer * buf, DiskDistribution & disk ) { *buf & disk.normal & disk.radius; }
// static void Serialize( SerializationBuffer * buf, LineDistribution & line ) { *buf & line.end; }

// static void Serialize( SerializationBuffer * buf, RandomDistribution & dist ) {
// 	*buf & dist.type;
// 	if( dist.type == RandomDistributionType_Uniform )
// 		*buf & dist.uniform;
// 	else
// 		*buf & dist.sigma;
// }

// static void Serialize( SerializationBuffer * buf, RandomDistribution3D & dist ) {
// 	*buf & dist.type;
// 	if( dist.type == RandomDistribution3DType_Sphere )
// 		*buf & dist.sphere;
// 	else if( dist.type == RandomDistribution3DType_Disk )
// 		*buf & dist.disk;
// 	else
// 		*buf & dist.line;
// }

// static void Serialize( SerializationBuffer * buf, ParticleEmitter & emitter ) {
// 	u32 version = ParticleEmitterVersion_First;
// 	*buf & version;

// 	*buf & emitter.position & emitter.position_distribution;
// 	*buf & emitter.use_cone_direction;

// 	if( emitter.use_cone_direction ) {
// 		*buf & emitter.direction_cone;
// 	}

// 	*buf & emitter.start_color & emitter.end_color & emitter.red_distribution & emitter.green_distribution & emitter.blue_distribution & emitter.alpha_distribution;

// 	*buf & emitter.start_size & emitter.end_size & emitter.size_distribution;

// 	*buf & emitter.lifetime & emitter.lifetime_distribution;

// 	*buf & emitter.emission_rate & emitter.n;
// }

/*
 * particle editor
 */

// static ParticleSystem menu_ps = { };
// static ParticleEmitter menu_emitter;
// static char menu_material_name[ 256 ];
// static char menu_gradient_name[ 256 ];
// static bool menu_one_shot;
// static bool menu_blend;


// void InitParticleMenuEffect() {
// 	strcpy( menu_material_name, "$particle" );
// 	strcpy( menu_gradient_name, "$whiteimage" );
// 	menu_one_shot = false;
// 	menu_blend = false;

// 	menu_ps = NewParticleSystem( sys_allocator, 8192, FindMaterial( StringHash( ( const char * ) menu_material_name ) ) );
// 	menu_ps.gradient = FindMaterial( StringHash( ( const char * ) menu_gradient_name ) );
// 	menu_ps.blend_func = menu_blend ? BlendFunc_Blend : BlendFunc_Add;
// 	menu_emitter = { };

// 	menu_emitter.position_distribution.sphere.radius = 600.0f;

// 	menu_emitter.start_speed = 1.0f;
// 	menu_emitter.end_speed = 25.0f;
// 	menu_emitter.direction_cone.normal = Vec3( 0, 0, 1 );
// 	menu_emitter.direction_cone.theta = 90.0f;
// 	menu_emitter.start_color = vec4_black;
// 	menu_emitter.start_color.w = 1.0f;
// 	menu_emitter.end_color = vec4_black.xyz();

// 	menu_emitter.red_distribution.uniform = 0.325f;
// 	menu_emitter.green_distribution.uniform = 0.325f;
// 	menu_emitter.blue_distribution.uniform = 0.325f;


// 	menu_emitter.start_size = 0.0f;
// 	menu_emitter.end_size = 20.0f;
// 	menu_emitter.lifetime = 10.0f;
// 	menu_emitter.emission_rate = 500.0f;
// }

// void ShutdownParticleEditor() {
// 	DeleteParticleSystem( sys_allocator, menu_ps );
// }

// void ResetParticleMenuEffect() {
// 	DeleteParticleSystem( sys_allocator, menu_ps );
// 	InitParticleMenuEffect();
// }

// void ResetParticleEditor() {
// 	DeleteParticleSystem( sys_allocator, menu_ps );

// 	strcpy( menu_material_name, "$particle" );
// 	strcpy( menu_gradient_name, "$whiteimage" );
// 	menu_one_shot = false;
// 	menu_blend = false;

// 	menu_ps = NewParticleSystem( sys_allocator, 8192, FindMaterial( StringHash( ( const char * ) menu_material_name ) ) );
// 	menu_ps.gradient = FindMaterial( StringHash( ( const char * ) menu_gradient_name ) );
// 	menu_ps.blend_func = menu_blend ? BlendFunc_Blend : BlendFunc_Add;

// 	menu_emitter = { };

// 	menu_emitter.start_speed = 400.0f;
// 	menu_emitter.end_speed = 400.0f;
// 	menu_emitter.direction_cone.normal = Vec3( 0, 0, 1 );
// 	menu_emitter.direction_cone.theta = 90.0f;
// 	menu_emitter.start_color = vec4_white;
// 	menu_emitter.end_color = vec4_white.xyz();
// 	menu_emitter.start_size = 16.0f;
// 	menu_emitter.end_size = 16.0f;
// 	menu_emitter.lifetime = 1.0f;
// 	menu_emitter.emission_rate = 500;
// }

// static void RandomDistributionEditor( const char * id, RandomDistribution * dist, float range ) {
// 	constexpr const char * names[] = { "Uniform", "Normal" };

// 	TempAllocator temp = cls.frame_arena.temp();

// 	if( ImGui::BeginCombo( temp( "Distribution##{}", id ), names[ dist->type ] ) ) {
// 		for( int i = 0; i < 2; i++ ) {
// 			if( ImGui::Selectable( names[ i ], i == dist->type ) )
// 				dist->type = RandomDistributionType( i );
// 			if( i == dist->type )
// 				ImGui::SetItemDefaultFocus();
// 		}

// 		ImGui::EndCombo();
// 	}

// 	switch( dist->type ) {
// 		case RandomDistributionType_Uniform:
// 			ImGui::SliderFloat( temp( "Range##{}", id ), &dist->uniform, 0, range, "%.2f" );
// 			break;

// 		case RandomDistributionType_Normal:
// 			ImGui::SliderFloat( temp( "Stddev##{}", id ), &dist->sigma, 0, 8, "%.2f" );
// 			break;
// 	}
// }

// void DrawParticleMenuEffect() {
// 	RendererSetView( Vec3( -400, 0, 400 ), EulerDegrees3( 45, 0, 0 ), 90 );

// 	float dt = cls.frametime / 1000.0f;

// 	EmitParticles( &menu_ps, menu_emitter, dt );
// 	UpdateParticleSystem( &menu_ps, dt );
// 	DrawParticleSystem( &menu_ps );
// }

// void DrawParticleEditor() {
// 	TempAllocator temp = cls.frame_arena.temp();

// 	bool emit = false;

// 	ImGui::PushFont( cls.console_font );
// 	ImGui::BeginChild( "Particle editor", ImVec2( 300, 0 ) );
// 	{
// 		ImGuiWindowFlags popup_flags = ( ImGuiWindowFlags_NoDecoration & ~ImGuiWindowFlags_NoTitleBar ) | ImGuiWindowFlags_NoMove;

// 		if( ImGui::Button( "Load..." ) ) {
// 			ImGui::OpenPopup( "Load" );
// 		}

// 		ImGui::SameLine();

// 		if( ImGui::Button( "Save..." ) ) {
// 			ImGui::OpenPopup( "Save" );
// 		}

// 		if( ImGui::BeginPopupModal( "Load", NULL, popup_flags ) ) {
// 			static char name[ 64 ];
// 			ImGui::PushItemWidth( 300 );
// 			if( ImGui::IsWindowAppearing() ) {
// 				ImGui::SetKeyboardFocusHere();
// 				strcpy( name, "" );
// 			}
// 			bool do_load = ImGui::InputText( "##loadpath", name, sizeof( name ), ImGuiInputTextFlags_EnterReturnsTrue );
// 			ImGui::PopItemWidth();
// 			do_load = ImGui::Button( "Load" ) || do_load;

// 			if( do_load ) {
// 				Span< const char > data = AssetBinary( temp( "particles/{}.emitter", name ) ).cast< const char >();
// 				if( data.ptr != NULL ) {
// 					bool ok = Deserialize( menu_emitter, data.ptr, data.n );
// 					assert( ok );
// 				}

// 				ImGui::CloseCurrentPopup();
// 			}

// 			ImGui::SameLine();

// 			if( ImGui::Button( "Cancel" ) )
// 				ImGui::CloseCurrentPopup();

// 			ImGui::EndPopup();
// 		}

// 		if( ImGui::BeginPopupModal( "Save", NULL, popup_flags ) ) {
// 			static char name[ 64 ];
// 			ImGui::PushItemWidth( 300 );
// 			if( ImGui::IsWindowAppearing() ) {
// 				ImGui::SetKeyboardFocusHere();
// 				strcpy( name, "" );
// 			}
// 			bool ok = ImGui::InputText( "##savepath", name, sizeof( name ), ImGuiInputTextFlags_EnterReturnsTrue );
// 			ImGui::PopItemWidth();
// 			ok = ImGui::Button( "Save" ) || ok;

// 			if( ok ) {
// 				char buf[ 1024 ];
// 				SerializationBuffer sb( SerializationMode_Serializing, buf, sizeof( buf ) );
// 				sb & menu_emitter;
// 				assert( !sb.error );
// 				// TODO: writefile can fail
// 				WriteFile( temp( "base/particles/{}.emitter", name ), buf, sb.cursor - buf );
// 				HotloadAssets( &temp );

// 				ImGui::CloseCurrentPopup();
// 			}

// 			ImGui::SameLine();

// 			if( ImGui::Button( "Cancel" ) )
// 				ImGui::CloseCurrentPopup();

// 			ImGui::EndPopup();
// 		}

// 		ImGui::Separator();

// 		if( ImGui::InputText( "Material", menu_material_name, sizeof( menu_material_name ) ) ) {
// 			ResetParticleEditor();
// 		}

// 		if( ImGui::InputText( "Gradient material", menu_gradient_name, sizeof( menu_gradient_name ) ) ) {
// 			ResetParticleEditor();
// 		}

// 		ImGui::Checkbox( "Blend", &menu_blend );
// 		menu_ps.blend_func = menu_blend ? BlendFunc_Blend : BlendFunc_Add;

// 		ImGui::Separator();

// 		constexpr const char * position_distribution_names[] = { "Sphere", "Disk", "Line" };
// 		if( ImGui::BeginCombo( "Position distribution", position_distribution_names[ menu_emitter.position_distribution.type ] ) ) {
// 			for( int i = 0; i < 3; i++ ) {
// 				if( ImGui::Selectable( position_distribution_names[ i ], i == menu_emitter.position_distribution.type ) )
// 					menu_emitter.position_distribution.type = RandomDistribution3DType( i );
// 				if( i == menu_emitter.position_distribution.type )
// 					ImGui::SetItemDefaultFocus();
// 			}

// 			ImGui::EndCombo();
// 		}

// 		menu_emitter.position = Vec3( 0 );

// 		switch( menu_emitter.position_distribution.type ) {
// 			case RandomDistribution3DType_Sphere:
// 				ImGui::SliderFloat( "Radius", &menu_emitter.position_distribution.sphere.radius, 0, 100, "%.2f" );
// 				break;

// 			case RandomDistribution3DType_Disk:
// 				ImGui::SliderFloat( "Radius", &menu_emitter.position_distribution.disk.radius, 0, 100, "%.2f" );
// 				break;

// 			case RandomDistribution3DType_Line:
// 				menu_emitter.position = Vec3( 0, -300, 0 );
// 				menu_emitter.position_distribution.line.end = Vec3( 0, 300, 0 );
// 				break;
// 		}

// 		ImGui::Separator();

// 		ImGui::Checkbox( "Direction cone?", &menu_emitter.use_cone_direction );

// 		if( menu_emitter.use_cone_direction ) {
// 			ImGui::SliderFloat( "Angle", &menu_emitter.direction_cone.theta, 0, 180, "%.2f" );
// 		}

// 		ImGui::SliderFloat( "Start speed", &menu_emitter.start_speed, 0, 1000, "%.2f" );
// 		ImGui::SliderFloat( "End speed", &menu_emitter.end_speed, 0, 1000, "%.2f" );

// 		ImGui::Separator();

// 		ImGui::ColorEdit4( "Start color", menu_emitter.start_color.ptr() );
// 		ImGui::ColorEdit3( "End color", menu_emitter.end_color.ptr() );

// 		if( ImGui::TreeNodeEx( "Start color randomness", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoAutoOpenOnLog ) ) {
// 			ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 0, 0, 255 ) );
// 			RandomDistributionEditor( "r", &menu_emitter.red_distribution, 1.0f );
// 			ImGui::PopStyleColor();

// 			ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 255, 0, 255 ) );
// 			RandomDistributionEditor( "g", &menu_emitter.green_distribution, 1.0f );
// 			ImGui::PopStyleColor();

// 			ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 255, 255 ) );
// 			RandomDistributionEditor( "b", &menu_emitter.blue_distribution, 1.0f );
// 			ImGui::PopStyleColor();

// 			RandomDistributionEditor( "a", &menu_emitter.alpha_distribution, 1.0f );

// 			ImGui::TreePop();
// 		}

// 		ImGui::Separator();

// 		ImGui::SliderFloat( "Start size", &menu_emitter.start_size, 0, 256, "%.2f" );
// 		ImGui::SliderFloat( "End size", &menu_emitter.end_size, 0, 256, "%.2f" );
// 		RandomDistributionEditor( "size", &menu_emitter.size_distribution, menu_emitter.start_size );

// 		ImGui::Separator();

// 		ImGui::SliderFloat( "Lifetime", &menu_emitter.lifetime, 0, 10, "%.2f" );
// 		RandomDistributionEditor( "lifetime", &menu_emitter.lifetime_distribution, menu_emitter.lifetime );

// 		ImGui::Separator();

// 		ImGui::Checkbox( "One shot mode", &menu_one_shot );

// 		if( menu_one_shot ) {
// 			ImGui::SliderFloat( "Particle count", &menu_emitter.n, 0, 500, "%.2f" );
// 			menu_emitter.emission_rate = 0;
// 			emit = ImGui::Button( "Go" );
// 		}
// 		else {
// 			ImGui::SliderFloat( "Emission rate", &menu_emitter.emission_rate, 0, 500, "%.2f" );
// 		}
// 	}
// 	ImGui::EndChild();
// 	ImGui::PopFont();

// 	RendererSetView( Vec3( -400, 0, 400 ), EulerDegrees3( 45, 0, 0 ), 90 );

// 	float dt = cls.frametime / 1000.0f;

// 	if( !menu_one_shot || emit || menu_ps.num_particles == 0 ) {
// 		EmitParticles( &menu_ps, menu_emitter, dt );
// 	}

// 	UpdateParticleSystem( &menu_ps, dt );
// 	DrawParticleSystem( &menu_ps );
// }
