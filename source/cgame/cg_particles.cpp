#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/serialization.h"
#include "qcommon/time.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_shared.h"
#include "cgame/cg_local.h"

#include "qcommon/hash.h"
#include "qcommon/hashmap.h"
#include "gameshared/q_shared.h"

#include "imgui/imgui.h"

static constexpr u32 MAX_PARTICLE_EMITTERS = 512;
static constexpr u32 MAX_PARTICLE_EMITTER_EVENTS = 8;
static constexpr u32 MAX_PARTICLE_EMITTER_MATERIALS = 32;

static constexpr u32 MAX_DECAL_EMITTERS = 512;
static constexpr u32 MAX_DECAL_EMITTER_MATERIALS = 8;

static constexpr u32 MAX_DLIGHT_EMITTERS = 512;

static constexpr u32 MAX_VISUAL_EFFECT_GROUPS = 512;
static constexpr u32 MAX_VISUAL_EFFECTS = 16;

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
	BoundedDynamicArray< StringHash, MAX_PARTICLE_EMITTER_EVENTS > events;
};

struct ParticleSystem {
	size_t max_particles;

	BlendFunc blend_func;

	GPUBuffer gpu_particles1;
	GPUBuffer gpu_particles2;

	GPUBuffer new_particles;
	u32 num_new_particles;
	bool clear;

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
	BoundedDynamicArray< VisualEffect, MAX_VISUAL_EFFECTS > effects;
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
	BoundedDynamicArray< StringHash, MAX_PARTICLE_EMITTER_MATERIALS > materials;

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
	BoundedDynamicArray< StringHash, MAX_DECAL_EMITTER_MATERIALS > materials;

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

static ParticleSystem addParticleSystem;
static ParticleSystem blendParticleSystem;
static Hashmap< VisualEffectGroup, MAX_VISUAL_EFFECT_GROUPS > visualEffectGroups;
static Hashmap< ParticleEmitter, MAX_PARTICLE_EMITTERS > particleEmitters;
static Hashmap< DecalEmitter, MAX_DECAL_EMITTERS > decalEmitters;
static Hashmap< DynamicLightEmitter, MAX_DLIGHT_EMITTERS > dlightEmitters;

static ParticleSystem NewParticleSystem( Allocator * a, BlendFunc blend_func, size_t max_particles ) {
	u32 zero = 0;
	DispatchComputeIndirectArguments compute_indirect = { 1, 1, 1 };
	DrawArraysIndirectArguments draw_indirect = { .count = 6 };

	return ParticleSystem {
		.max_particles = max_particles,
		.blend_func = blend_func,

		.gpu_particles1 = NewBuffer( GPULifetime_Persistent, "particles flip", max_particles * sizeof( Particle ), sizeof( Particle ) ),
		.gpu_particles2 = NewBuffer( GPULifetime_Persistent, "particles flop",  max_particles * sizeof( Particle ), sizeof( Particle ) ),
		.new_particles = NewStreamingBuffer( max_particles * sizeof( Particle ), "new particles" ),

		.compute_count1 = NewBuffer( GPULifetime_Persistent, "compute_count flip", zero ),
		.compute_count2 = NewBuffer( GPULifetime_Persistent, "compute_count flop", zero ),

		.compute_indirect = NewBuffer( GPULifetime_Persistent, "particle compute indirect", &compute_indirect, sizeof( compute_indirect ) ),
		.draw_indirect = NewBuffer( GPULifetime_Persistent, "particle draw indirect", &draw_indirect, sizeof( draw_indirect ) ),
	};
}

static RandomDistribution ParseRandomDistribution( Span< const char > * data, ParseStopOnNewLine stop ) {
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
				if( !emitter->materials.add( StringHash( Hash64( value.ptr, value.n ) ) ) ) {
					Com_Printf( S_COLOR_YELLOW "Too many materials in particle emitter!\n" );
					return false;
				}
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
				if( !emitter->materials.add( StringHash( Hash64( value.ptr, value.n ) ) ) ) {
					Com_Printf( S_COLOR_YELLOW "Too many materials in decal emitter!\n" );
					return false;
				}
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

			if( key == "particles" ) {
				ParticleEmitter emitter = { };
				if( !ParseParticleEmitter( &emitter, data ) )
					return false;

				size_t n = group->effects.size();
				VisualEffect e = {
					.type = VisualEffectType_Particles,
					.hash = Hash64( &n, sizeof( n ), base_hash ),
				};

				if( !particleEmitters.upsert( e.hash, emitter ) ) {
					Com_Printf( S_COLOR_YELLOW "Too many particle emitters\n" );
					return false;
				}

				if( !group->effects.add( e ) ) {
					Com_Printf( S_COLOR_YELLOW "Too many effects\n" );
					return false;
				}
			}
			else if( key == "decal" ) {
				DecalEmitter emitter = { };
				if( !ParseDecalEmitter( &emitter, data ) )
					return false;

				size_t n = group->effects.size();
				VisualEffect e = {
					.type = VisualEffectType_Decal,
					.hash = Hash64( &n, sizeof( n ), base_hash ),
				};

				if( !decalEmitters.upsert( e.hash, emitter ) ) {
					Com_Printf( S_COLOR_YELLOW "Too many decal emitters\n" );
					return false;
				}

				if( !group->effects.add( e ) ) {
					Com_Printf( S_COLOR_YELLOW "Too many effects\n" );
					return false;
				}
			}
			else if( key == "dlight" ) {
				DynamicLightEmitter emitter = { };
				if( !ParseDynamicLightEmitter( &emitter, data ) )
					return false;

				size_t n = group->effects.size();
				VisualEffect e = {
					.type = VisualEffectType_DynamicLight,
					.hash = Hash64( &n, sizeof( n ), base_hash ),
				};

				if( !dlightEmitters.upsert( e.hash, emitter ) ) {
					Com_Printf( S_COLOR_YELLOW "Too many dlight emitters\n" );
					return false;
				}

				if( !group->effects.add( e ) ) {
					Com_Printf( S_COLOR_YELLOW "Too many effects\n" );
					return false;
				}
			}
		}
	}

	return true;
}

