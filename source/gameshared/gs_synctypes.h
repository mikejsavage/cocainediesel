#pragma once

#include "qcommon/types.h"

enum MatchState {
	MATCH_STATE_NONE,
	MATCH_STATE_WARMUP,
	MATCH_STATE_COUNTDOWN,
	MATCH_STATE_PLAYTIME,
	MATCH_STATE_POSTMATCH,
	MATCH_STATE_WAITEXIT,

	MATCH_STATE_TOTAL
};

typedef u8 WeaponType;
enum WeaponType_ : WeaponType {
	Weapon_None,

	Weapon_Knife,
	Weapon_Pistol,
	Weapon_MachineGun,
	Weapon_Deagle,
	Weapon_Shotgun,
	Weapon_AssaultRifle,
	Weapon_StakeGun,
	Weapon_GrenadeLauncher,
	Weapon_RocketLauncher,
	Weapon_Plasma,
	Weapon_BubbleGun,
	Weapon_Laser,
	Weapon_Railgun,
	Weapon_Sniper,
	Weapon_Rifle,
	Weapon_MasterBlaster,
	Weapon_RoadGun,
	Weapon_Minigun,

	Weapon_Count
};

typedef u8 WeaponState;
enum WeaponState_ : WeaponState {
	WeaponState_SwitchingIn,
	WeaponState_SwitchingOut,
	WeaponState_Idle,
	WeaponState_Firing,
	WeaponState_FiringSemiAuto,
	WeaponState_FiringSmooth,
	WeaponState_FiringEntireClip,
	WeaponState_Reloading,

	WeaponState_Cooking,
	WeaponState_Throwing,
};

typedef u8 MovementType;
enum MovementType_ : MovementType {
	Movement_Dash,
	Movement_Leap,

	Movement_Count,
};

enum FiringMode {
	FiringMode_Auto,
	FiringMode_Smooth,
	FiringMode_SemiAuto,
	FiringMode_Clip,
};

enum ItemType {
	Item_Bomb,
	Item_FakeBomb,

	Item_Count
};

typedef u8 RoundType;
enum RoundType_ {
	RoundType_Normal,
	RoundType_MatchPoint,
	RoundType_Overtime,
	RoundType_OvertimeMatchPoint,
};

using RoundState = u8;
enum RoundState_ : RoundState {
	RoundState_None,
	RoundState_Countdown,
	RoundState_Round,
	RoundState_Finished,
	RoundState_Post,
};

enum BombDown {
	BombDown_Dropped,
	BombDown_Planting,
};

enum BombProgress {
	BombProgress_Nothing,
	BombProgress_Planting,
	BombProgress_Defusing,
};

#define GAMESTAT_FLAG_PAUSED ( 1 << 0LL )
#define GAMESTAT_FLAG_WAITING ( 1 << 1LL )
#define GAMESTAT_FLAG_HASCHALLENGERS ( 1 << 2LL )
#define GAMESTAT_FLAG_INHIBITSHOOTING ( 1 << 3LL )
#define GAMESTAT_FLAG_ISTEAMBASED ( 1 << 4LL )
#define GAMESTAT_FLAG_ISRACE ( 1 << 5LL )
#define GAMESTAT_FLAG_COUNTDOWN ( 1 << 6LL )

struct SyncBombGameState {
	u8 alpha_score;
	u8 beta_score;
	u8 alpha_players_alive;
	u8 alpha_players_total;
	u8 beta_players_alive;
	u8 beta_players_total;

	bool exploding;
	s64 exploded_at;
};

struct SyncGameState {
	u16 flags;
	int match_state;
	int64_t match_start;
	int64_t match_duration;
	int64_t clock_override;
	RoundState round_state;
	RoundType round_type;
	u8 max_team_players;

	StringHash map;
	u32 map_checksum;

	SyncBombGameState bomb;
};

struct SyncEvent {
	u64 parm;
	s8 type;
};

struct SyncEntityState {
	int number;                         // edict index

	unsigned int svflags;

	int type;                           // ET_GENERIC, ET_BEAM, etc

	Vec3 origin;
	Vec3 angles;
	Vec3 origin2; // velocity for players/corpses. often used for endpoints, e.g. ET_BEAM and some events
	MinMax3 bounds;

	StringHash model;
	StringHash model2;

	bool animating;
	float animation_time;

	StringHash material;
	RGBA8 color;

	int channel;                    // ET_SOUNDEVENT

	int ownerNum;                   // ET_EVENT specific

	unsigned int effects;

	// impulse events -- muzzle flashes, footsteps, etc
	// events only go out for a single frame, they
	// are automatically cleared each frame
	SyncEvent events[ 2 ];

	int counterNum;                 // ET_GENERIC
	int targetNum;                  // ET_EVENT specific
	RGBA8 silhouetteColor;
	int radius;                     // ET_GLADIATOR always extended, ET_BOMB state, EV_BLOOD damage, ...

	bool linearMovement;
	Vec3 linearMovementVelocity;      // this is transmitted instead of origin when linearProjectile is true
	Vec3 linearMovementEnd;           // the end movement point for brush models
	Vec3 linearMovementBegin;			// the starting movement point for brush models
	unsigned int linearMovementDuration;
	int64_t linearMovementTimeStamp;
	int linearMovementTimeDelta;

	WeaponType weapon;                  // WEAP_ for players
	bool teleported;

	StringHash sound;                          // for looping sounds, to guarantee shutoff

	int team;                           // team in the game
};

// SyncPlayerState is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 SyncPlayerState sent each second,
// but the number of pmove_state_t changes will be relative to client
// frame rates
struct pmove_state_t {
	int pm_type;

	Vec3 origin;
	Vec3 velocity;
	short delta_angles[3];      // add to command angles to get view direction
	                            // changed by spawns, rotating objects, and teleporters

	int pm_flags;               // ducked, jump_held, etc
	int pm_time;

	u16 features;

	s16 knockback_time;
	s16 crouch_time;
	s16 tbag_time;
	s16 jump_time;
	s16 walljump_time;

	s16 max_speed;
	s16 jump_speed;
	s16 dash_speed;
};

struct WeaponSlot {
	WeaponType weapon;
	u8 ammo;
};

struct SyncPlayerState {
	pmove_state_t pmove;        // for prediction

	// these fields do not need to be communicated bit-precise

	Vec3 viewangles;          // for fixed views

	SyncEvent events[ 2 ];
	unsigned int POVnum;        // entity number of the player in POV
	unsigned int playerNum;     // client number
	float viewheight;
	float fov;                  // horizontal field of view (unused)

	// BitArray items;
	//
	// struct GrenadeInfo { short count; };
	// struct RechargableCloakingDeviceInfo { float energy; };

	WeaponSlot weapons[ Weapon_Count - 1 ];
	bool items[ Item_Count ];
	MovementType movement;

	bool show_scoreboard;
	bool ready;
	bool voted;
	bool can_change_loadout;
	bool carrying_bomb;
	bool can_plant;

	s16 health;

	WeaponState weapon_state;
	u16 weapon_state_time;

	WeaponType weapon;
	WeaponType pending_weapon;
	WeaponType last_weapon;
	s16 zoom_time;

	int team;
	int real_team;

	u8 progress_type; // enum BombProgress
	u8 progress;

	int pointed_player;
	int pointed_health;
};

// usercmd_t is sent to the server each client frame
struct usercmd_t {
	u8 msec;
	u32 buttons;
	u16 entropy;
	s64 serverTimeStamp;
	s16 angles[ 3 ];
	s8 forwardmove, sidemove, upmove;
	WeaponType weaponSwitch;
};
