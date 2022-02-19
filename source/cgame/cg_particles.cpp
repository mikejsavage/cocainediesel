#include "qcommon/fs.h"
#include "qcommon/serialization.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "cgame/cg_local.h"

#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "gameshared/q_shared.h"

#include "imgui/imgui.h"

// must match glsl
#define PARTICLE_COLLISION_POINT 1u
#define PARTICLE_COLLISION_SPHERE 2u
#define PARTICLE_ROTATE 4u
#define PARTICLE_STRETCH 8u

#define FEEDBACK_NONE 0u
#define FEEDBACK_AGE 1u
#define FEEDBACK_COLLISION 2u

static ParticleSystem particleSystems[ MAX_PARTICLE_SYSTEMS ];
static u32 num_particleSystems;
static Hashtable< MAX_PARTICLE_SYSTEMS * 2 > particleSystems_hashtable;

static VisualEffectGroup visualEffectGroups[ MAX_VISUAL_EFFECT_GROUPS ];
static u32 num_visualEffectGroups;
static Hashtable< MAX_VISUAL_EFFECT_GROUPS * 2 > visualEffectGroups_hashtable;

static ParticleEmitter particleEmitters[ MAX_PARTICLE_EMITTERS ];
static u32 num_particleEmitters;
static Hashtable< MAX_PARTICLE_EMITTERS * 2 > particleEmitters_hashtable;

static DecalEmitter decalEmitters[ MAX_DECAL_EMITTERS ];
static u32 num_decalEmitters;
static Hashtable< MAX_DECAL_EMITTERS * 2 > decalEmitters_hashtable;

static DynamicLightEmitter dlightEmitters[ MAX_DECAL_EMITTERS ];
static u32 num_dlightEmitters;
static Hashtable< MAX_DLIGHT_EMITTERS * 2 > dlightEmitters_hashtable;

constexpr u32 particles_per_emitter = 10000;

bool ParseParticleEvents( Span< const char > * data, ParticleEvents * event ) {
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
			StringHash name = StringHash( Hash64( key.ptr, key.n ) );

			if( key == "}" ) {
				return true;
			}

			if( key == "" ) {
				Com_Printf( S_COLOR_YELLOW "Missing event\n" );
				return false;
			}

			event->events[ event->num_events ] = name;
			event->num_events++;
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

	ps->particles = ALLOC_SPAN( a, GPUParticle, ps->max_particles );
	ps->gpu_instances = ALLOC_SPAN( a, u32, ps->max_particles );
	if( ps->feedback ) {
		ps->particles_feedback = ALLOC_SPAN( a, GPUParticleFeedback, ps->max_particles );
		memset( ps->particles_feedback.ptr, 0, ps->particles_feedback.num_bytes() );
		ps->vb_feedback = NewGPUBuffer( ps->particles_feedback.begin(), ps->max_particles * sizeof( GPUParticleFeedback ) );
	}
	else {
		ps->gpu_instances_time = ALLOC_SPAN( a, s64, ps->max_particles );
	}
	ps->ibo = NewGPUBuffer( ps->max_particles * sizeof( ps->gpu_instances[ 0 ] ) );
	ps->vb = NewParticleGPUBuffer( ps->max_particles );
	ps->vb2 = NewParticleGPUBuffer( ps->max_particles );

	if( !ps->model ) {
		{
			constexpr Vec2 verts[] = {
				Vec2( -0.5f, -0.5f ),
				Vec2( 0.5f, -0.5f ),
				Vec2( -0.5f, 0.5f ),
				Vec2( 0.5f, 0.5f ),
			};

			constexpr Vec2 uvs[] = {
				Vec2( 0.0f, 0.0f ),
				Vec2( 1.0f, 0.0f ),
				Vec2( 0.0f, 1.0f ),
				Vec2( 1.0f, 1.0f ),
			};

			constexpr u16 indices[] = { 0, 1, 2, 3 };

			MeshConfig mesh_config;
			mesh_config.name = "Particle quad";
			mesh_config.positions = NewGPUBuffer( verts, sizeof( verts ) );
			mesh_config.positions_format = VertexFormat_Floatx2;
			mesh_config.tex_coords = NewGPUBuffer( uvs, sizeof( uvs ) );
			mesh_config.indices = NewGPUBuffer( indices, sizeof( indices ) );
			mesh_config.num_vertices = ARRAY_COUNT( indices );
			mesh_config.primitive_type = PrimitiveType_TriangleStrip;

			ps->mesh = NewMesh( mesh_config );
		}
	}

	{
		MeshConfig mesh_config;
		mesh_config.name = "???";
		mesh_config.indices = ps->ibo;
		mesh_config.indices_format = IndexFormat_U32;
		mesh_config.num_vertices = 1;
		mesh_config.primitive_type = PrimitiveType_Points;

		ps->update_mesh = NewMesh( mesh_config );
	}

	ps->initialized = true;
}