static void LoadVisualEffect( Span< const char > path ) {
	TracyZoneScoped;
	TracyZoneSpan( path );

	Span< const char > data = AssetString( path );
	u64 hash = Hash64( StripExtension( path ) );

	VisualEffectGroup vfx = { };

	if( !ParseVisualEffectGroup( &vfx, &data, hash ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Couldn't load {}", path );
		return;
	}

	if( !visualEffectGroups.upsert( hash, vfx ) ) {
		Com_Printf( S_COLOR_YELLOW "Too many VFX groups!" );
	}
}

static void CreateParticleSystems() {
	static constexpr size_t particles_per_emitter = 1000;
	size_t add_max_particles = 0;
	size_t blend_max_particles = 0;

	for( const ParticleEmitter & emitter : particleEmitters.span() ) {
		if( emitter.materials.size() > 0 ) {
			if( emitter.blend_func == BlendFunc_Add ) {
				add_max_particles += particles_per_emitter;
			}
			else if( emitter.blend_func == BlendFunc_Blend ) {
				blend_max_particles += particles_per_emitter;
			}
		}
	}

	addParticleSystem = NewParticleSystem( sys_allocator, BlendFunc_Add, add_max_particles );
	blendParticleSystem = NewParticleSystem( sys_allocator, BlendFunc_Blend, blend_max_particles );
}

void InitVisualEffects() {
	TracyZoneScoped;

	visualEffectGroups.clear();
	particleEmitters.clear();
	decalEmitters.clear();
	dlightEmitters.clear();

	for( Span< const char > path : AssetPaths() ) {
		if( FileExtension( path ) == ".cdvfx" ) {
			LoadVisualEffect( path );
		}
	}

	CreateParticleSystems();
}

void HotloadVisualEffects() {
	TracyZoneScoped;

	bool restart_systems = false;
	for( Span< const char > path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".cdvfx" ) {
			LoadVisualEffect( path );
			restart_systems = true;
		}
	}
	if( restart_systems ) {
		CreateParticleSystems();
	}
}

VisualEffectGroup * FindVisualEffectGroup( StringHash name ) {
	return visualEffectGroups.get( name.hash );
}

VisualEffectGroup * FindVisualEffectGroup( const char * name ) {
	return FindVisualEffectGroup( StringHash( name ) );
}

static void UpdateParticleSystem( ParticleSystem * ps, float dt ) {
	{
		// u32 collision = cl.map == NULL ? 0 : 1;
		// if( collision ) {
		// 	pipeline.bind_buffer( "b_BSPNodeLinks", cl.map->render_data.nodes );
		// 	pipeline.bind_buffer( "b_BSPLeaves", cl.map->render_data.leaves );
		// 	pipeline.bind_buffer( "b_BSPBrushes", cl.map->render_data.brushes );
		// 	pipeline.bind_buffer( "b_BSPPlanes", cl.map->render_data.planes );
		// }

		GPUBuffer update = NewTempBuffer( ParticleUpdateUniforms {
			.collision = 0_u32,
			.dt = dt,
			.num_new_particles = ps->num_new_particles,
		} );

		EncodeIndirectComputeCall( RenderPass_ParticleUpdate, shaders.particle_compute, ps->compute_indirect, {
			{ "b_ParticlesIn", ps->gpu_particles1 },
			{ "b_ParticlesOut", ps->gpu_particles2 },
			{ "b_NewParticles", ... },
			{ "b_ComputeCountIn", ps->compute_count1 },
			{ "b_ComputeCountOut", ps->compute_count2 },
			{ "b_ParticleUpdate", update },
		} );
	}

	{
		NewParticlesUniforms new_particles = {
			.num_new_particles = ps->num_new_particles,
			.clear = ps->clear ? 1_u32 : 0_u32,
		};

		EncodeComputeCall( RenderPass_ParticleSetupIndirect, shaders.particle_setup_indirect, 1, 1, 1, {
			{ "b_NextComputeCount", ps->compute_count1 },
			{ "b_ComputeCount", ps->compute_count2 },
			{ "b_ComputeIndirect", ps->compute_indirect },
			{ "b_DrawIndirect", ps->draw_indirect },
			{ "b_ParticleUpdate", NewTempBuffer( new_particles ) },
		} );
	}

	ps->num_new_particles = 0;
	ps->clear = false;
}

