#include "qcommon/fs.h"
#include "qcommon/serialization.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_constants.h"
#include "cgame/cg_local.h"

#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "gameshared/q_shared.h"

#include "imgui/imgui.h"

// TODO: WriteGPUBuffer is always wrong but fixing it here is non-trivial
// we should write new particles to a coherent buffer and perform the CPU->GPU
// copy in the update shader
// similarly we should clear particles by making it part of the update step
// rather than zeroing out a buffer that may or may not be in use
void WriteGPUBuffer( GPUBuffer buf, const void * data, u32 size, u32 offset = 0 );

static constexpr u32 MAX_PARTICLE_SYSTEMS = 512;
static constexpr u32 MAX_PARTICLE_EMITTERS = 512;
static constexpr u32 MAX_PARTICLE_EMITTER_EVENTS = 8;
static constexpr u32 MAX_PARTICLE_EMITTER_MATERIALS = 32;

static constexpr u32 MAX_DECAL_EMITTERS = 512;
static constexpr u32 MAX_DECAL_EMITTER_MATERIALS = 8;

static constexpr u32 MAX_DLIGHT_EMITTERS = 512;

static constexpr u32 MAX_VISUAL_EFFECT_GROUPS = 512;
static constexpr u32 MAX_VISUAL_EFFECTS = 16;

struct GPUParticle {
	Vec3 position;
	float angle;
	Vec3 velocity;
	float angular_velocity;
	float acceleration;
	float drag;
	float restitution;
	float PADDING;
	Vec4 uvwh;
	RGBA8 start_color;
	RGBA8 end_color;
	float start_size;
	float end_size;
	float age;
	float lifetime;
	ParticleFlags flags;
	u32 PADDING2;
};

enum EasingFunction {
	EasingFunction_Linear,
	EasingFunction_Quadratic,
	EasingFunction_Cubic,
	EasingFunction_QuadraticEaseIn,
	EasingFunction_QuadraticEaseOut,
};

enum RandomDistribution3DType : u8 {
	RandomDistribution3DType_Sphere,
	RandomDistribution3DType_Disk,
	RandomDistribution3DType_Line,
};

struct SphereDistribution {
	float radius;
};

struct ConeDistribution {
	Vec3 normal;
	float theta;
};

struct DiskDistribution {
	Vec3 normal;
	float radius;
};

struct LineDistribution {
	Vec3 end;
};

enum RandomDistributionType : u8 {
	RandomDistributionType_Uniform,
	RandomDistributionType_Normal,
};

struct RandomDistribution {
	u8 type;
	union {
		float uniform;
		float sigma;
	};
};

struct RandomDistribution3D {
	u8 type;
	union {
		SphereDistribution sphere;
		DiskDistribution disk;
		LineDistribution line;
	};
};

enum ParticleCollisionType : u8 {
	ParticleCollisionType_None,
	ParticleCollisionType_Point,
	ParticleCollisionType_Sphere,
};

struct ParticleEvents {
	u8 num_events;
	StringHash events[ MAX_PARTICLE_EMITTER_EVENTS ];
};

struct ParticleSystem {
	size_t max_particles;

	BlendFunc blend_func;
	float radius;

	// dynamic stuff
	bool initialized;

	GPUBuffer gpu_particles1;
	GPUBuffer gpu_particles2;

	StreamingBuffer new_particles;
	size_t num_new_particles;

	GPUBuffer compute_count1;
	GPUBuffer compute_count2;

	GPUBuffer compute_indirect;
	GPUBuffer draw_indirect;

	Mesh mesh;
};

enum VisualEffectType : u8 {
	VisualEffectType_Particles,
	VisualEffectType_Decal,
	VisualEffectType_DynamicLight,
};

struct VisualEffect {
	u8 type;
	u64 hash;
};

struct VisualEffectGroup {
	VisualEffect effects[ MAX_VISUAL_EFFECTS ];
	u8 num_effects;
};

enum ParticleEmitterPositionType : u8 {
	ParticleEmitterPosition_Sphere,
	ParticleEmitterPosition_Disk,
	ParticleEmitterPosition_Line,
};

struct ParticleEmitterPosition {
	u8 type;
	Vec3 origin;
	union {
		Vec3 normal;
		Vec3 end;
	};
	float theta;
	float radius;
	RandomDistribution surface_offset;
	float surface_theta;
};

struct ParticleEmitter {
	u64 particle_system;

	u8 num_materials;
	StringHash materials[ MAX_PARTICLE_EMITTER_MATERIALS ];
	StringHash model;

	BlendFunc blend_func = BlendFunc_Add;

