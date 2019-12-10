#include "qcommon/assets.h"
#include "qcommon/fs.h"
#include "qcommon/serialization.h"
#include "cgame/cg_local.h"

#include "imgui/imgui.h"

void InitParticles() {
	constexpr Vec3 gravity = Vec3( 0, 0, -GRAVITY );

	cgs.ions = NewParticleSystem( sys_allocator, 8192, FindMaterial( "$particle" ) );
	cgs.SMGsparks = NewParticleSystem( sys_allocator, 8192, FindMaterial( "weapons/SMG/SMGsparks" ) );
	cgs.SMGsparks.acceleration = gravity;
	cgs.smoke = NewParticleSystem( sys_allocator, 1024, FindMaterial( "gfx/misc/cartoon_smokepuff3" ) );
	cgs.sparks = NewParticleSystem( sys_allocator, 8192, FindMaterial( "$particle" ) );
	cgs.sparks.acceleration = gravity;
	cgs.sparks.blend_func = BlendFunc_Blend;
}

void ShutdownParticles() {
	DeleteParticleSystem( sys_allocator, cgs.ions );
	DeleteParticleSystem( sys_allocator, cgs.SMGsparks );
	DeleteParticleSystem( sys_allocator, cgs.sparks );
	DeleteParticleSystem( sys_allocator, cgs.smoke );
}

ParticleSystem NewParticleSystem( Allocator * a, size_t n, const Material * material ) {
	ParticleSystem ps = { };
	size_t num_chunks = AlignPow2( n, size_t( 4 ) ) / 4;
	ps.chunks = ALLOC_SPAN( a, ParticleChunk, num_chunks );
	ps.blend_func = BlendFunc_Add;

	ps.material = material;
	ps.gradient = cgs.white_material;

	ps.vb = NewParticleVertexBuffer( n );
	ps.vb_memory = ALLOC_MANY( a, GPUParticle, n );

	{
		constexpr Vec2 verts[] = {
			Vec2( -0.5f, -0.5f ),
			Vec2( 0.5f, -0.5f ),
			Vec2( -0.5f, 0.5f ),
			Vec2( 0.5f, 0.5f ),
		};

		Vec2 half_pixel = 0.5f / Vec2( material->texture->width, material->texture->height );
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

		ps.mesh = NewMesh( mesh_config );
	}

	return ps;
}