static void DrawParticleSystem( ParticleSystem * ps, float dt ) {
	PipelineState pipeline = {
		.shader = ps->blend_func == BlendFunc_Add ? shaders.particle_add : shaders.particle_blend,
		.dynamic_state = { .depth_func = DepthFunc_LessNoWrite },
		.material_bind_group = DecalAtlasBindGroup(),
	};

	Mesh mesh = { .num_vertices = 6 };
	EncodeDrawCall( RenderPass_Transparent, pipeline, mesh, {
		{ "b_Particles", ps->gpu_particles2 },
	} );

	Swap2( &ps->gpu_particles1, &ps->gpu_particles2 );
	Swap2( &ps->compute_count1, &ps->compute_count2 );
}

void DrawParticles() {
	float dt = cls.frametime / 1000.0f;

	s64 total_new_particles = addParticleSystem.num_new_particles + blendParticleSystem.num_new_particles;

	// TODO: probably merge these into one pass
	frame_static.render_passes[ RenderPass_ParticleUpdate ] = NewComputePass( ComputePassConfig {
		.name = "Update particles",
		// .signal = ...,
	} );

	frame_static.render_passes[ RenderPass_ParticleSetupIndirect ] = NewComputePass( ComputePassConfig {
		.name = "Particle setup indirect",
		// .wait = ...,
		// .signal = ...,
	} );

	UpdateParticleSystem( &addParticleSystem, dt );
	DrawParticleSystem( &addParticleSystem, dt );
	UpdateParticleSystem( &blendParticleSystem, dt );
	DrawParticleSystem( &blendParticleSystem, dt );

	TracyPlotSample( "New Particles", total_new_particles );
}

