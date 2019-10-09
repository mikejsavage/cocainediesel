/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// cg_effects.c -- entity effects parsing and management

#include "cgame/cg_local.h"
#include "client/client.h"

struct Particle {
	Vec3 origin;
	Vec3 velocity;
	bool gravity;

	RGBA8 color;
	float dalpha;

	float size;

	s64 spawn_time;

	Texture texture;
};

void AddParticle2( const Particle & p );

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/

typedef struct cdlight_s
{
	vec3_t color;
	vec3_t origin;
	float radius;
} cdlight_t;

#define MAX_DLIGHTS 256
static cdlight_t cg_dlights[MAX_DLIGHTS];
static int cg_numDlights;

/*
* CG_ClearDlights
*/
static void CG_ClearDlights( void ) {
	memset( cg_dlights, 0, sizeof( cg_dlights ) );
	cg_numDlights = 0;
}

/*
* CG_AllocDlight
*/
static void CG_AllocDlight( vec3_t origin, float radius, float r, float g, float b ) {
	cdlight_t *dl;

	if( radius <= 0 ) {
		return;
	}
	if( cg_numDlights == MAX_DLIGHTS ) {
		return;
	}

	dl = &cg_dlights[cg_numDlights++];
	dl->radius = radius;
	VectorCopy( origin, dl->origin );
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}

void CG_AddLightToScene( vec3_t org, float radius, float r, float g, float b ) {
	CG_AllocDlight( org, radius, r, g, b );
}

/*
* CG_AddDlights
*/
void CG_AddDlights( void ) {
	int i;
	cdlight_t *dl;

	// for( i = 0, dl = cg_dlights; i < cg_numDlights; i++, dl++ )
	// 	trap_R_AddLightToScene( dl->origin, dl->radius, dl->color[0], dl->color[1], dl->color[2] );

	cg_numDlights = 0;
}

/*
==============================================================

PLAYER SHADOWS MANAGEMENT

==============================================================
*/

#define MAX_PLAYERSHADOW_VERTS 128
#define MAX_PLAYERSHADOW_FRAGMENTS 64

struct PlayerShadow {
	vec3_t origin;
	vec3_t mins, maxs;
	int entNum;

	vec4_t verts[MAX_PLAYERSHADOW_VERTS];
	vec4_t norms[MAX_PLAYERSHADOW_VERTS];
	vec2_t stcoords[MAX_PLAYERSHADOW_VERTS];
	byte_vec4_t colors[MAX_PLAYERSHADOW_VERTS];
};

static PlayerShadow player_shadows[ 128 ];
static size_t num_player_shadows = 0;

static void CG_AddPlayerShadow( vec3_t origin, vec3_t normal, float radius, float alpha, PlayerShadow * player_shadow ) {
	vec3_t axis[3];
	fragment_t fragments[MAX_PLAYERSHADOW_FRAGMENTS];
	poly_t poly;
	vec4_t verts[MAX_PLAYERSHADOW_VERTS];

	if( VectorCompare( normal, vec3_origin ) ) {
		return;
	}

	// calculate orientation matrix
	VectorNormalize2( normal, axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], 0 );
	CrossProduct( axis[0], axis[2], axis[1] );

	// int numfragments = trap_R_GetClippedFragments( origin, radius, axis, MAX_PLAYERSHADOW_VERTS, verts, MAX_PLAYERSHADOW_FRAGMENTS, fragments );
	int numfragments = 0;
	if( numfragments == 0 )
		return;

	// clamp and scale colors
	byte_vec4_t color;
	color[0] = ( uint8_t )( 255 );
	color[1] = ( uint8_t )( 255 );
	color[2] = ( uint8_t )( 255 );
	color[3] = ( uint8_t )( alpha * 255 );
	int c = *( int * )color;

	radius = 0.5f / radius;
	VectorScale( axis[1], radius, axis[1] );
	VectorScale( axis[2], radius, axis[2] );

	memset( &poly, 0, sizeof( poly ) );

	int nverts = 0;
	for( int i = 0; i < numfragments; i++ ) {
		fragment_t *fr = &fragments[i];
		if( nverts + fr->numverts > MAX_PLAYERSHADOW_VERTS ) {
			return;
		}
		if( fr->numverts <= 0 ) {
			continue;
		}

		poly.material = cgs.media.shaderPlayerShadow;
		poly.verts = &player_shadow->verts[nverts];
		poly.normals = &player_shadow->norms[nverts];
		poly.stcoords = &player_shadow->stcoords[nverts];
		poly.colors = &player_shadow->colors[nverts];
		poly.numverts = fr->numverts;
		nverts += fr->numverts;

		for( int j = 0; j < fr->numverts; j++ ) {
			vec3_t v;

			Vector4Copy( verts[fr->firstvert + j], poly.verts[j] );
			VectorCopy( axis[0], poly.normals[j] ); poly.normals[j][3] = 0;
			VectorSubtract( poly.verts[j], origin, v );
			poly.stcoords[j][0] = DotProduct( v, axis[1] ) + 0.5f;
			poly.stcoords[j][1] = DotProduct( v, axis[2] ) + 0.5f;
			*( int * )poly.colors[j] = c;
		}

		// trap_R_AddPolyToScene( &poly );
	}
}

