#pragma once

#include "qcommon/types.h"

constexpr int MAX_CLIENTS = 16;
constexpr int MAX_EDICTS = 1024; // must change protocol to increase more

enum MatchState : u8 {
	MatchState_Warmup,
	MatchState_Countdown,
	MatchState_Playing,
	MatchState_PostMatch,
	MatchState_WaitExit,
};

#define EVENT_ENTITIES_START    96 // entity types above this index will get event treatment

enum EntityType : u8 {
	ET_GENERIC,
	ET_PLAYER,
	ET_CORPSE,
	ET_GHOST,
	ET_JUMPPAD,
	ET_PAINKILLER_JUMPPAD,

	ET_ROCKET,
	ET_GRENADE,
	ET_ARBULLET,
	ET_BUBBLE,
	ET_RIFLEBULLET,
	ET_STAKE,
	ET_BLAST,

	ET_THROWING_AXE,

	ET_LASERBEAM,

	ET_DECAL,

	ET_BOMB,
	ET_BOMB_SITE,

	ET_LASER,
	ET_SPIKES,
	ET_SPEAKER,

	// eventual entities: types below this will get event treatment
	ET_EVENT = EVENT_ENTITIES_START,
	ET_SOUNDEVENT,
};

using WeaponType = u8;
enum WeaponType_ : WeaponType {
	Weapon_None,

	Weapon_Knife,
	Weapon_Pistol,
	Weapon_MachineGun,
	Weapon_Deagle,
	Weapon_Shotgun,
	Weapon_BurstRifle,
	Weapon_StakeGun,
	Weapon_GrenadeLauncher,
	Weapon_RocketLauncher,
	Weapon_AssaultRifle,
	Weapon_BubbleGun,
	Weapon_Laser,
	Weapon_Railgun,
	Weapon_Sniper,
	Weapon_AutoSniper,
	Weapon_Rifle,
	Weapon_MasterBlaster,
	Weapon_RoadGun,
	Weapon_StickyGun,
	// Weapon_Minigun,

	Weapon_Count
};

enum GadgetType : u8 {
	Gadget_None,

	Gadget_ThrowingAxe,
	Gadget_SuicideBomb,
	Gadget_StunGrenade,

	Gadget_Count
};

enum PerkType : u8 {
	Perk_None,

	Perk_Midget,

	Perk_Count
};

enum WeaponState : u8 {
	WeaponState_Dispatch,
	WeaponState_DispatchQuiet,

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

enum FiringMode {
	FiringMode_Auto,
	FiringMode_Smooth,
	FiringMode_SemiAuto,
	FiringMode_Clip,
};

enum RoundType : u8 {
	RoundType_Normal,
	RoundType_MatchPoint,
	RoundType_Overtime,
	RoundType_OvertimeMatchPoint,
};

enum RoundState : u8 {
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

#define GAMESTAT_FLAG_PAUSED ( 1 << 0 )
#define GAMESTAT_FLAG_WAITING ( 1 << 1 )
#define GAMESTAT_FLAG_ISTEAMBASED ( 1 << 2 )
#define GAMESTAT_FLAG_COUNTDOWN ( 1 << 3 )

enum {
	TEAM_SPECTATOR,
	TEAM_PLAYERS,
	TEAM_ALPHA,
	TEAM_BETA,

	GS_MAX_TEAMS,

	TEAM_ALLY,
	TEAM_ENEMY,
};

struct SyncScoreboardPlayer {
	int ping;
	int score;
	int kills;
	bool ready;
	bool carrier;
	bool alive;
};

struct SyncTeamState {
	int player_indices[ MAX_CLIENTS ];
	u8 num_players;
	u8 score;
};

struct SyncBombGameState {
	int attacking_team;

	u8 alpha_players_alive;
	u8 alpha_players_total;
	u8 beta_players_alive;
	u8 beta_players_total;

	bool exploding;
	s64 exploded_at;
};

struct SyncGameState {
	u16 flags;
	MatchState match_state;
	s64 match_state_start_time;
	s64 match_duration;
	s64 clock_override;

	u8 round_num;
	RoundState round_state;
	RoundType round_type;

	SyncTeamState teams[ GS_MAX_TEAMS ];
	SyncScoreboardPlayer players[ MAX_CLIENTS ];

	StringHash map;
	u32 map_checksum;

	SyncBombGameState bomb;
};

struct SyncEvent {
	u64 parm;
	s8 type;
};

struct SyncEntityState {
	int number;

	unsigned int svflags;

	EntityType type;

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
	RGBA8 silhouetteColor;
	int radius;                     // spikes always extended, BombDown stuff, EV_BLOOD damage, ...

	bool linearMovement;
	Vec3 linearMovementVelocity;
	Vec3 linearMovementEnd;
	Vec3 linearMovementBegin;
	unsigned int linearMovementDuration;
	int64_t linearMovementTimeStamp;
	int linearMovementTimeDelta;

	WeaponType weapon;
	bool teleported;
	float scale;

	StringHash sound;

	int team;
};

struct pmove_state_t {
	int pm_type;

	Vec3 origin;
	Vec3 velocity;
	short delta_angles[3];      // add to command angles to get view direction
	                            // changed by spawns, rotating objects, and teleporters

	int pm_flags;               // ducked, jump_held, etc
	int pm_time;

	u16 features;

	s16 no_shooting_time;

	s16 knockback_time;
	s16 crouch_time;
	s16 tbag_time;
	s16 dash_time;
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
	pmove_state_t pmove;

	Vec3 viewangles;

	SyncEvent events[ 2 ];
	unsigned int POVnum;        // entity number of the player in POV
	unsigned int playerNum;     // client number
	float viewheight;

	WeaponSlot weapons[ Weapon_Count - 1 ];

	GadgetType gadget;
	u8 gadget_ammo;

	bool ready;
	bool voted;
	bool can_change_loadout;
	bool carrying_bomb;
	bool can_plant;

	s16 health;
	u16 flashed;

	WeaponState weapon_state;
	u16 weapon_state_time;
	s16 zoom_time;

	WeaponType weapon;
	bool using_gadget;

	WeaponType pending_weapon;
	bool pending_gadget;

	WeaponType last_weapon;

	int team;
	int real_team;

	u8 progress_type; // enum BombProgress
	u8 progress;

	int pointed_player;
	int pointed_health;
};

struct UserCommand {
	u8 msec;
	u8 buttons, down_edges;
	u16 entropy;
	s64 serverTimeStamp;
	s16 angles[ 3 ];
	s8 forwardmove, sidemove, upmove;
	WeaponType weaponSwitch;
};