static bool ParseParticleEmitter( ParticleEmitter * emitter, Span< const char > * data ) {
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

			if( key == "material" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				emitter->materials[ emitter->num_materials ] = StringHash( Hash64( value.ptr, value.n ) );
				emitter->num_materials++;
			}
			else if( key == "model" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				emitter->model = StringHash( Hash64( value.ptr, value.n ) );
			}
			else if( key == "blendfunc" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value == "blend" ) {
					emitter->blend_func = BlendFunc_Blend;
				}
				else if( value == "add" ) {
					emitter->blend_func = BlendFunc_Add;
				}
			}
			else if( key == "position" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value == "sphere" ) {
					emitter->position.type = ParticleEmitterPosition_Sphere;
					emitter->position.radius = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				}
				else if( value == "cone" ) {
					emitter->position.type = ParticleEmitterPosition_Sphere;
					emitter->position.theta = ParseFloat( data, 90.0f, Parse_StopOnNewLine );
					emitter->position.radius = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				}
				else if( value == "line" ) {
					emitter->position.type = ParticleEmitterPosition_Line;
					emitter->position.radius = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				}
			}
			else if( key == "collision" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value == "point" ) {
					emitter->flags |= PARTICLE_COLLISION_POINT;
				}
				else if( value == "sphere" ) {
					emitter->flags |= PARTICLE_COLLISION_SPHERE;
				}
			}
			else if( key == "stretch" ) {
				emitter->flags |= PARTICLE_STRETCH;
			}
			else if( key == "rotate" ) {
				emitter->flags |= PARTICLE_ROTATE;
			}
			else if( key == "acceleration" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value == "$gravity" ) {
					emitter->acceleration = -GRAVITY;
				}
				else if( value == "$negative_gravity" ) {
					emitter->acceleration = GRAVITY;
				}
				else {
					emitter->acceleration = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				}
			}
			else if( key == "acceleration" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				if( value == "$gravity" ) {
					emitter->acceleration = -GRAVITY;
				}
				else if( value == "$negative_gravity" ) {
					emitter->acceleration = GRAVITY;
				}
				else {
					emitter->acceleration = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				}
			}
			else if( key == "drag" ) {
				emitter->drag = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "restitution" ) {
				emitter->restitution = ParseFloat( data, 0.8f, Parse_StopOnNewLine );
			}
			else if( key == "speed" ) {
				emitter->speed = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "speed_distribution" ) {
				emitter->speed_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "angle" ) {
				emitter->angle = Radians( ParseFloat( data, 0.0f, Parse_StopOnNewLine ) );
			}
			else if( key == "angle_distribution" ) {
				emitter->angle_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "rotation" ) {
				emitter->rotation = Radians( ParseFloat( data, 0.0f, Parse_StopOnNewLine ) );
			}
			else if( key == "rotation_distribution" ) {
				emitter->rotation_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
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
				float r = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				float g = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				float b = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				float a = ParseFloat( data, 1.0f, Parse_StopOnNewLine );
				emitter->start_color = Vec4( r, g, b, a );
				emitter->end_color = Vec4( r, g, b, 0.0f );
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
			else if( key == "color_override" ) {
				emitter->color_override = true;
			}
			else if( key == "count" ) {
				emitter->count = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "emission" ) {
				emitter->emission = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "on_collision" ) {
				ParseParticleEvents( data, &emitter->on_collision );
				emitter->feedback = true;
			}
			else if( key == "on_age" ) {
				ParseParticleEvents( data, &emitter->on_age );
				emitter->feedback = true;
			}
			else if( key == "on_frame" ) {
				ParseParticleEvents( data, &emitter->on_frame );
				emitter->feedback = true;
			}
		}
	}

	return true;
}