static void EmitParticle( ParticleSystem * ps, float lifetime, Vec3 position, Vec3 velocity, float angle, float angular_velocity, float acceleration, float drag, float restitution, Vec4 uvwh, Vec4 trim, Vec4 start_color, Vec4 end_color, float start_size, float end_size, ParticleFlags flags ) {
	TracyZoneScopedN( "Store Particle" );
	if( ps->num_new_particles == ps->max_particles )
		return;

	Particle * new_particles = ( Particle * ) GetStreamingBufferMemory( ps->new_particles );
	new_particles[ ps->num_new_particles ] = Particle {
		.position = position,
		.angle = angle,
		.velocity = velocity,
		.angular_velocity = angular_velocity,
		.acceleration = acceleration,
		.drag = drag,
		.restitution = restitution,
		.uvwh = uvwh,
		.trim = trim,
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

static void EmitParticle( ParticleSystem * ps, const ParticleEmitter * emitter, ParticleEmitterPosition pos, float t, Vec4 start_color, Vec4 end_color, Mat3x4 dir_transform ) {
	TracyZoneScopedN( "Emit Particle" );
	float lifetime = Max2( 0.0f, emitter->lifetime + SampleRandomDistribution( &cls.rng, emitter->lifetime_distribution ) );

	float size = Max2( 0.0f, emitter->start_size + SampleRandomDistribution( &cls.rng, emitter->size_distribution ) );

	Vec3 position = pos.origin;

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

	if( emitter->materials.size() > 0 ) {
		Vec4 uvwh = Vec4( 0.0f );
		Vec4 trim = Vec4( 0.0f, 0.0f, 1.0f, 1.0f );
		if( TryFindDecal( RandomElement( &cls.rng, emitter->materials.span() ), &uvwh, &trim ) ) {
			EmitParticle( ps, lifetime, position, dir * speed, angle, angular_velocity, emitter->acceleration, emitter->drag, emitter->restitution, uvwh, trim, start_color, end_color, size, emitter->end_size, emitter->flags );
		}
	}
}

static Mat3x4 TransformKToDir( Vec3 dir ) {
	if( dir == Vec3( 0.0f ) )
		return Mat3x4::Identity();

	dir = Normalize( dir );

	Vec3 K = Vec3( 0, 0, 1 );

	Vec3 axis;
	if( Abs( dir.z ) < 0.9999f ) {
		axis = Normalize( Cross( K, dir ) );
	}
	else {
		axis = Vec3( 1.0f, 0.0f, 0.0f );
	}

	float c = Dot( K, dir );
	float s = sqrtf( 1.0f - c * c );

	return Mat3x4(
		c + axis.x * axis.x * ( 1.0f - c ),
		axis.x * axis.y * ( 1.0f - c ) - axis.z * s,
		axis.x * axis.z * ( 1.0f - c ) + axis.y * s,
		0.0f,

		axis.y * axis.x * ( 1.0f - c ) + axis.z * s,
		c + axis.y * axis.y * ( 1.0f - c ),
		axis.y * axis.z * ( 1.0f - c ) - axis.x * s,
		0.0f,

		axis.z * axis.x * ( 1.0f - c ) - axis.y * s,
		axis.z * axis.y * ( 1.0f - c ) + axis.x * s,
		c + axis.z * axis.z * ( 1.0f - c ),
		0.0f
	);
}

static void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count, Vec4 color ) {
	TracyZoneScoped;

	float dt = cls.frametime / 1000.0f;
	ParticleSystem * ps = emitter->blend_func == BlendFunc_Add ? &addParticleSystem : &blendParticleSystem;

	float p = emitter->count * count + emitter->emission * count * dt;
	u32 n = u32( p );
	float remaining_p = p - n;

	Mat3x4 dir_transform = Mat3x4::Identity();
	if( pos.theta != 0.0f ) {
		dir_transform = TransformKToDir( pos.normal );
	}
	else if( pos.type == ParticleEmitterPosition_Line && pos.radius > 0.0f ) {
		dir_transform = TransformKToDir( pos.end - pos.origin );
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
	if( normal == Vec3( 0.0f ) ) {
		return;
	}

	float lifetime = Max2( 0.0f, emitter->lifetime + SampleRandomDistribution( &cls.rng, emitter->lifetime_distribution ) ) * lifetime_scale;
	float size = Max2( 0.0f, emitter->size + SampleRandomDistribution( &cls.rng, emitter->size_distribution ) );
	float angle = RandomUniformFloat( &cls.rng, 0.0f, Radians( 360.0f ) );
	StringHash material = RandomElement( &cls.rng, emitter->materials.span() );

	Vec4 actual_color = emitter->color;
	if( emitter->color_override ) {
		actual_color *= color;
	}
	actual_color.x += SampleRandomDistribution( &cls.rng, emitter->red_distribution );
	actual_color.y += SampleRandomDistribution( &cls.rng, emitter->green_distribution );
	actual_color.z += SampleRandomDistribution( &cls.rng, emitter->blue_distribution );
	actual_color.w += SampleRandomDistribution( &cls.rng, emitter->alpha_distribution );
	actual_color = Clamp01( actual_color );
	AddPersistentDecal( origin, QuaternionFromNormalAndRadians( normal, angle ), size, material, actual_color, Seconds( lifetime ), emitter->height );
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
	AddPersistentDynamicLight( origin, actual_color, intensity, Seconds( lifetime ) );
}

void DoVisualEffect( StringHash name, Vec3 origin, Vec3 normal, float count, Vec4 color, float decal_lifetime_scale ) {
	VisualEffectGroup * vfx = FindVisualEffectGroup( name );
	if( vfx == NULL )
		return;

	for( const VisualEffect & e : vfx->effects ) {
		if( e.type == VisualEffectType_Particles ) {
			ParticleEmitter * emitter = particleEmitters.get( e.hash );
			if( emitter != NULL ) {
				ParticleEmitterPosition pos = emitter->position;
				pos.origin = origin;
				pos.normal = normal;
				EmitParticles( emitter, pos, count, color );
			}
		}
		else if( e.type == VisualEffectType_Decal ) {
			DecalEmitter * emitter = decalEmitters.get( e.hash );
			if( emitter != NULL ) {
				EmitDecal( emitter, origin, normal, color, decal_lifetime_scale );
			}
		}
		else if( e.type == VisualEffectType_DynamicLight ) {
			DynamicLightEmitter * emitter = dlightEmitters.get( e.hash );
			if( emitter != NULL ) {
				EmitDynamicLight( emitter, origin, color.xyz() );
			}
		}
	}
}

void DoVisualEffect( const char * name, Vec3 origin, Vec3 normal, float count, Vec4 color, float decal_lifetime_scale ) {
	DoVisualEffect( StringHash( name ), origin, normal, count, color, decal_lifetime_scale );
}

void ClearParticles() {
	addParticleSystem.clear = true;
	blendParticleSystem.clear = true;
}