static void CG_ClearPlayerShadows() {
	num_player_shadows = 0;
	memset( player_shadows, 0, sizeof( player_shadows ) );
}

void CG_AllocPlayerShadow( int entNum, const vec3_t origin, const vec3_t mins, const vec3_t maxs ) {
	if( num_player_shadows == ARRAY_COUNT( player_shadows ) )
		return;

	// Kill if behind the view or if too far away
	vec3_t dir;
	VectorSubtract( origin, cg.view.origin, dir );
	float dist = VectorNormalize2( dir, dir ) * cg.view.fracDistFOV;
	if( dist > 4096 )
		return;

	if( DotProduct( dir, &cg.view.axis[AXIS_FORWARD] ) < 0 )
		return;

	PlayerShadow * shadow = &player_shadows[ num_player_shadows ];
	num_player_shadows++;

	VectorCopy( origin, shadow->origin );
	VectorCopy( mins, shadow->mins );
	VectorCopy( maxs, shadow->maxs );
	shadow->entNum = entNum;
}

#define SHADOW_LERP_DISTANCE 256
#define SHADOW_MAX_SIZE 24
#define SHADOW_MIN_SIZE 4

void CG_AddPlayerShadows() {
	vec3_t down, end;

	VectorSet( down, 0, 0, -1 );

	for( size_t i = 0; i < num_player_shadows; i++ ) {
		// move the point we will project close to the bottom of the bbox (so shadow doesn't dance much to the sides)
		PlayerShadow * shadow = &player_shadows[i];
		VectorMA( shadow->origin, 1024, down, end );

		trace_t trace;
		CG_Trace( &trace, shadow->origin, vec3_origin, vec3_origin, end, shadow->entNum, MASK_OPAQUE );

		float frac = trace.fraction * 1024.0f / float( SHADOW_LERP_DISTANCE );
		if( frac > 1 )
			frac = 1;
		float radius = SHADOW_MAX_SIZE - frac * ( SHADOW_MAX_SIZE - SHADOW_MIN_SIZE );
		float alpha = 0.8f - frac * 0.4f;

		CG_AddPlayerShadow( trace.endpos, trace.plane.normal, radius, alpha, shadow );
	}

	num_player_shadows = 0;
}

/*
==============================================================

TEMPORARY (ONE-FRAME) DECALS

==============================================================
*/

#define MAX_TEMPDECALS              32      // in fact, a semi-random multiplier
#define MAX_TEMPDECAL_VERTS         128
#define MAX_TEMPDECAL_FRAGMENTS     64

static unsigned int cg_numDecalVerts = 0;

/*
* CG_ClearFragmentedDecals
*/
void CG_ClearFragmentedDecals( void ) {
	cg_numDecalVerts = 0;
}