static bool ParseDecalEmitter( DecalEmitter * emitter, Span< const char > * data ) {
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

			if( key == "material" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				emitter->materials[ emitter->num_materials ] = StringHash( Hash64( value.ptr, value.n ) );
				emitter->num_materials++;
			}
			else if( key == "color" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				float r = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				float g = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				float b = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				float a = ParseFloat( data, 1.0f, Parse_StopOnNewLine );
				emitter->color = Vec4( r, g, b, a );
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
			else if( key == "color_override" ) {
				emitter->color_override = true;
			}
			else if( key == "size" ) {
				emitter->size = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
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
			else if( key == "height" ) {
				emitter->height = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
		}
	}

	return true;
}

static bool ParseDynamicLightEmitter( DynamicLightEmitter * emitter, Span< const char > * data ) {
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

			if( key == "color" ) {
				Span< const char > value = ParseToken( data, Parse_StopOnNewLine );
				float r = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				float g = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				float b = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				float a = ParseFloat( data, 1.0f, Parse_StopOnNewLine );
				emitter->color = Vec4( r, g, b, a );
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
			else if( key == "color_override" ) {
				emitter->color_override = true;
			}
			else if( key == "intensity" ) {
				emitter->intensity = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "intensity_distribution" ) {
				emitter->intensity_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
			else if( key == "lifetime" ) {
				emitter->lifetime = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
			}
			else if( key == "lifetime_distribution" ) {
				emitter->lifetime_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
			}
		}
	}

	return true;
}

static bool ParseVisualEffectGroup( VisualEffectGroup * group, Span< const char > * data, u64 base_hash ) {
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
				Com_Printf( S_COLOR_YELLOW "Missing type\n" );
				return false;
			}

			VisualEffect e = { };
			if( key == "particles" ) {
				e.type = VisualEffectType_Particles;
				ParticleEmitter emitter = { };
				if( ParseParticleEmitter( &emitter, data ) ) {
					e.hash = Hash64( ( void * )&group->num_effects, 1, base_hash );

					u64 idx = num_particleEmitters;
					if( !particleEmitters_hashtable.get( e.hash, &idx ) ) {
						particleEmitters_hashtable.add( e.hash, idx );
						num_particleEmitters++;
					}
					particleEmitters[ idx ] = emitter;

					group->effects[ group->num_effects ] = e;
					group->num_effects++;
				}
			}
			else if( key == "decal" ) {
				e.type = VisualEffectType_Decal;
				DecalEmitter emitter = { };
				if( ParseDecalEmitter( &emitter, data ) ) {
					e.hash = Hash64( ( void * )&group->num_effects, 1, base_hash );

					u64 idx = num_decalEmitters;
					if( !decalEmitters_hashtable.get( e.hash, &idx ) ) {
						decalEmitters_hashtable.add( e.hash, idx );
						num_decalEmitters++;
					}
					decalEmitters[ idx ] = emitter;

					group->effects[ group->num_effects ] = e;
					group->num_effects++;
				}
			}
			else if( key == "dlight" ) {
				e.type = VisualEffectType_DynamicLight;
				DynamicLightEmitter emitter = { };
				if( ParseDynamicLightEmitter( &emitter, data ) ) {
					e.hash = Hash64( ( void * )&group->num_effects, 1, base_hash );

					u64 idx = num_dlightEmitters;
					if( !dlightEmitters_hashtable.get( e.hash, &idx ) ) {
						dlightEmitters_hashtable.add( e.hash, idx );
						num_dlightEmitters++;
					}
					dlightEmitters[ idx ] = emitter;

					group->effects[ group->num_effects ] = e;
					group->num_effects++;
				}
			}
		}
	}

	return true;
}

static void LoadVisualEffect( const char * path ) {
	TracyZoneScoped;
	TracyZoneText( path, strlen( path ) );

	Span< const char > data = AssetString( path );
	u64 hash = Hash64( path, strlen( path ) - strlen( ".cdvfx" ) );

	VisualEffectGroup vfx = { };

	if( !ParseVisualEffectGroup( &vfx, &data, hash ) ) {
		Com_Printf( S_COLOR_YELLOW "Couldn't load %s\n", path );
		return;
	}

	u64 idx = num_visualEffectGroups;
	if( !visualEffectGroups_hashtable.get( hash, &idx ) ) {
		visualEffectGroups_hashtable.add( hash, idx );
		num_visualEffectGroups++;
	}
	visualEffectGroups[ idx ] = vfx;
}

