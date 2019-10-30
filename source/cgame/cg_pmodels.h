/*
Copyright (C) 2002-2003 Victor Luchits

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
// cg_pmodels.h -- local definitions for pmodels and view weapon

//=============================================================================
//
//							SPLITMODELS
//
//=============================================================================

extern cvar_t *cg_weaponFlashes;
extern cvar_t *cg_gunx;
extern cvar_t *cg_guny;
extern cvar_t *cg_gunz;
extern cvar_t *cg_debugPlayerModels;
extern cvar_t *cg_debugWeaponModels;
extern cvar_t *cg_gunbob;
extern cvar_t *cg_gun_fov;
extern cvar_t *cg_handOffset;

enum {
	WEAPANIM_NOANIM,
	WEAPANIM_STANDBY,
	WEAPANIM_ATTACK_WEAK,
	WEAPANIM_ATTACK_STRONG,
	WEAPANIM_WEAPDOWN,
	WEAPANIM_WEAPONUP,

	VWEAP_MAXANIMS
};

enum {
	WEAPMODEL_WEAPON,
	WEAPMODEL_FLASH,
	WEAPMODEL_HAND,
	WEAPMODEL_BARREL,

	VWEAP_MAXPARTS
};

// equivalent to pmodelinfo_t. Shared by different players, etc.
typedef struct weaponinfo_s {
	char name[MAX_QPATH];
	bool inuse;

	const Model *model[VWEAP_MAXPARTS]; // one weapon consists of several models

	int firstframe[VWEAP_MAXANIMS];         // animation script
	int lastframe[VWEAP_MAXANIMS];
	int loopingframes[VWEAP_MAXANIMS];
	unsigned int frametime[VWEAP_MAXANIMS];

	orientation_t tag_projectionsource;

	// handOffset
	vec3_t handpositionOrigin;
	vec3_t handpositionAngles;

	// flash
	int64_t flashTime;
	bool flashFade;
	float flashRadius;
	vec3_t flashColor;

	// barrel
	int64_t barrelTime;
	float barrelSpeed;

	const SoundEffect * sound_fire;
} weaponinfo_t;

enum {
	BASE_CHANNEL,
	EVENT_CHANNEL,
	PLAYERANIM_CHANNELS
};

typedef struct {
	int anim;
	int64_t startTimestamp;
} animstate_t;

struct PlayerModelAnimationSet {
	int parts[PMODEL_PARTS];
};

typedef struct {
	// animations in the mixer
	animstate_t curAnims[PMODEL_PARTS][PLAYERANIM_CHANNELS];
	PlayerModelAnimationSet pending[PLAYERANIM_CHANNELS];
} pmodel_animationstate_t;

enum PlayerSound {
	PlayerSound_Death,
	PlayerSound_Jump,
	PlayerSound_Pain25,
	PlayerSound_Pain50,
	PlayerSound_Pain75,
	PlayerSound_Pain100,
	PlayerSound_WallJump,
	PlayerSound_Dash,

	PlayerSound_Count
};

struct PlayerModelMetadata {
	struct Tag {
		u8 joint_idx;
		Mat4 transform;
	};

	struct AnimationClip {
		float start_time;
		float duration;
		float loop_from; // we only loop the last part of the animation
	};

	u64 name_hash;

	const Model * model;
	const SoundEffect * sounds[ PlayerSound_Count ];

	u8 upper_rotator_joints[ 2 ];
	u8 head_rotator_joint;
	u8 upper_root_joint;

	Tag tag_backpack;
	Tag tag_head;
	Tag tag_weapon;

	AnimationClip clips[ PMODEL_TOTAL_ANIMATIONS ];

	PlayerModelMetadata *next;
};

typedef struct {
	// static data
	const PlayerModelMetadata * metadata;

	// dynamic
	pmodel_animationstate_t animState;

	vec3_t angles[PMODEL_PARTS];                // for rotations
	vec3_t oldangles[PMODEL_PARTS];             // for rotations

	// effects
	orientation_t projectionSource;     // for projectiles
	// weapon. Not sure about keeping it here
	int64_t flash_time;
	int64_t barrel_time;
} pmodel_t;

extern pmodel_t cg_entPModels[MAX_EDICTS];      //a pmodel handle for each cg_entity

//
// cg_pmodels.c
//

//utils
bool CG_GrabTag( orientation_t *tag, entity_t *ent, const char *tagname );
void CG_PlaceModelOnTag( entity_t *ent, entity_t *dest, const orientation_t *tag );
void CG_PlaceRotatedModelOnTag( entity_t *ent, entity_t *dest, orientation_t *tag );
void CG_MoveToTag( vec3_t move_origin,
				   mat3_t move_axis,
				   const vec3_t space_origin,
				   const mat3_t space_axis,
				   const vec3_t tag_origin,
				   const mat3_t tag_axis );

//pmodels
void CG_PModelsInit( void );
void CG_PModelsShutdown( void );
void CG_ResetPModels( void );
PlayerModelMetadata *CG_RegisterPlayerModel( const char *filename );
void CG_DrawPlayer( centity_t * cent );
bool CG_PModel_GetProjectionSource( int entnum, orientation_t *tag_result );
void CG_UpdatePlayerModelEnt( centity_t *cent );
void CG_PModel_AddAnimation( int entNum, int loweranim, int upperanim, int headanim, int channel );
void CG_PModel_ClearEventAnimations( int entNum );

//
// cg_wmodels.c
//
void CG_WModelsInit();
struct weaponinfo_s *CG_CreateWeaponZeroModel( char *cgs_name );
struct weaponinfo_s *CG_RegisterWeaponModel( char *cgs_name, int weaponTag );
void CG_AddWeaponOnTag( entity_t *ent, const orientation_t *tag, int weapon, int effects,
	orientation_t *projectionSource, int64_t flash_time, int64_t barrel_time );
struct weaponinfo_s *CG_GetWeaponInfo( int currentweapon );

//=================================================
//				VIEW WEAPON
//=================================================

typedef struct {
	entity_t ent;

	unsigned int POVnum;
	int weapon;

	// animation
	int baseAnim;
	int64_t baseAnimStartTime;
	int eventAnim;
	int64_t eventAnimStartTime;

	// other effects
	orientation_t projectionSource;
} cg_viewweapon_t;