/*
* CG_AddFragmentedDecal
*/
void CG_AddFragmentedDecal( vec3_t origin, vec3_t dir, float orient, float radius,
							float r, float g, float b, float a, const Material * material ) {
	int i, j, c;
	vec3_t axis[3];
	byte_vec4_t color;
	fragment_t *fr, fragments[MAX_TEMPDECAL_FRAGMENTS];
	int numfragments;
	poly_t poly;
	vec4_t verts[MAX_PLAYERSHADOW_VERTS];
	static vec4_t t_verts[MAX_TEMPDECAL_VERTS * MAX_TEMPDECALS];
	static vec4_t t_norms[MAX_TEMPDECAL_VERTS * MAX_TEMPDECALS];
	static vec2_t t_stcoords[MAX_TEMPDECAL_VERTS * MAX_TEMPDECALS];
	static byte_vec4_t t_colors[MAX_TEMPDECAL_VERTS * MAX_TEMPDECALS];

	if( radius <= 0 || VectorCompare( dir, vec3_origin ) ) {
		return; // invalid

	}

	// calculate orientation matrix
	VectorNormalize2( dir, axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], orient );
	CrossProduct( axis[0], axis[2], axis[1] );

	// numfragments = trap_R_GetClippedFragments( origin, radius, axis, // clip it
	// 										   MAX_PLAYERSHADOW_VERTS, verts, MAX_TEMPDECAL_FRAGMENTS, fragments );
	numfragments = 0;

	// no valid fragments
	if( !numfragments ) {
		return;
	}

	// clamp and scale colors
	if( r < 0 ) {
		r = 0;
	} else if( r > 1 ) {
		r = 255;
	} else {
		r *= 255;
	}
	if( g < 0 ) {
		g = 0;
	} else if( g > 1 ) {
		g = 255;
	} else {
		g *= 255;
	}
	if( b < 0 ) {
		b = 0;
	} else if( b > 1 ) {
		b = 255;
	} else {
		b *= 255;
	}
	if( a < 0 ) {
		a = 0;
	} else if( a > 1 ) {
		a = 255;
	} else {
		a *= 255;
	}

	color[0] = ( uint8_t )( r );
	color[1] = ( uint8_t )( g );
	color[2] = ( uint8_t )( b );
	color[3] = ( uint8_t )( a );
	c = *( int * )color;

	radius = 0.5f / radius;
	VectorScale( axis[1], radius, axis[1] );
	VectorScale( axis[2], radius, axis[2] );

	memset( &poly, 0, sizeof( poly ) );

	for( i = 0, fr = fragments; i < numfragments; i++, fr++ ) {
		if( fr->numverts <= 0 ) {
			continue;
		}
		if( cg_numDecalVerts + (unsigned)fr->numverts > ARRAY_COUNT( t_verts ) ) {
			return;
		}

		poly.material = material;
		poly.verts = &t_verts[cg_numDecalVerts];
		poly.normals = &t_norms[cg_numDecalVerts];
		poly.stcoords = &t_stcoords[cg_numDecalVerts];
		poly.colors = &t_colors[cg_numDecalVerts];
		poly.numverts = fr->numverts;
		cg_numDecalVerts += (unsigned)fr->numverts;

		for( j = 0; j < fr->numverts; j++ ) {
			vec3_t v;

			Vector4Copy( verts[fr->firstvert + j], poly.verts[j] );
			VectorCopy( axis[0], poly.normals[j] ); poly.normals[j][3] = 0;
			VectorSubtract( poly.verts[j], origin, v );
			poly.stcoords[j][0] = DotProduct( v, axis[1] ) + 0.5f;
			poly.stcoords[j][1] = DotProduct( v, axis[2] ) + 0.5f;
			*( int * )poly.colors[j] = c;
		}

		// trap_R_AddPolyToScene( &poly );
	}
}

/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/

typedef struct particle_s
{
	float time;

	vec3_t org;
	vec3_t vel;
	vec3_t accel;
	vec3_t color;
	float alpha;
	float alphavel;
	float scale;

	poly_t poly;
	vec4_t pVerts[4];
	vec2_t pStcoords[4];
	byte_vec4_t pColor[4];

	const Material * material;
} cparticle_t;

#define PARTICLE_GRAVITY    500

#define MAX_PARTICLES       4096

static vec3_t avelocities[NUMVERTEXNORMALS];

cparticle_t particles[MAX_PARTICLES];
int cg_numparticles;

/*
* CG_ClearParticles
*/
static void CG_ClearParticles( void ) {
	int i;
	cparticle_t *p;

	cg_numparticles = 0;
	memset( particles, 0, sizeof( cparticle_t ) * MAX_PARTICLES );

	for( i = 0, p = particles; i < MAX_PARTICLES; i++, p++ ) {
		p->pStcoords[0][0] = 0; p->pStcoords[0][1] = 1;
		p->pStcoords[1][0] = 0; p->pStcoords[1][1] = 0;
		p->pStcoords[2][0] = 1; p->pStcoords[2][1] = 0;
		p->pStcoords[3][0] = 1; p->pStcoords[3][1] = 1;
	}
}