	ParticleEmitterPosition position;

	float acceleration;
	float drag = 0.0f;
	float restitution = 0.8f;

	float speed;
	RandomDistribution speed_distribution;

	float angle;
	RandomDistribution angle_distribution;

	float angular_velocity;
	RandomDistribution angular_velocity_distribution;

	Vec4 start_color = Vec4( 1.0f ), end_color = Vec4( 1.0f );
	RandomDistribution red_distribution, green_distribution, blue_distribution, alpha_distribution;
	bool color_override;

	float start_size = 16.0f, end_size = 16.0f;
	RandomDistribution size_distribution;

	float lifetime = 1.0f;
	RandomDistribution lifetime_distribution;

	float count;
	float emission;

	ParticleFlags flags;
};

struct DecalEmitter {
	u8 num_materials;
	StringHash materials[ MAX_DECAL_EMITTER_MATERIALS ];

	Vec4 color = Vec4( 1.0f );
	RandomDistribution red_distribution, green_distribution, blue_distribution, alpha_distribution;
	bool color_override;

	float size = 32.0f;
	RandomDistribution size_distribution;

	float lifetime = 30.0f;
	RandomDistribution lifetime_distribution;

	float height = 0.0f;
};

struct DynamicLightEmitter {
	Vec3 color = Vec3( 1.0f );
	RandomDistribution red_distribution, green_distribution, blue_distribution;
	bool color_override;

	float intensity = 3200.0f;
	RandomDistribution intensity_distribution;

	float lifetime = 5.0f;
	RandomDistribution lifetime_distribution;
};

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

constexpr u32 particles_per_emitter = 1000;

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

struct DrawArraysIndirect {
	u32 count;
	u32 primCount;
	u32 baseVertex;
	u32 baseInstance;
};

