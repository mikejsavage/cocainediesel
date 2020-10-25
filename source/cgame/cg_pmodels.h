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
extern cvar_t *cg_gunbob;
extern cvar_t *cg_gun_fov;
extern cvar_t *cg_handOffset;

enum {
	LOWER = 0,
	UPPER,
	HEAD,

	PMODEL_PARTS
};

enum {
	ANIM_NONE,
	BOTH_DEATH1,      //Death animation
	BOTH_DEAD1,       //corpse on the ground
	BOTH_DEATH2,
	BOTH_DEAD2,
	BOTH_DEATH3,
	BOTH_DEAD3,

	LEGS_STAND_IDLE,
	LEGS_WALK_FORWARD,
	LEGS_WALK_BACK,
	LEGS_WALK_LEFT,
	LEGS_WALK_RIGHT,

	LEGS_RUN_FORWARD,
	LEGS_RUN_BACK,
	LEGS_RUN_LEFT,
	LEGS_RUN_RIGHT,

	LEGS_JUMP_LEG1,
	LEGS_JUMP_LEG2,
	LEGS_JUMP_NEUTRAL,
	LEGS_LAND,

	LEGS_CROUCH_IDLE,
	LEGS_CROUCH_WALK,

	LEGS_SWIM_FORWARD,
	LEGS_SWIM_NEUTRAL,

	LEGS_WALLJUMP,
	LEGS_WALLJUMP_LEFT,
	LEGS_WALLJUMP_RIGHT,
	LEGS_WALLJUMP_BACK,

	LEGS_DASH,
	LEGS_DASH_LEFT,
	LEGS_DASH_RIGHT,
	LEGS_DASH_BACK,

	TORSO_HOLD_BLADE,
	TORSO_HOLD_PISTOL,
	TORSO_HOLD_LIGHTWEAPON,
	TORSO_HOLD_HEAVYWEAPON,
	TORSO_HOLD_AIMWEAPON,

	TORSO_SHOOT_BLADE,
	TORSO_SHOOT_PISTOL,
	TORSO_SHOOT_LIGHTWEAPON,
	TORSO_SHOOT_HEAVYWEAPON,
	TORSO_SHOOT_AIMWEAPON,

	TORSO_WEAPON_SWITCHOUT,
	TORSO_WEAPON_SWITCHIN,

	TORSO_DROPHOLD,
	TORSO_DROP,

	TORSO_SWIM,

	TORSO_PAIN1,
	TORSO_PAIN2,
	TORSO_PAIN3,

	PMODEL_TOTAL_ANIMATIONS,
};

enum {
	WEAPANIM_NOANIM,
	WEAPANIM_STANDBY,
	WEAPANIM_ATTACK,
	WEAPANIM_WEAPDOWN,
	WEAPANIM_WEAPONUP,

	VWEAP_MAXANIMS
};

struct WeaponModelMetadata {
	bool inuse;

	const Model * model;

	orientation_t tag_projectionsource;

	Vec3 handpositionOrigin;
	Vec3 handpositionAngles;

	const SoundEffect * fire_sound;
	const SoundEffect * up_sound;
	const SoundEffect * zoom_in_sound;
	const SoundEffect * zoom_out_sound;
};

enum {
	BASE_CHANNEL,
	EVENT_CHANNEL,
	PLAYERANIM_CHANNELS
};

struct animstate_t {
	int anim;
	int64_t startTimestamp;
};

struct PlayerModelAnimationSet {
	int parts[PMODEL_PARTS];
};

struct pmodel_animationstate_t {
	// animations in the mixer
	animstate_t curAnims[PMODEL_PARTS][PLAYERANIM_CHANNELS];
	PlayerModelAnimationSet pending[PLAYERANIM_CHANNELS];
};

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
		u8 node_idx;
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

	u8 upper_rotator_nodes[ 2 ];
	u8 head_rotator_node;
	u8 upper_root_node;

	Tag tag_backpack;
	Tag tag_head;
	Tag tag_weapon;

	AnimationClip clips[ PMODEL_TOTAL_ANIMATIONS ];

	PlayerModelMetadata *next;
};

struct pmodel_t {
	// static data
	const PlayerModelMetadata * metadata;

	// dynamic
	pmodel_animationstate_t animState;

	Vec3 angles[PMODEL_PARTS];                // for rotations
	Vec3 oldangles[PMODEL_PARTS];             // for rotations

	// effects
	orientation_t projectionSource;     // for projectiles
};

extern pmodel_t cg_entPModels[MAX_EDICTS];      //a pmodel handle for each cg_entity

//
// cg_pmodels.c
//

//utils
void CG_MoveToTag( Vec3 * move_origin,
				   mat3_t move_axis,
				   Vec3 space_origin,
				   const mat3_t space_axis,
				   Vec3 tag_origin,
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
void InitWeaponModels();
const WeaponModelMetadata * GetWeaponModelMetadata( WeaponType weapon );

//=================================================
//				VIEW WEAPON
//=================================================

struct cg_viewweapon_t {
	mat3_t axis;
	Vec3 origin;

	// animation
	int baseAnim;
	int64_t baseAnimStartTime;
	int eventAnim;
	int64_t eventAnimStartTime;

	// other effects
	orientation_t projectionSource;
};