void DeleteParticleSystem( Allocator * a, ParticleSystem ps ) {
	FREE( a, ps.chunks.ptr );
	FREE( a, ps.vb_memory );
	DeleteVertexBuffer( ps.vb );
	DeleteMesh( ps.mesh );
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

static void UpdateParticleChunk( const ParticleSystem * ps, ParticleChunk * chunk, Vec3 acceleration, float dt ) {
	for( int i = 0; i < 4; i++ ) {
		chunk->t[ i ] += dt;
		float t = chunk->t[ i ] / chunk->lifetime[ i ];

		chunk->velocity_x[ i ] += acceleration.x * dt;
		chunk->velocity_y[ i ] += acceleration.y * dt;
		chunk->velocity_z[ i ] += acceleration.z * dt;

		float velocity = Max2( 0.0001f, Length( Vec3( chunk->velocity_x[ i ], chunk->velocity_y[ i ], chunk->velocity_z[ i ] ) ) );
		float new_velocity = Max2( 0.0001f, velocity + chunk->dvelocity[ i ] * dt );
		float velocity_scale = new_velocity / velocity;

		chunk->velocity_x[ i ] *= velocity_scale;
		chunk->velocity_y[ i ] *= velocity_scale;
		chunk->velocity_z[ i ] *= velocity_scale;

		chunk->position_x[ i ] += chunk->velocity_x[ i ] * dt;
		chunk->position_y[ i ] += chunk->velocity_y[ i ] * dt;
		chunk->position_z[ i ] += chunk->velocity_z[ i ] * dt;

		chunk->color_r[ i ] += EvaluateEasingDerivative( ps->color_easing, t ) * chunk->dcolor_r[ i ] * dt;
		chunk->color_g[ i ] += EvaluateEasingDerivative( ps->color_easing, t ) * chunk->dcolor_g[ i ] * dt;
		chunk->color_b[ i ] += EvaluateEasingDerivative( ps->color_easing, t ) * chunk->dcolor_b[ i ] * dt;
		chunk->color_a[ i ] += EvaluateEasingDerivative( ps->color_easing, t ) * chunk->dcolor_a[ i ] * dt;

		chunk->size[ i ] += EvaluateEasingDerivative( ps->size_easing, t ) * chunk->dsize[ i ] * dt;
	}
}

void UpdateParticleSystem( ParticleSystem * ps, float dt ) {
	{
		ZoneScopedN( "Update particles" );
		size_t active_chunks = AlignPow2( ps->num_particles, size_t( 4 ) ) / 4;
		for( size_t i = 0; i < active_chunks; i++ ) {
			UpdateParticleChunk( ps, &ps->chunks[ i ], ps->acceleration, dt );
		}
	}

	ZoneScopedN( "Delete expired particles" );

	// delete expired particles
	for( size_t i = 0; i < ps->num_particles; i++ ) {
		ParticleChunk & chunk = ps->chunks[ i / 4 ];
		size_t chunk_offset = i % 4;

		if( chunk.t[ chunk_offset ] > chunk.lifetime[ chunk_offset ] ) {
			ps->num_particles--;
			i--;

			ParticleChunk & swap_chunk = ps->chunks[ ps->num_particles / 4 ];
			size_t swap_offset = ps->num_particles % 4;

			Swap2( &chunk.t[ chunk_offset ], &swap_chunk.t[ swap_offset ] );
			Swap2( &chunk.lifetime[ chunk_offset ], &swap_chunk.lifetime[ swap_offset ] );

			Swap2( &chunk.position_x[ chunk_offset ], &swap_chunk.position_x[ swap_offset ] );
			Swap2( &chunk.position_y[ chunk_offset ], &swap_chunk.position_y[ swap_offset ] );
			Swap2( &chunk.position_z[ chunk_offset ], &swap_chunk.position_z[ swap_offset ] );

			Swap2( &chunk.velocity_x[ chunk_offset ], &swap_chunk.velocity_x[ swap_offset ] );
			Swap2( &chunk.velocity_y[ chunk_offset ], &swap_chunk.velocity_y[ swap_offset ] );
			Swap2( &chunk.velocity_z[ chunk_offset ], &swap_chunk.velocity_z[ swap_offset ] );

			Swap2( &chunk.dvelocity[ chunk_offset ], &swap_chunk.dvelocity[ swap_offset ] );

			Swap2( &chunk.color_r[ chunk_offset ], &swap_chunk.color_r[ swap_offset ] );
			Swap2( &chunk.color_g[ chunk_offset ], &swap_chunk.color_g[ swap_offset ] );
			Swap2( &chunk.color_b[ chunk_offset ], &swap_chunk.color_b[ swap_offset ] );
			Swap2( &chunk.color_a[ chunk_offset ], &swap_chunk.color_a[ swap_offset ] );

			Swap2( &chunk.dcolor_r[ chunk_offset ], &swap_chunk.dcolor_r[ swap_offset ] );
			Swap2( &chunk.dcolor_g[ chunk_offset ], &swap_chunk.dcolor_g[ swap_offset ] );
			Swap2( &chunk.dcolor_b[ chunk_offset ], &swap_chunk.dcolor_b[ swap_offset ] );
			Swap2( &chunk.dcolor_a[ chunk_offset ], &swap_chunk.dcolor_a[ swap_offset ] );

			Swap2( &chunk.size[ chunk_offset ], &swap_chunk.size[ swap_offset ] );
			Swap2( &chunk.dsize[ chunk_offset ], &swap_chunk.dsize[ swap_offset ] );
		}
	}
}

void DrawParticleSystem( ParticleSystem * ps ) {
	if( ps->num_particles == 0 )
		return;

	ZoneScoped;

	size_t active_chunks = AlignPow2( ps->num_particles, size_t( 4 ) ) / 4;
	for( size_t i = 0; i < active_chunks; i++ ) {
		const ParticleChunk & chunk = ps->chunks[ i ];
		for( int j = 0; j < 4; j++ ) {
			ps->vb_memory[ i * 4 + j ].position = Vec3( chunk.position_x[ j ], chunk.position_y[ j ], chunk.position_z[ j ] );
			ps->vb_memory[ i * 4 + j ].scale = chunk.size[ j ];
			ps->vb_memory[ i * 4 + j ].t = chunk.t[ j ] / chunk.lifetime[ j ];
			Vec4 color = Vec4( chunk.color_r[ j ], chunk.color_g[ j ], chunk.color_b[ j ], chunk.color_a[ j ] );
			ps->vb_memory[ i * 4 + j ].color = RGBA8( color );
		}
	}

	WriteVertexBuffer( ps->vb, ps->vb_memory, ps->num_particles * sizeof( GPUParticle ) );

	DrawInstancedParticles( ps->mesh, ps->vb, ps->material, ps->gradient, ps->blend_func, ps->num_particles );
}

void DrawParticles() {
	float dt = cls.frametime / 1000.0f;
	UpdateParticleSystem( &cgs.ions, dt );
	UpdateParticleSystem( &cgs.SMGsparks, dt );
	UpdateParticleSystem( &cgs.sparks, dt );
	UpdateParticleSystem( &cgs.smoke, dt );
	DrawParticleSystem( &cgs.ions );
	DrawParticleSystem( &cgs.SMGsparks );
	DrawParticleSystem( &cgs.sparks );
	DrawParticleSystem( &cgs.smoke );
}

static void EmitParticle( ParticleSystem * ps, float lifetime, Vec3 position, Vec3 velocity, float dvelocity, Vec4 color, Vec4 dcolor, float size, float dsize ) {
	if( ps->num_particles == ps->chunks.n * 4 )
		return;

	ParticleChunk & chunk = ps->chunks[ ps->num_particles / 4 ];
	size_t i = ps->num_particles % 4;

	chunk.t[ i ] = 0.0f;
	chunk.lifetime[ i ] = lifetime;

	chunk.position_x[ i ] = position.x;
	chunk.position_y[ i ] = position.y;
	chunk.position_z[ i ] = position.z;

	chunk.velocity_x[ i ] = velocity.x;
	chunk.velocity_y[ i ] = velocity.y;
	chunk.velocity_z[ i ] = velocity.z;

	chunk.dvelocity[ i ] = dvelocity;

	chunk.color_r[ i ] = color.x;
	chunk.color_g[ i ] = color.y;
	chunk.color_b[ i ] = color.z;
	chunk.color_a[ i ] = color.w;

	chunk.dcolor_r[ i ] = dcolor.x;
	chunk.dcolor_g[ i ] = dcolor.y;
	chunk.dcolor_b[ i ] = dcolor.z;
	chunk.dcolor_a[ i ] = dcolor.w;

	chunk.size[ i ] = size;
	chunk.dsize[ i ] = dsize;

	ps->num_particles++;
}

static float SampleRandomDistribution( RNG * rng, RandomDistribution dist ) {
	if( dist.type == RandomDistributionType_Uniform ) {
		return random_float11( rng ) * dist.uniform;
	}

	return SampleNormalDistribution( rng ) * dist.sigma;
}

static void EmitParticle( ParticleSystem * ps, const ParticleEmitter & emitter, float t ) {
	float lifetime = Max2( 0.0f, emitter.lifetime + SampleRandomDistribution( &cls.rng, emitter.lifetime_distribution ) );

	Vec3 position = emitter.position;

	switch( emitter.position_distribution.type ) {
		case RandomDistribution3DType_Sphere: {
			position += UniformSampleInsideSphere( &cls.rng ) * emitter.position_distribution.sphere.radius;
		} break;

		case RandomDistribution3DType_Disk: {
			Vec2 p = UniformSampleDisk( &cls.rng );
			position += emitter.position_distribution.disk.radius * Vec3( p, 0.0f );
			// TODO: emitter.position_disk.normal;
		} break;

		case RandomDistribution3DType_Line: {
			position = Lerp( position, t, emitter.position_distribution.line.end );
		} break;
	}

	Vec3 dir;

	if( emitter.use_cone_direction ) {
		Mat4 dir_transform = TransformKToDir( emitter.direction_cone.normal );
		dir = ( dir_transform * Vec4( UniformSampleCone( &cls.rng, DEG2RAD( emitter.direction_cone.theta ) ), 0.0f ) ).xyz();
	}
	else {
		dir = UniformSampleSphere( &cls.rng );
	}

	float speed = emitter.start_speed + SampleRandomDistribution( &cls.rng, emitter.speed_distribution );
	float dspeed = ( emitter.end_speed - emitter.start_speed ) / lifetime;

	Vec4 color = emitter.start_color;
	color.x += SampleRandomDistribution( &cls.rng, emitter.red_distribution );
	color.y += SampleRandomDistribution( &cls.rng, emitter.green_distribution );
	color.z += SampleRandomDistribution( &cls.rng, emitter.blue_distribution );
	color.w += SampleRandomDistribution( &cls.rng, emitter.alpha_distribution );
	color = Clamp01( color );

	Vec4 dcolor = Vec4( emitter.end_color - emitter.start_color.xyz(), -color.w ) / lifetime;

	float size = Max2( 0.0f, emitter.start_size + SampleRandomDistribution( &cls.rng, emitter.size_distribution ) );
	float dsize = ( emitter.end_size - emitter.start_size ) / lifetime;

	EmitParticle( ps, lifetime, position, dir * speed, dspeed, color, dcolor, size, dsize );
}

static void EmitParticles( ParticleSystem * ps, const ParticleEmitter & emitter, float dt ) {
	ZoneScoped;

	float p = emitter.emission_rate > 0 ? emitter.emission_rate * dt : emitter.n;
	u32 n = u32( p );
	float remaining_p = p - n;

	for( u32 i = 0; i < n; i++ ) {
		float t = float( i ) / ( p + 1.0f );
		EmitParticle( ps, emitter, t );
	}

	if( random_p( &cls.rng, remaining_p ) ) {
		EmitParticle( ps, emitter, p / ( p + 1.0f ) );
	}
}

void EmitParticles( ParticleSystem * ps, const ParticleEmitter & emitter ) {
	EmitParticles( ps, emitter, cls.frametime / 1000.0f );
}

enum ParticleEmitterVersion : u32 {
	ParticleEmitterVersion_First,
};

static void Serialize( SerializationBuffer * buf, SphereDistribution & sphere ) { *buf & sphere.radius; }
static void Serialize( SerializationBuffer * buf, ConeDistribution & cone ) { *buf & cone.normal & cone.theta; }
static void Serialize( SerializationBuffer * buf, DiskDistribution & disk ) { *buf & disk.normal & disk.radius; }
static void Serialize( SerializationBuffer * buf, LineDistribution & line ) { *buf & line.end; }

static void Serialize( SerializationBuffer * buf, RandomDistribution & dist ) {
	*buf & dist.type;
	if( dist.type == RandomDistributionType_Uniform )
		*buf & dist.uniform;
	else
		*buf & dist.sigma;
}

static void Serialize( SerializationBuffer * buf, RandomDistribution3D & dist ) {
	*buf & dist.type;
	if( dist.type == RandomDistribution3DType_Sphere )
		*buf & dist.sphere;
	else if( dist.type == RandomDistribution3DType_Disk )
		*buf & dist.disk;
	else
		*buf & dist.line;
}

static void Serialize( SerializationBuffer * buf, ParticleEmitter & emitter ) {
	u32 version = ParticleEmitterVersion_First;
	*buf & version;

	*buf & emitter.position & emitter.position_distribution;
	*buf & emitter.use_cone_direction;

	if( emitter.use_cone_direction ) {
		*buf & emitter.direction_cone;
	}

	*buf & emitter.start_color & emitter.end_color & emitter.red_distribution & emitter.green_distribution & emitter.blue_distribution & emitter.alpha_distribution;

	*buf & emitter.start_size & emitter.end_size & emitter.size_distribution;

	*buf & emitter.lifetime & emitter.lifetime_distribution;

	*buf & emitter.emission_rate & emitter.n;
}

/*
 * particle editor
 */

static ParticleSystem editor_ps = { };
static ParticleEmitter editor_emitter;
static char editor_material_name[ 256 ];
static char editor_gradient_name[ 256 ];
static bool editor_one_shot;
static bool editor_blend;

void InitParticleEditor() {
	strcpy( editor_material_name, "$particle" );
	strcpy( editor_gradient_name, "$whiteimage" );
	editor_one_shot = false;
	editor_blend = false;

	editor_ps = NewParticleSystem( sys_allocator, 8192, FindMaterial( StringHash( ( const char * ) editor_material_name ) ) );
	editor_ps.gradient = FindMaterial( StringHash( ( const char * ) editor_gradient_name ) );
	editor_ps.blend_func = editor_blend ? BlendFunc_Blend : BlendFunc_Add;
	editor_emitter = { };

	editor_emitter.start_speed = 400.0f;
	editor_emitter.end_speed = 400.0f;
	editor_emitter.direction_cone.normal = Vec3( 0, 0, 1 );
	editor_emitter.direction_cone.theta = 90.0f;
	editor_emitter.start_color = vec4_white;
	editor_emitter.end_color = vec4_white.xyz();
	editor_emitter.start_size = 16.0f;
	editor_emitter.end_size = 16.0f;
	editor_emitter.lifetime = 1.0f;
	editor_emitter.emission_rate = 1000;
}

void ShutdownParticleEditor() {
	DeleteParticleSystem( sys_allocator, editor_ps );
}

void ResetParticleEditor() {
	DeleteParticleSystem( sys_allocator, editor_ps );
	editor_ps = NewParticleSystem( sys_allocator, 8192, FindMaterial( StringHash( ( const char * ) editor_material_name ) ) );
	editor_ps.gradient = FindMaterial( StringHash( ( const char * ) editor_gradient_name ) );
	editor_ps.blend_func = editor_blend ? BlendFunc_Blend : BlendFunc_Add;
}

static void RandomDistributionEditor( const char * id, RandomDistribution * dist, float range ) {
	constexpr const char * names[] = { "Uniform", "Normal" };

	TempAllocator temp = cls.frame_arena.temp();

	if( ImGui::BeginCombo( temp( "Distribution##{}", id ), names[ dist->type ] ) ) {
		for( int i = 0; i < 2; i++ ) {
			if( ImGui::Selectable( names[ i ], i == dist->type ) )
				dist->type = RandomDistributionType( i );
			if( i == dist->type )
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	switch( dist->type ) {
		case RandomDistributionType_Uniform:
			ImGui::SliderFloat( temp( "Range##{}", id ), &dist->uniform, 0, range, "%.2f" );
			break;

		case RandomDistributionType_Normal:
			ImGui::SliderFloat( temp( "Stddev##{}", id ), &dist->sigma, 0, 8, "%.2f" );
			break;
	}
}

void DrawParticleEditor() {
	TempAllocator temp = cls.frame_arena.temp();

	bool emit = false;

	ImGui::PushFont( cls.console_font );
	ImGui::BeginChild( "Particle editor", ImVec2( 300, 0 ) );
	{
		ImGuiWindowFlags popup_flags = ( ImGuiWindowFlags_NoDecoration & ~ImGuiWindowFlags_NoTitleBar ) | ImGuiWindowFlags_NoMove;

		if( ImGui::Button( "Load..." ) ) {
			ImGui::OpenPopup( "Load" );
		}

		ImGui::SameLine();

		if( ImGui::Button( "Save..." ) ) {
			ImGui::OpenPopup( "Save" );
		}

		if( ImGui::BeginPopupModal( "Load", NULL, popup_flags ) ) {
			static char name[ 64 ];
			ImGui::PushItemWidth( 300 );
			if( ImGui::IsWindowAppearing() ) {
				ImGui::SetKeyboardFocusHere();
				strcpy( name, "" );
			}
			bool do_load = ImGui::InputText( "##loadpath", name, sizeof( name ), ImGuiInputTextFlags_EnterReturnsTrue );
			ImGui::PopItemWidth();
			do_load = ImGui::Button( "Load" ) || do_load;

			if( do_load ) {
				Span< const char > data = AssetBinary( temp( "particles/{}.emitter", name ) ).cast< const char >();
				if( data.ptr != NULL ) {
					bool ok = Deserialize( editor_emitter, data.ptr, data.n );
					assert( ok );
				}

				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if( ImGui::Button( "Cancel" ) )
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		if( ImGui::BeginPopupModal( "Save", NULL, popup_flags ) ) {
			static char name[ 64 ];
			ImGui::PushItemWidth( 300 );
			if( ImGui::IsWindowAppearing() ) {
				ImGui::SetKeyboardFocusHere();
				strcpy( name, "" );
			}
			bool ok = ImGui::InputText( "##savepath", name, sizeof( name ), ImGuiInputTextFlags_EnterReturnsTrue );
			ImGui::PopItemWidth();
			ok = ImGui::Button( "Save" ) || ok;

			if( ok ) {
				char buf[ 1024 ];
				SerializationBuffer sb( SerializationMode_Serializing, buf, sizeof( buf ) );
				sb & editor_emitter;
				assert( !sb.error );
				// TODO: writefile can fail
				WriteFile( temp( "base/particles/{}.emitter", name ), buf, sb.cursor - buf );
				HotloadAssets( &temp );

				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if( ImGui::Button( "Cancel" ) )
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		ImGui::Separator();

		if( ImGui::InputText( "Material", editor_material_name, sizeof( editor_material_name ) ) ) {
			ResetParticleEditor();
		}

		if( ImGui::InputText( "Gradient material", editor_gradient_name, sizeof( editor_gradient_name ) ) ) {
			ResetParticleEditor();
		}

		ImGui::Checkbox( "Blend", &editor_blend );
		editor_ps.blend_func = editor_blend ? BlendFunc_Blend : BlendFunc_Add;

		ImGui::Separator();

		constexpr const char * position_distribution_names[] = { "Sphere", "Disk", "Line" };
		if( ImGui::BeginCombo( "Position distribution", position_distribution_names[ editor_emitter.position_distribution.type ] ) ) {
			for( int i = 0; i < 3; i++ ) {
				if( ImGui::Selectable( position_distribution_names[ i ], i == editor_emitter.position_distribution.type ) )
					editor_emitter.position_distribution.type = RandomDistribution3DType( i );
				if( i == editor_emitter.position_distribution.type )
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		editor_emitter.position = Vec3( 0 );

		switch( editor_emitter.position_distribution.type ) {
			case RandomDistribution3DType_Sphere:
				ImGui::SliderFloat( "Radius", &editor_emitter.position_distribution.sphere.radius, 0, 100, "%.2f" );
				break;

			case RandomDistribution3DType_Disk:
				ImGui::SliderFloat( "Radius", &editor_emitter.position_distribution.disk.radius, 0, 100, "%.2f" );
				break;

			case RandomDistribution3DType_Line:
				editor_emitter.position = Vec3( 0, -300, 0 );
				editor_emitter.position_distribution.line.end = Vec3( 0, 300, 0 );
				break;
		}

		ImGui::Separator();

		ImGui::Checkbox( "Direction cone?", &editor_emitter.use_cone_direction );

		if( editor_emitter.use_cone_direction ) {
			ImGui::SliderFloat( "Angle", &editor_emitter.direction_cone.theta, 0, 180, "%.2f" );
		}

		ImGui::SliderFloat( "Start speed", &editor_emitter.start_speed, 0, 1000, "%.2f" );
		ImGui::SliderFloat( "End speed", &editor_emitter.end_speed, 0, 1000, "%.2f" );

		ImGui::Separator();

		ImGui::ColorEdit4( "Start color", editor_emitter.start_color.ptr() );
		ImGui::ColorEdit3( "End color", editor_emitter.end_color.ptr() );

		if( ImGui::TreeNodeEx( "Start color randomness", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoAutoOpenOnLog ) ) {
			ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 0, 0, 255 ) );
			RandomDistributionEditor( "r", &editor_emitter.red_distribution, 1.0f );
			ImGui::PopStyleColor();

			ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 255, 0, 255 ) );
			RandomDistributionEditor( "g", &editor_emitter.green_distribution, 1.0f );
			ImGui::PopStyleColor();

			ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 255, 255 ) );
			RandomDistributionEditor( "b", &editor_emitter.blue_distribution, 1.0f );
			ImGui::PopStyleColor();

			RandomDistributionEditor( "a", &editor_emitter.alpha_distribution, 1.0f );

			ImGui::TreePop();
		}

		ImGui::Separator();

		ImGui::SliderFloat( "Start size", &editor_emitter.start_size, 0, 256, "%.2f" );
		ImGui::SliderFloat( "End size", &editor_emitter.end_size, 0, 256, "%.2f" );
		RandomDistributionEditor( "size", &editor_emitter.size_distribution, editor_emitter.start_size );

		ImGui::Separator();

		ImGui::SliderFloat( "Lifetime", &editor_emitter.lifetime, 0, 10, "%.2f" );
		RandomDistributionEditor( "lifetime", &editor_emitter.lifetime_distribution, editor_emitter.lifetime );

		ImGui::Separator();

		ImGui::Checkbox( "One shot mode", &editor_one_shot );

		if( editor_one_shot ) {
			ImGui::SliderFloat( "Particle count", &editor_emitter.n, 0, 500, "%.2f" );
			editor_emitter.emission_rate = 0;
			emit = ImGui::Button( "Go" );
		}
		else {
			ImGui::SliderFloat( "Emission rate", &editor_emitter.emission_rate, 0, 500, "%.2f" );
		}
	}
	ImGui::EndChild();
	ImGui::PopFont();

	RendererSetView( Vec3( -400, 0, 400 ), EulerDegrees3( 45, 0, 0 ), 90 );

	float dt = cls.frametime / 1000.0f;

	if( !editor_one_shot || emit || editor_ps.num_particles == 0 ) {
		EmitParticles( &editor_ps, editor_emitter, dt );
	}

	UpdateParticleSystem( &editor_ps, dt );
	DrawParticleSystem( &editor_ps );
}