void CreateParticleSystems() {
	ParticleSystem * addSystem = &particleSystems[ 0 ];
	u64 addSystem_hash = Hash64( "addSystem", strlen( "addSystem" ) );
	DeleteParticleSystem( sys_allocator, addSystem );
	addSystem->blend_func = BlendFunc_Add;
	particleSystems_hashtable.add( addSystem_hash, 0 );
	num_particleSystems ++;

	ParticleSystem * blendSystem = &particleSystems[ 1 ];
	u64 blendSystem_hash = Hash64( "blendSystem", strlen( "blendSystem" ) );
	DeleteParticleSystem( sys_allocator, blendSystem );
	blendSystem->blend_func = BlendFunc_Blend;
	particleSystems_hashtable.add( blendSystem_hash, 1 );
	num_particleSystems ++;

	for( size_t i = 0; i < num_particleEmitters; i++ ) {
		ParticleEmitter * emitter = &particleEmitters[ i ];
		if( emitter->num_materials ) {
			if( !emitter->feedback ) {
				if( emitter->blend_func == BlendFunc_Add ) {
					emitter->particle_system = addSystem_hash;
					addSystem->max_particles += particles_per_emitter;
				}
				else if( emitter->blend_func == BlendFunc_Blend ) {
					emitter->particle_system = blendSystem_hash;
					blendSystem->max_particles += particles_per_emitter;
				}
			}
			else {
				ParticleSystem ps = { };
				ps.max_particles += particles_per_emitter;
				ps.blend_func = emitter->blend_func;
				ps.feedback = true;
				ps.on_collision = emitter->on_collision;
				ps.on_age = emitter->on_age;
				ps.on_frame = emitter->on_frame;

				// TODO(msc): lol
				u64 hash = Random64( &cls.rng );
				u64 idx = num_particleSystems;
				if( !particleSystems_hashtable.get( hash, &idx ) ) {
					particleSystems_hashtable.add( hash, idx );
					num_particleSystems++;
				}
				particleSystems[ idx ] = ps;
				emitter->particle_system = hash;
			}
		}
		else {
			// models
			ParticleSystem ps = { };
			ps.model = FindModel( emitter->model );
			ps.max_particles += particles_per_emitter;
			u64 hash = Random64( &cls.rng );
			if( emitter->feedback ) {
				ps.blend_func = emitter->blend_func;
				ps.feedback = true;
				ps.on_collision = emitter->on_collision;
				ps.on_age = emitter->on_age;
				ps.on_frame = emitter->on_frame;
			}
			u64 idx = num_particleSystems;
			if( !particleSystems_hashtable.get( hash, &idx ) ) {
				particleSystems_hashtable.add( hash, idx );
				num_particleSystems++;
			}
			particleSystems[ idx ] = ps;

			emitter->particle_system = hash;
		}
	}

	for( size_t i = 0; i < num_particleSystems; i++ ) {
		ParticleSystem * ps = &particleSystems[ i ];
		InitParticleSystem( sys_allocator, ps );
	}
}

void ShutdownParticleSystems() {
	for( size_t i = 0; i < num_particleSystems; i++ ) {
		ParticleSystem * ps = &particleSystems[ i ];
		DeleteParticleSystem( sys_allocator, ps );
		*ps = { };
	}
	particleSystems_hashtable.clear();
	num_particleSystems = 0;
}

void InitVisualEffects() {
	TracyZoneScoped;

	ShutdownParticleSystems();

	for( const char * path : AssetPaths() ) {
		if( FileExtension( path ) == ".cdvfx" ) {
			LoadVisualEffect( path );
		}
	}

	CreateParticleSystems();
}

void HotloadVisualEffects() {
	TracyZoneScoped;

	bool restart_systems = false;
	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".cdvfx" ) {
			LoadVisualEffect( path );
			restart_systems = true;
		}
	}
	if( restart_systems ) {
		ShutdownParticleSystems();
		CreateParticleSystems();
	}
}