#define CG_InitParticle( p, s, a, r, g, b, h ) \
	( \
		( p )->time = cg.time, \
		( p )->scale = ( s ), \
		( p )->alpha = ( a ), \
		( p )->color[0] = ( r ), \
		( p )->color[1] = ( g ), \
		( p )->color[2] = ( b ), \
		( p )->material = ( h ) \
	)

/*
* CG_ParticleEffect
*
* Wall impact puffs
*/
void CG_ParticleEffect( const vec3_t org, const vec3_t dir, float r, float g, float b, int count ) {
	int j;
	cparticle_t *p;
	float d;

	if( !cg_particles->integer ) {
		return;
	}

	if( cg_numparticles + count > MAX_PARTICLES ) {
		count = MAX_PARTICLES - cg_numparticles;
	}
	for( p = &particles[cg_numparticles], cg_numparticles += count; count > 0; count--, p++ ) {
		CG_InitParticle( p, 0.75, 1, r, g, b, NULL );

		d = rand() & 31;
		for( j = 0; j < 3; j++ ) {
			p->org[j] = org[j] + ( ( rand() & 7 ) - 4 ) + d * dir[j];
			p->vel[j] = crandom() * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / ( 0.5 + random() * 0.3 );
	}
}

/*
* CG_ParticleEffect2
*/
void CG_ParticleEffect2( const vec3_t org, const vec3_t dir, float r, float g, float b, int count ) {
	int j;
	float d;
	cparticle_t *p;

	if( !cg_particles->integer ) {
		return;
	}

	if( cg_numparticles + count > MAX_PARTICLES ) {
		count = MAX_PARTICLES - cg_numparticles;
	}
	for( p = &particles[cg_numparticles], cg_numparticles += count; count > 0; count--, p++ ) {
		CG_InitParticle( p, 0.75, 1, r, g, b, NULL );

		d = rand() & 7;
		for( j = 0; j < 3; j++ ) {
			p->org[j] = org[j] + ( ( rand() & 7 ) - 4 ) + d * dir[j];
			p->vel[j] = crandom() * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / ( 0.5 + random() * 0.3 );
	}
}

/*
* CG_ParticleExplosionEffect
*/
void CG_ParticleExplosionEffect( const vec3_t org, const vec3_t dir, float r, float g, float b, int count ) {
	int j;
	cparticle_t *p;
	float d;

	if( !cg_particles->integer ) {
		return;
	}

	if( cg_numparticles + count > MAX_PARTICLES ) {
		count = MAX_PARTICLES - cg_numparticles;
	}
	for( p = &particles[cg_numparticles], cg_numparticles += count; count > 0; count--, p++ ) {
		CG_InitParticle( p, 0.75, 1, r + random() * 0.1, g + random() * 0.1, b + random() * 0.1, NULL );

		d = rand() & 31;
		for( j = 0; j < 3; j++ ) {
			p->org[j] = org[j] + ( ( rand() & 7 ) - 4 ) + d * dir[j];
			p->vel[j] = crandom() * 400;
		}

		//p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / ( 0.7 + random() * 0.25 );
	}
}

/*
* CG_ImpactPuffParticles
* Wall impact puffs
*/
void CG_ImpactPuffParticles( const vec3_t org, const vec3_t dir, int count, float scale, float r, float g, float b, float a, const Material * material ) {
	int j;
	float d;
	cparticle_t *p;

	if( !cg_particles->integer ) {
		return;
	}

	if( cg_numparticles + count > MAX_PARTICLES ) {
		count = MAX_PARTICLES - cg_numparticles;
	}
	for( p = &particles[cg_numparticles], cg_numparticles += count; count > 0; count--, p++ ) {
		CG_InitParticle( p, scale, a, r, g, b, material );

		d = rand() & 15;
		for( j = 0; j < 3; j++ ) {
			p->org[j] = org[j] + ( ( rand() & 7 ) - 4 ) + d * dir[j];
			p->vel[j] = dir[j] * 90 + crandom() * 40;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / ( 0.5 + random() * 0.3 );
	}
}

/*
* CG_HighVelImpactPuffParticles
* High velocity wall impact puffs
*/
void CG_HighVelImpactPuffParticles( const vec3_t org, const vec3_t dir, int count, float scale, float r, float g, float b, float a, const Material * material ) {
	int j;
	float d;
	cparticle_t *p;

	if( !cg_particles->integer ) {
		return;
	}

	if( cg_numparticles + count > MAX_PARTICLES ) {
		count = MAX_PARTICLES - cg_numparticles;
	}
	for( p = &particles[cg_numparticles], cg_numparticles += count; count > 0; count--, p++ ) {
		CG_InitParticle( p, scale, a, r, g, b, material );

		d = rand() & 15;
		for( j = 0; j < 3; j++ ) {
			p->org[j] = org[j] + ( ( rand() & 7 ) - 4 ) + d * dir[j];
			p->vel[j] = dir[j] * 180 + crandom() * 40;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY * 2;
		p->alphavel = -5.0 / ( 0.5 + random() * 0.3 );
	}
}

static Vec3 UniformSampleSphere( RNG * rng ) {
	float z = 1.0f - 2.0f * random_float01( rng );
	float r = sqrtf( Max2( 0.0f, 1.0f - z * z ) );
	float phi = 2.0f * float( M_PI ) * random_float01( rng );
	return Vec3( r * cosf( phi ), r * sinf( phi ), z );
}

void CG_EBIonsTrail( Vec3 start, Vec3 end, Vec4 color ) {
	constexpr int max_ions = 256;
	float distance_between_particles = 4.0f;

	float len = Length( end - start );
	Vec3 dir = Normalize( end - start );

	int n = int( len / distance_between_particles ) + 1;

	if( n > max_ions ) {
		distance_between_particles *= float( max_ions ) / n;
		n = max_ions;
	}

	for( int i = 0; i < n; i++ ) {
		Particle p;
		memset( &p, 0, sizeof( p ) );
		p.origin = start + dir * distance_between_particles * i;
		p.velocity = UniformSampleSphere( &cls.rng ) * 4;

		Vec4 random_color = color;
		random_color.x += random_float11( &cls.rng ) * 0.1f;
		random_color.y += random_float11( &cls.rng ) * 0.1f;
		random_color.z += random_float11( &cls.rng ) * 0.1f;
		p.color = RGBA8( Clamp01( random_color ) );

		p.dalpha = -1.0f / random_uniform_float( &cls.rng, 0.6f, 1.2f );

		p.size = 0.65f;

		p.texture = FindTexture( "gfx/misc/cartoon_smokepuff1" );

		AddParticle2( p );
	}
}

#define BEAMLENGTH      16

/*
* CG_FlyParticles
*/
static void CG_FlyParticles( const vec3_t origin, int count ) {
	int i, j;
	float angle, sp, sy, cp, cy;
	vec3_t forward, dir;
	float dist, ltime;
	cparticle_t *p;

	if( !cg_particles->integer ) {
		return;
	}

	if( count > NUMVERTEXNORMALS ) {
		count = NUMVERTEXNORMALS;
	}

	if( !avelocities[0][0] ) {
		for( i = 0; i < NUMVERTEXNORMALS; i++ )
			for( j = 0; j < 3; j++ )
				avelocities[i][j] = ( rand() & 255 ) * 0.01;
	}

	i = 0;
	ltime = (float)cg.time / 1000.0;

	count /= 2;
	if( cg_numparticles + count > MAX_PARTICLES ) {
		count = MAX_PARTICLES - cg_numparticles;
	}
	for( p = &particles[cg_numparticles], cg_numparticles += count; count > 0; count--, p++ ) {
		CG_InitParticle( p, 1, 1, 0, 0, 0, NULL );

		angle = ltime * avelocities[i][0];
		sy = sin( angle );
		cy = cos( angle );
		angle = ltime * avelocities[i][1];
		sp = sin( angle );
		cp = cos( angle );

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		dist = sin( ltime + i ) * 64;
		ByteToDir( i, dir );
		p->org[0] = origin[0] + dir[0] * dist + forward[0] * BEAMLENGTH;
		p->org[1] = origin[1] + dir[1] * dist + forward[1] * BEAMLENGTH;
		p->org[2] = origin[2] + dir[2] * dist + forward[2] * BEAMLENGTH;

		VectorClear( p->vel );
		VectorClear( p->accel );
		p->alphavel = -100;

		i += 2;
	}
}

/*
* CG_FlyEffect
*/
void CG_FlyEffect( centity_t *ent, const vec3_t origin ) {
	int n;
	int count;
	int starttime;

	if( !cg_particles->integer ) {
		return;
	}

	if( ent->fly_stoptime < cg.time ) {
		starttime = cg.time;
		ent->fly_stoptime = cg.time + 60000;
	} else {
		starttime = ent->fly_stoptime - 60000;
	}

	n = cg.time - starttime;
	if( n < 20000 ) {
		count = n * 162 / 20000.0;
	} else {
		n = ent->fly_stoptime - cg.time;
		if( n < 20000 ) {
			count = n * 162 / 20000.0;
		} else {
			count = 162;
		}
	}

	CG_FlyParticles( origin, count );
}

/*
* CG_AddParticles
*/
void CG_AddParticles( void ) {
	int i, j, k;
	float alpha;
	float time, time2;
	vec3_t org;
	vec3_t corner;
	byte_vec4_t color;
	int maxparticle, activeparticles;
	float alphaValues[MAX_PARTICLES];
	cparticle_t *p, *free_particles[MAX_PARTICLES];

	if( !cg_numparticles ) {
		return;
	}

	j = 0;
	maxparticle = -1;
	activeparticles = 0;

	for( i = 0, p = particles; i < cg_numparticles; i++, p++ ) {
		time = ( cg.time - p->time ) * 0.001f;
		alpha = alphaValues[i] = p->alpha + time * p->alphavel;

		if( alpha <= 0 ) { // faded out
			free_particles[j++] = p;
			continue;
		}

		maxparticle = i;
		activeparticles++;

		time2 = time * time * 0.5f;

		org[0] = p->org[0] + p->vel[0] * time + p->accel[0] * time2;
		org[1] = p->org[1] + p->vel[1] * time + p->accel[1] * time2;
		org[2] = p->org[2] + p->vel[2] * time + p->accel[2] * time2;

		color[0] = (uint8_t)( bound( 0, p->color[0], 1.0f ) * 255 );
		color[1] = (uint8_t)( bound( 0, p->color[1], 1.0f ) * 255 );
		color[2] = (uint8_t)( bound( 0, p->color[2], 1.0f ) * 255 );
		color[3] = (uint8_t)( bound( 0, alpha, 1.0f ) * 255 );

		corner[0] = org[0];
		corner[1] = org[1] - 0.5f * p->scale;
		corner[2] = org[2] - 0.5f * p->scale;

		Vector4Set( p->pVerts[0], corner[0], corner[1] + p->scale, corner[2] + p->scale, 1 );
		Vector4Set( p->pVerts[1], corner[0], corner[1], corner[2] + p->scale, 1 );
		Vector4Set( p->pVerts[2], corner[0], corner[1], corner[2], 1 );
		Vector4Set( p->pVerts[3], corner[0], corner[1] + p->scale, corner[2], 1 );
		for( k = 0; k < 4; k++ ) {
			Vector4Copy( color, p->pColor[k] );
		}

		p->poly.numverts = 4;
		p->poly.verts = p->pVerts;
		p->poly.stcoords = p->pStcoords;
		p->poly.colors = p->pColor;
		p->poly.material = p->material == NULL ? cgs.media.shaderParticle : p->material;

		// trap_R_AddPolyToScene( &p->poly );
	}

	i = 0;
	while( maxparticle >= activeparticles ) {
		*free_particles[i++] = particles[maxparticle--];

		while( maxparticle >= activeparticles ) {
			if( alphaValues[maxparticle] <= 0 ) {
				maxparticle--;
			} else {
				break;
			}
		}
	}

	cg_numparticles = activeparticles;
}

/*
* CG_ClearEffects
*/
void CG_ClearEffects( void ) {
	CG_ClearFragmentedDecals();
	CG_ClearParticles();
	CG_ClearDlights();
	CG_ClearPlayerShadows();
}

static constexpr size_t MAX_PARTICLES2 = 8192;
static Particle particles2[ MAX_PARTICLES2 ];
static size_t num_particles;

void InitParticles() {
	num_particles = 0;
}

void DrawParticles() {
	ZoneScoped;

	PipelineState pipeline;
	pipeline.shader = &shaders.particle;
	pipeline.pass = frame_static.transparent_pass;
	pipeline.blend_func = BlendFunc_Add;
	pipeline.write_depth = false;
	pipeline.set_uniform( "u_View", frame_static.view_uniforms );

	for( size_t i = 0; i < num_particles; i++ ) {
		Particle & p = particles2[ i ];
		float t = ( cg.time - p.spawn_time ) / 1000.0f;
		float alpha = p.color.a / 255.0f + p.dalpha * t;

		if( alpha <= 0 ) {
			num_particles--;
			Swap2( &p, &particles2[ num_particles ] );
			i--;
			continue;
		}

		Vec3 accel = p.gravity ? Vec3( 0, 0, -BASEGRAVITY ) : Vec3( 0 );
		Vec3 origin = p.origin + p.velocity * t + 0.5f * accel * t * t;

		Vec3 right = 0.5f * p.size * frame_static.V.row0().xyz();
		Vec3 up = 0.5f * p.size * frame_static.V.row1().xyz();

		Vec3 positions[] = {
			origin - right - up,
			origin + right - up,
			origin - right + up,
			origin + right + up,
		};

		Vec2 half_pixel = 0.5f / Vec2( p.texture.width, p.texture.height );
		Vec2 uvs[] = {
			Vec2( half_pixel.x, 1.0f - half_pixel.y ),
			Vec2( 1.0f - half_pixel.x, 1.0f - half_pixel.y ),
			Vec2( half_pixel.x, half_pixel.y ),
			Vec2( 1.0f - half_pixel.x, half_pixel.y ),
		};

		RGBA8 color = RGBA8( p.color.r, p.color.g, p.color.b, 255 * alpha );
		RGBA8 colors[] = { color, color, color, color };

		u16 base_index = DynamicMeshBaseIndex();
		u16 indices[] = { 0, 1, 2, 1, 3, 2 };
		for( u16 & idx : indices ) {
			idx += base_index;
		}

		DynamicMesh mesh = { };
		mesh.positions = positions;
		mesh.uvs = uvs;
		mesh.colors = colors;
		mesh.indices = indices;
		mesh.num_vertices = 4;
		mesh.num_indices = 6;

		pipeline.set_texture( "u_BaseTexture", p.texture );

		DrawDynamicMesh( pipeline, mesh );
	}
}

void AddParticle2( const Particle & p ) {
	if( num_particles == ARRAY_COUNT( particles2 ) )
		return;
	particles2[ num_particles ] = p;
	particles2[ num_particles ].spawn_time = cg.time;
	num_particles++;
}

void DrawBeam( Vec3 start, Vec3 end, float width, Vec4 color, const Material * material ) {
	Vec3 dir = Normalize( end - start );
	Vec3 forward = Normalize( start - frame_static.position );

	if( fabsf( Dot( dir, forward ) ) == 1.0f )
		return;

	Vec3 beam_across = Normalize( Cross( -forward, dir ) );

	Vec3 positions[] = {
		start + width * beam_across * 0.5f,
		start - width * beam_across * 0.5f,
		end + width * beam_across * 0.5f,
		end - width * beam_across * 0.5f,
	};

	Texture texture = material->textures[ 0 ].texture;
	float texture_aspect_ratio = float( texture.width ) / float( texture.height );
	float beam_aspect_ratio = Length( end - start ) / width;
	float repetitions = beam_aspect_ratio / texture_aspect_ratio;

	Vec2 half_pixel = 0.5f / Vec2( texture.width, texture.height );
	Vec2 uvs[] = {
		Vec2( half_pixel.x, half_pixel.y ),
		Vec2( half_pixel.x, 1.0f - half_pixel.y ),
		Vec2( repetitions - half_pixel.x, half_pixel.y ),
		Vec2( repetitions - half_pixel.x, 1.0f - half_pixel.y ),
	};

	constexpr RGBA8 colors[] = { rgba8_white, rgba8_white, rgba8_white, rgba8_white };

	u16 base_index = DynamicMeshBaseIndex();
	u16 indices[] = { 0, 1, 2, 1, 3, 2 };
	for( u16 & idx : indices ) {
		idx += base_index;
	}

	PipelineState pipeline = MaterialToPipelineState( material, color );
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
	beam.spawn_time = cg.time;
	beam.duration = duration;
	beam.start_fade_time = duration - fade_time;
}

void DrawPersistentBeams() {
	ZoneScoped;

	for( size_t i = 0; i < num_persistent_beams; i++ ) {
		PersistentBeam & beam = persistent_beams[ i ];
		float t = ( cg.time - beam.spawn_time ) / 1000.0f;
		float alpha;
		if( beam.start_fade_time != beam.duration )
			alpha = 1.0f - Clamp01( Unlerp( beam.start_fade_time, t, beam.duration ) );
		else
			alpha = t < beam.duration ? 1.0f : 0.0f;

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