void InitParticleSystem( Allocator * a, ParticleSystem * ps ) {
	DeleteParticleSystem( a, ps );

	ps->gpu_particles1 = NewGPUBuffer( NULL, ps->max_particles * sizeof( GPUParticle ), GPUBuffer_Writeable, "particles flip" );
	ps->gpu_particles2 = NewGPUBuffer( NULL, ps->max_particles * sizeof( GPUParticle ), GPUBuffer_Writeable, "particles flop" );

	ps->new_particles = NewStreamingBuffer( ps->max_particles * sizeof( GPUParticle ), "new particles" );

	u32 zero = 0;
	ps->compute_count1 = NewGPUBuffer( &zero, sizeof( u32 ), GPUBuffer_Writeable, "compute_count flip" );
	ps->compute_count2 = NewGPUBuffer( &zero, sizeof( u32 ), GPUBuffer_Writeable, "compute_count flop" );

	u32 counts[] = { 1, 1, 1 };
	ps->compute_indirect = NewGPUBuffer( counts, sizeof( counts ), "compute_indirect" );

	{
		MeshConfig mesh_config = { };
		mesh_config.name = "Particle quad";
		mesh_config.num_vertices = 6;
		ps->mesh = NewMesh( mesh_config );

		DrawArraysIndirect indirect = { };
		indirect.count = 6;
		ps->draw_indirect = NewGPUBuffer( &indirect, sizeof( indirect ), "draw_indirect" );
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
				if( emitter->num_materials == ARRAY_COUNT( emitter->materials ) ) {
					Com_Printf( S_COLOR_YELLOW "Too many materials in particle emitter!\n" );
					return false;
				}

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
				( void ) value;
				// if( value == "point" ) {
				// 	emitter->flags = ParticleFlags( emitter->flags | ParticleFlag_CollisionPoint );
				// }
				// else if( value == "sphere" ) {
				// 	emitter->flags = ParticleFlags( emitter->flags | ParticleFlag_CollisionSphere );
				// }
			}
			else if( key == "stretch" ) {
				emitter->flags = ParticleFlags( emitter->flags | ParticleFlag_Stretch );
			}
			else if( key == "rotate" ) {
				emitter->flags = ParticleFlags( emitter->flags | ParticleFlag_Rotate );
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
			else if( key == "angular_velocity" ) {
				emitter->angular_velocity = Radians( ParseFloat( data, 0.0f, Parse_StopOnNewLine ) );
			}
			else if( key == "angular_velocity_distribution" ) {
				emitter->angular_velocity_distribution = ParseRandomDistribution( data, Parse_StopOnNewLine );
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
				float r = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				float g = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				float b = ParseFloat( data, 0.0f, Parse_StopOnNewLine );
				emitter->end_color = Vec4( r, g, b, 0.0f );
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
				emitter->color = Vec3( r, g, b );
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
	num_particleSystems++;

	ParticleSystem * blendSystem = &particleSystems[ 1 ];
	u64 blendSystem_hash = Hash64( "blendSystem", strlen( "blendSystem" ) );
	DeleteParticleSystem( sys_allocator, blendSystem );
	blendSystem->blend_func = BlendFunc_Blend;
	particleSystems_hashtable.add( blendSystem_hash, 1 );
	num_particleSystems++;

	for( size_t i = 0; i < num_particleEmitters; i++ ) {
		ParticleEmitter * emitter = &particleEmitters[ i ];
		if( emitter->num_materials ) {
			if( emitter->blend_func == BlendFunc_Add ) {
				emitter->particle_system = addSystem_hash;
				addSystem->max_particles += particles_per_emitter;
			}
			else if( emitter->blend_func == BlendFunc_Blend ) {
				emitter->particle_system = blendSystem_hash;
				blendSystem->max_particles += particles_per_emitter;
			}
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
	DeleteGPUBuffer( ps->gpu_particles1 );
	DeleteGPUBuffer( ps->gpu_particles2 );
	DeleteStreamingBuffer( ps->new_particles );
	DeleteGPUBuffer( ps->compute_count1 );
	DeleteGPUBuffer( ps->compute_count2 );
	DeleteGPUBuffer( ps->compute_indirect );
	DeleteGPUBuffer( ps->draw_indirect );
	DeleteMesh( ps->mesh );

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

static void UpdateParticleSystem( ParticleSystem * ps, float dt ) {
	{
		PipelineState pipeline;
		pipeline.pass = frame_static.particle_update_pass;
		pipeline.shader = &shaders.particle_compute;
		pipeline.bind_buffer( "b_ParticlesIn", ps->gpu_particles1 );
		pipeline.bind_buffer( "b_ParticlesOut", ps->gpu_particles2 );
		pipeline.bind_streaming_buffer( "b_NewParticles", ps->new_particles );
		pipeline.bind_buffer( "b_ComputeCountIn", ps->compute_count1 );
		pipeline.bind_buffer( "b_ComputeCountOut", ps->compute_count2 );
		// u32 collision = cl.map == NULL ? 0 : 1;
		u32 collision = 0;
		pipeline.bind_uniform( "u_ParticleUpdate", UploadUniformBlock( collision, ps->radius, dt, u32( ps->num_new_particles ) ) );
		// if( collision ) {
		// 	pipeline.bind_buffer( "b_BSPNodeLinks", cl.map->render_data.nodes );
		// 	pipeline.bind_buffer( "b_BSPLeaves", cl.map->render_data.leaves );
		// 	pipeline.bind_buffer( "b_BSPBrushes", cl.map->render_data.brushes );
		// 	pipeline.bind_buffer( "b_BSPPlanes", cl.map->render_data.planes );
		// }
		DispatchComputeIndirect( pipeline, ps->compute_indirect );
	}

	{
		PipelineState pipeline;
		pipeline.pass = frame_static.particle_setup_indirect_pass;
		pipeline.shader = &shaders.particle_setup_indirect;
		pipeline.bind_buffer( "b_NextComputeCount", ps->compute_count1 );
		pipeline.bind_buffer( "b_ComputeCount", ps->compute_count2 );
		pipeline.bind_buffer( "b_ComputeIndirect", ps->compute_indirect );
		pipeline.bind_buffer( "b_DrawIndirect", ps->draw_indirect );
		pipeline.bind_uniform( "u_ParticleUpdate", UploadUniformBlock( u32( ps->num_new_particles ) ) );
		DispatchCompute( pipeline, 1, 1, 1 );
	}

	ps->num_new_particles = 0;
}

static void DrawParticleSystem( ParticleSystem * ps, float dt ) {
	PipelineState pipeline;
	pipeline.pass = frame_static.transparent_pass;
	pipeline.shader = &shaders.particle;
	pipeline.blend_func = ps->blend_func;
	pipeline.cull_face = CullFace_Disabled;
	pipeline.write_depth = false;
	pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
	pipeline.bind_texture_and_sampler( "u_DecalAtlases", DecalAtlasTextureArray(), Sampler_Standard );
	pipeline.bind_buffer( "b_Particles", ps->gpu_particles2 );
	DrawMeshIndirect( ps->mesh, pipeline, ps->draw_indirect );

	Swap2( &ps->gpu_particles1, &ps->gpu_particles2 );
	Swap2( &ps->compute_count1, &ps->compute_count2 );
}

void DrawParticles() {
	float dt = cls.frametime / 1000.0f;

	s64 total_new_particles = 0;

	for( size_t i = 0; i < num_particleSystems; i++ ) {
		if( particleSystems[ i ].initialized ) {
			total_new_particles += particleSystems[ i ].num_new_particles;
			UpdateParticleSystem( &particleSystems[ i ], dt );
			DrawParticleSystem( &particleSystems[ i ], dt );
		}
	}

	TracyPlotSample( "New Particles", total_new_particles );
}

static void EmitParticle( ParticleSystem * ps, float lifetime, Vec3 position, Vec3 velocity, float angle, float angular_velocity, float acceleration, float drag, float restitution, Vec4 uvwh, Vec4 start_color, Vec4 end_color, float start_size, float end_size, ParticleFlags flags ) {
	TracyZoneScopedN( "Store Particle" );
	if( ps->num_new_particles == ps->max_particles )
		return;

	GPUParticle * new_particles = ( GPUParticle * ) GetStreamingBufferMemory( ps->new_particles );
	new_particles[ ps->num_new_particles ] = GPUParticle {
		.position = position,
		.angle = angle,
		.velocity = velocity,
		.angular_velocity = angular_velocity,
		.acceleration = acceleration,
		.drag = drag,
		.restitution = restitution,
		.uvwh = uvwh,
		.start_color = LinearTosRGB( start_color ),
		.end_color = LinearTosRGB( end_color ),
		.start_size = start_size,
		.end_size = end_size,
		.age = 0.0f,
		.lifetime = lifetime,
		.flags = flags,
	};

	ps->num_new_particles++;
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
	float angular_velocity = emitter->angular_velocity + Radians( SampleRandomDistribution( &cls.rng, emitter->angular_velocity_distribution ) );

	start_color.x += SampleRandomDistribution( &cls.rng, emitter->red_distribution );
	start_color.y += SampleRandomDistribution( &cls.rng, emitter->green_distribution );
	start_color.z += SampleRandomDistribution( &cls.rng, emitter->blue_distribution );
	start_color.w += SampleRandomDistribution( &cls.rng, emitter->alpha_distribution );
	start_color = Clamp01( start_color );

	if( emitter->num_materials ) {
		Vec4 uvwh = Vec4( 0.0f );
		if( TryFindDecal( emitter->materials[ RandomUniform( &cls.rng, 0, emitter->num_materials - 1 ) ], &uvwh ) ) {
			EmitParticle( ps, lifetime, position, dir * speed, angle, angular_velocity, emitter->acceleration, emitter->drag, emitter->restitution, uvwh, start_color, end_color, size, emitter->end_size, emitter->flags );
		}
	}
}

static void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count, Vec4 color ) {
	TracyZoneScoped;

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

static void EmitDecal( DecalEmitter * emitter, Vec3 origin, Vec3 normal, Vec4 color, float lifetime_scale ) {
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

static void EmitDynamicLight( DynamicLightEmitter * emitter, Vec3 origin, Vec3 color ) {
	float lifetime = Max2( 0.0f, emitter->lifetime + SampleRandomDistribution( &cls.rng, emitter->lifetime_distribution ) );
	float intensity = Max2( 0.0f, emitter->intensity + SampleRandomDistribution( &cls.rng, emitter->intensity_distribution ) );

	Vec3 actual_color = emitter->color;
	if( emitter->color_override ) {
		actual_color *= color;
	}
	actual_color.x += SampleRandomDistribution( &cls.rng, emitter->red_distribution );
	actual_color.y += SampleRandomDistribution( &cls.rng, emitter->green_distribution );
	actual_color.z += SampleRandomDistribution( &cls.rng, emitter->blue_distribution );
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
				EmitDynamicLight( emitter, origin, color.xyz() );
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
			ParticleSystem * ps = &particleSystems[ i ];
			ps->num_new_particles = 0;

			u32 zero = 0;
			WriteGPUBuffer( ps->compute_count1, &zero, sizeof( zero ) );
			WriteGPUBuffer( ps->compute_count2, &zero, sizeof( zero ) );
		}
	}
}

void DrawParticleMenuEffect() {
	ImVec2 mouse_pos = ImGui::GetMousePos();
	Vec2 pos = Clamp( Vec2( 0.0f ), Vec2( mouse_pos.x, mouse_pos.y ), frame_static.viewport ) - frame_static.viewport * 0.5f;
	pos *= 0.05f;
	RendererSetView( Vec3( -400, pos.x, pos.y ), EulerDegrees3( 0, 0, 0 ), 90 );
	DoVisualEffect( "vfx/menu", Vec3( 0.0f ) );
	DrawParticles();
}