VisualEffectGroup * FindVisualEffectGroup( StringHash name ) {
	u64 idx;
	if( !visualEffectGroups_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &visualEffectGroups[ idx ];
}

VisualEffectGroup * FindVisualEffectGroup( const char * name ) {
	return FindVisualEffectGroup( StringHash( name ) );
}

void DeleteParticleSystem( Allocator * a, ParticleSystem * ps ) {
	if( !ps->initialized ) {
		return;
	}
	FREE( a, ps->particles.ptr );
	FREE( a, ps->particles_feedback.ptr );
	FREE( a, ps->gpu_instances.ptr );
	FREE( a, ps->gpu_instances_time.ptr );
	DeleteGPUBuffer( ps->ibo );
	DeleteGPUBuffer( ps->vb );
	DeleteGPUBuffer( ps->vb2 );
	DeleteGPUBuffer( ps->vb_feedback );

	DeleteMesh( ps->mesh );
	DeleteMesh( ps->update_mesh );

	ps->initialized = false;
}

void ShutdownVisualEffects() {
	visualEffectGroups_hashtable.clear();
	num_visualEffectGroups = 0;
	particleEmitters_hashtable.clear();
	num_particleEmitters = 0;
	decalEmitters_hashtable.clear();
	num_decalEmitters = 0;
	ShutdownParticleSystems();
}

bool ParticleFeedback( ParticleSystem * ps, GPUParticleFeedback * feedback ) {
	StringHash despawn = StringHash( "despawn" );
	bool result = true;
	Vec3 position = Floor( feedback->position_normal );
	Vec3 normal = ( ( feedback->position_normal - position ) - 0.5f ) / 0.49f;
	Vec4 color = Vec4( sRGBToLinear( feedback->color ), 1.0f );

	if( feedback->parm & FEEDBACK_COLLISION ) {
		for( u8 i = 0; i < ps->on_collision.num_events; i++ ) {
			StringHash event = ps->on_collision.events[ i ];
			if( event == despawn ) {
				result = false;
			}
			else {
				DoVisualEffect( event, position, normal, 1.0f, color );
			}
		}
	}

	if( feedback->parm & FEEDBACK_AGE ) {
		result = false;
		for( u8 i = 0; i < ps->on_age.num_events; i++ ) {
			StringHash event = ps->on_age.events[ i ];
			if( event == despawn ) {
				result = false;
			}
			else {
				DoVisualEffect( event, position, normal, 1.0f, color );
			}
		}
	}

	for( u8 i = 0; i < ps->on_frame.num_events; i++ ) {
		StringHash event = ps->on_frame.events[ i ];
		if( event == despawn ) {
			result = false;
		}
		else {
			DoVisualEffect( event, position, normal, 1.0f, color );
		}
	}

	return result;
};

void UpdateParticleSystem( ParticleSystem * ps, float dt ) {
	TracyZoneScopedN( "Update particles" );

	size_t previous_num_particles = ps->num_particles;

	{
		TracyZoneScopedN( "Despawn expired particles" );

		if( ps->feedback ) {
			ReadGPUBuffer( ps->vb_feedback, ps->particles_feedback.begin(), ps->num_particles * sizeof( GPUParticleFeedback ) );

			for( size_t i = 0; i < ps->num_particles; i++ ) {
				size_t index = ps->gpu_instances[ i ];
				GPUParticleFeedback feedback = ps->particles_feedback[ index ];
				if( !ParticleFeedback( ps, &feedback ) ) {
					ps->num_particles--;
					Swap2( &ps->gpu_instances[ i ], &ps->gpu_instances[ ps->num_particles ] );
					i--;
				}
			}
		} else {
			for( size_t i = 0; i < ps->num_particles; i++ ) {
				if( ps->gpu_instances_time[ i ] < cls.gametime ) {
					ps->num_particles--;
					Swap2( &ps->gpu_instances[ i ], &ps->gpu_instances[ ps->num_particles ] );
					Swap2( &ps->gpu_instances_time[ i ], &ps->gpu_instances_time[ ps->num_particles ] );
					i--;
				}
			}
		}
	}

	{
		TracyZoneScopedN( "Spawn new particles" );
		if( ps->new_particles > 0 ) {
			WriteGPUBuffer( ps->vb, ps->particles.begin(), ps->new_particles * sizeof( GPUParticle ), previous_num_particles * sizeof( GPUParticle ) );
			for( size_t i = 0; i < ps->new_particles; i++ ) {
				ps->gpu_instances[ ps->num_particles + i ] = previous_num_particles + i;
				if( !ps->feedback ) {
					ps->gpu_instances_time[ ps->num_particles + i ] = cls.gametime + ps->particles[ i ].lifetime * 1000.0f;
				}
			}
		}
	}

	ps->num_particles += ps->new_particles;
	ps->new_particles = 0;

	{
		TracyZoneScopedN( "Upload index buffer" );
		WriteGPUBuffer( ps->ibo, ps->gpu_instances.begin(), ps->num_particles * sizeof( ps->gpu_instances[ 0 ] ) );
	}

	{
		TracyZoneScopedN( "Reset order" );
		for( size_t i = 0; i < ps->num_particles; i++ ) {
			ps->gpu_instances[ i ] = i;
		}
	}
}

void DrawParticleSystem( ParticleSystem * ps, float dt ) {
	if( ps->num_particles == 0 )
		return;

	TracyZoneScoped;

	if( ps->feedback ) {
		UpdateParticlesFeedback( ps->update_mesh, ps->vb, ps->vb2, ps->vb_feedback, ps->radius, ps->num_particles, dt );
	}
	else {
		UpdateParticles( ps->update_mesh, ps->vb, ps->vb2, ps->radius, ps->num_particles, dt );
	}

	if( ps->model ) {
		DrawInstancedParticles( ps->vb2, ps->model, ps->num_particles );
	}
	else {
		DrawInstancedParticles( ps->mesh, ps->vb2, ps->blend_func, ps->num_particles );
	}

	Swap2( &ps->vb, &ps->vb2 );
}

void DrawParticles() {
	float dt = cls.frametime / 1000.0f;

	s64 total_particles = 0;
	s64 total_new_particles = 0;

	for( size_t i = 0; i < num_particleSystems; i++ ) {
		if( particleSystems[ i ].initialized ) {
			total_particles += particleSystems[ i ].num_particles;
			total_new_particles += particleSystems[ i ].new_particles;
			UpdateParticleSystem( &particleSystems[ i ], dt );
			DrawParticleSystem( &particleSystems[ i ], dt );
		}
	}

	TracyCPlot( "Particles", total_particles );
	TracyCPlot( "New Particles", total_new_particles );

	if( cg_particleDebug != NULL && cg_particleDebug->integer ) {
		const ImGuiIO & io = ImGui::GetIO();
		float width_frac = Lerp( 0.25f, Unlerp01( 1024.0f, io.DisplaySize.x, 1920.0f ), 0.15f );
		Vec2 size = io.DisplaySize * Vec2( width_frac, 0.5f );
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs;


		ImGui::SetNextWindowSize( ImVec2( size.x, size.y ) );
		ImGui::SetNextWindowPos( ImVec2( io.DisplaySize.x - size.x, 100.0f ), ImGuiCond_Always );
		ImGui::Begin( "particle statistics", WindowZOrder_Chat, flags );


		ImGui::Text( "%i visual effects", num_visualEffectGroups );
		ImGui::Text( "%i particle emitters", num_particleEmitters );
		ImGui::Text( "%i decal emitters", num_decalEmitters );

		for( size_t i = 0; i < num_particleSystems; i++ ) {
			ParticleSystem * ps = &particleSystems[ i ];
			if( ps->initialized ) {
				ImGui::Text( "ps: %zu, num: %zu / %zu", i, ps->num_particles, ps->max_particles );
			}
		}

		ImGui::End();
	}
}

static void EmitParticle( ParticleSystem * ps, float lifetime, Vec3 position, Vec3 velocity, float angle, float rotation, float acceleration, float drag, float restitution, Vec4 uvwh, Vec4 start_color, Vec4 end_color, float start_size, float end_size, u32 flags ) {
	TracyZoneScopedN( "Store Particle" );
	if( ps->num_particles + ps->new_particles == ps->max_particles )
		return;

	GPUParticle & particle = ps->particles[ ps->new_particles ];
	particle.position = position;
	particle.angle = angle;
	particle.velocity = velocity;
	particle.rotation_speed = rotation;
	particle.acceleration = acceleration;
	particle.drag = drag;
	particle.restitution = restitution;
	particle.uvwh = uvwh;
	particle.start_color = LinearTosRGB( start_color );
	particle.end_color = LinearTosRGB( end_color );
	particle.start_size = start_size;
	particle.end_size = end_size;
	particle.age = 0.0f;
	particle.lifetime = lifetime;
	particle.flags = flags;

	ps->new_particles++;
}

static float SampleRandomDistribution( RNG * rng, RandomDistribution dist ) {
	if( dist.type == RandomDistributionType_Uniform ) {
		return RandomFloat11( rng ) * dist.uniform;
	}

	return SampleNormalDistribution( rng ) * dist.sigma;
}

static void EmitParticle( ParticleSystem * ps, const ParticleEmitter * emitter, ParticleEmitterPosition pos, float t, Vec4 start_color, Vec4 end_color, Mat4 dir_transform ) {
	TracyZoneScopedN( "Emit Particle" );
	float lifetime = Max2( 0.0f, emitter->lifetime + SampleRandomDistribution( &cls.rng, emitter->lifetime_distribution ) );

	float size = Max2( 0.0f, emitter->start_size + SampleRandomDistribution( &cls.rng, emitter->size_distribution ) );

	Vec3 position = pos.origin + size * ps->radius * pos.normal;

	switch( pos.type ) {
		case ParticleEmitterPosition_Sphere: {
			position += UniformSampleInsideSphere( &cls.rng ) * pos.radius;
		} break;

		case ParticleEmitterPosition_Disk: {
			Vec2 p = UniformSampleInsideCircle( &cls.rng );
			position += pos.radius * Vec3( p, 0.0f );
			// TODO: pos.normal;
		} break;

		case ParticleEmitterPosition_Line: {
			position = Lerp( position, RandomFloat01( &cls.rng ), pos.end );
			position += ( dir_transform * Vec4( UniformSampleInsideCircle( &cls.rng ) * pos.radius, 0.0f, 0.0f ) ).xyz();
		} break;
	}

	Vec3 dir;

	if( pos.theta != 0.0f ) {
		dir = ( dir_transform * Vec4( UniformSampleCone( &cls.rng, Radians( pos.theta ) ), 0.0f ) ).xyz();
	}
	else {
		dir = UniformSampleOnSphere( &cls.rng );
	}

	float speed = emitter->speed + SampleRandomDistribution( &cls.rng, emitter->speed_distribution );
	float angle = emitter->angle + Radians( SampleRandomDistribution( &cls.rng, emitter->angle_distribution ) );
	float rotation = emitter->rotation + Radians( SampleRandomDistribution( &cls.rng, emitter->rotation_distribution ) );

	start_color.x += SampleRandomDistribution( &cls.rng, emitter->red_distribution );
	start_color.y += SampleRandomDistribution( &cls.rng, emitter->green_distribution );
	start_color.z += SampleRandomDistribution( &cls.rng, emitter->blue_distribution );
	start_color.w += SampleRandomDistribution( &cls.rng, emitter->alpha_distribution );
	start_color = Clamp01( start_color );

	Vec4 uvwh = Vec4( 0.0f );
	if( ps->model ) {
		EmitParticle( ps, lifetime, position, dir * speed, angle, rotation, emitter->acceleration, emitter->drag, emitter->restitution, uvwh, start_color, end_color, size, emitter->end_size, emitter->flags );
	}
	else if( emitter->num_materials ) {
		if( TryFindDecal( emitter->materials[ RandomUniform( &cls.rng, 0, emitter->num_materials - 1 ) ], &uvwh ) ) {
			EmitParticle( ps, lifetime, position, dir * speed, angle, rotation, emitter->acceleration, emitter->drag, emitter->restitution, uvwh, start_color, end_color, size, emitter->end_size, emitter->flags );
		}
	}
}

void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count, Vec4 color ) {
	TracyZoneScoped;

	if( emitter == NULL ) {
		return;
	}

	float dt = cls.frametime / 1000.0f;
	u64 idx = num_particleSystems;
	if( !particleSystems_hashtable.get( emitter->particle_system, &idx ) ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Particle emitter doesn't have a system\n" );
		return;
	}
	ParticleSystem * ps = &particleSystems[ idx ];

	float p = emitter->count * count + emitter->emission * count * dt;
	u32 n = u32( p );
	float remaining_p = p - n;

	Mat4 dir_transform = Mat4::Identity();
	if( pos.theta != 0.0f ) {
		dir_transform = pos.normal == Vec3( 0.0f ) ? Mat4::Identity() : TransformKToDir( pos.normal );
	}
	else if( pos.type == ParticleEmitterPosition_Line && pos.radius > 0.0f ) {
		Vec3 dir = pos.end - pos.origin;
		dir_transform = dir == Vec3( 0.0f ) ? Mat4::Identity() : TransformKToDir( dir );
	}

	Vec4 start_color = emitter->start_color;
	Vec4 end_color = emitter->end_color;
	if( emitter->color_override ) {
		start_color *= color;
		end_color *= color;
	}

	for( u32 i = 0; i < n; i++ ) {
		float t = float( i ) / ( p + 1.0f );
		EmitParticle( ps, emitter, pos, t, start_color, end_color, dir_transform );
	}

	if( Probability( &cls.rng, remaining_p ) ) {
		EmitParticle( ps, emitter, pos, p / ( p + 1.0f ), start_color, end_color, dir_transform );
	}
}

void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count ) {
	EmitParticles( emitter, pos, count, Vec4( 1.0f ) );
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

void EmitDecal( DecalEmitter * emitter, Vec3 origin, Vec3 normal, Vec4 color, float lifetime_scale ) {
	float lifetime = Max2( 0.0f, emitter->lifetime + SampleRandomDistribution( &cls.rng, emitter->lifetime_distribution ) ) * lifetime_scale;
	float size = Max2( 0.0f, emitter->size + SampleRandomDistribution( &cls.rng, emitter->size_distribution ) );
	float angle = RandomUniformFloat( &cls.rng, 0.0f, Radians( 360.0f ) );
	StringHash material = emitter->materials[ RandomUniform( &cls.rng, 0, emitter->num_materials - 1 ) ];

	Vec4 actual_color = emitter->color;
	if( emitter->color_override ) {
		actual_color *= color;
	}
	actual_color.x += SampleRandomDistribution( &cls.rng, emitter->red_distribution );
	actual_color.y += SampleRandomDistribution( &cls.rng, emitter->green_distribution );
	actual_color.z += SampleRandomDistribution( &cls.rng, emitter->blue_distribution );
	actual_color.w += SampleRandomDistribution( &cls.rng, emitter->alpha_distribution );
	actual_color = Clamp01( actual_color );
	AddPersistentDecal( origin, normal, size, angle, material, actual_color, lifetime * 1000.0f, emitter->height );
}

void EmitDynamicLight( DynamicLightEmitter * emitter, Vec3 origin, Vec4 color ) {
	float lifetime = Max2( 0.0f, emitter->lifetime + SampleRandomDistribution( &cls.rng, emitter->lifetime_distribution ) );
	float intensity = Max2( 0.0f, emitter->intensity + SampleRandomDistribution( &cls.rng, emitter->intensity_distribution ) );

	Vec4 actual_color = emitter->color;
	if( emitter->color_override ) {
		actual_color *= color;
	}
	actual_color.x += SampleRandomDistribution( &cls.rng, emitter->red_distribution );
	actual_color.y += SampleRandomDistribution( &cls.rng, emitter->green_distribution );
	actual_color.z += SampleRandomDistribution( &cls.rng, emitter->blue_distribution );
	actual_color.w += SampleRandomDistribution( &cls.rng, emitter->alpha_distribution );
	actual_color = Clamp01( actual_color );
	AddPersistentDynamicLight( origin, actual_color, intensity, lifetime * 1000.0f );
}

void DoVisualEffect( StringHash name, Vec3 origin, Vec3 normal, float count, Vec4 color, float decal_lifetime_scale ) {
	VisualEffectGroup * vfx = FindVisualEffectGroup( name );
	if( vfx == NULL )
		return;

	for( size_t i = 0; i < vfx->num_effects; i++ ) {
		VisualEffect e = vfx->effects[ i ];
		if( e.type == VisualEffectType_Particles ) {
			u64 idx = num_particleEmitters;
			if( particleEmitters_hashtable.get( e.hash, &idx ) ) {
				ParticleEmitter * emitter = &particleEmitters[ idx ];
				ParticleEmitterPosition pos = emitter->position;
				pos.origin = origin;
				pos.normal = normal;
				EmitParticles( emitter, pos, count, color );
			}
		}
		else if( e.type == VisualEffectType_Decal ) {
			u64 idx = num_decalEmitters;
			if( decalEmitters_hashtable.get( e.hash, &idx ) ) {
				DecalEmitter * emitter = &decalEmitters[ idx ];
				EmitDecal( emitter, origin, normal, color, decal_lifetime_scale );
			}
		}
		else if( e.type == VisualEffectType_DynamicLight ) {
			u64 idx = num_dlightEmitters;
			if( dlightEmitters_hashtable.get( e.hash, &idx ) ) {
				DynamicLightEmitter * emitter = &dlightEmitters[ idx ];
				EmitDynamicLight( emitter, origin, color );
			}
		}
	}
}

void DoVisualEffect( const char * name, Vec3 origin, Vec3 normal, float count, Vec4 color, float decal_lifetime_scale ) {
	DoVisualEffect( StringHash( name ), origin, normal, count, color, decal_lifetime_scale );
}

void ClearParticles() {
	for( size_t i = 0; i < num_particleSystems; i++ ) {
		if( particleSystems[ i ].initialized ) {
			particleSystems[ i ].num_particles = 0;
			particleSystems[ i ].new_particles = 0;
		}
	}
}

void DrawParticleMenuEffect() {
	ImVec2 mouse_pos = ImGui::GetMousePos();
	Vec2 pos = Clamp( Vec2( 0.0f ), Vec2( mouse_pos.x, mouse_pos.y ), frame_static.viewport ) - frame_static.viewport * 0.5f;
	pos *= 0.05f;
	RendererSetView( Vec3( -400, pos.x, pos.y ), EulerDegrees3( 0, 0, 0 ), 90 );
	frame_static.fog_uniforms = UploadUniformBlock( 0.0f );
	DoVisualEffect( "vfx/menu", Vec3( 0.0f ) );
	DrawParticles();
}
