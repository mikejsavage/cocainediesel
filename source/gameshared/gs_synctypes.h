#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "gameshared/q_collision.h"
#include "gameshared/q_shared.h"

constexpr int MAX_CLIENTS = 16;
constexpr int MAX_EDICTS = 1024; // must change protocol to increase more

enum Gametype : u8 {
	Gametype_Bomb,
	Gametype_Gladiator,

	Gametype_Count
};

enum MatchState : u8 {
	MatchState_Warmup,
	MatchState_Countdown,
	MatchState_Playing,
	MatchState_PostMatch,
	MatchState_WaitExit,

	MatchState_Count
};

#define EVENT_ENTITIES_START    96 // entity types above this index will get event treatment

enum EntityType : u8 {
	ET_GENERIC,
	ET_PLAYER,
	ET_CORPSE,
	ET_GHOST,
	ET_JUMPPAD,
	ET_PAINKILLER_JUMPPAD,

	ET_BAZOOKA,
	ET_LAUNCHER,
	ET_FLASH,
	ET_ASSAULT,
	ET_BUBBLE,
	ET_RAILALT,
	ET_RIFLE,
	ET_PISTOL,
	ET_CROSSBOW,
	ET_BLASTER,
	ET_SAWBLADE,
	ET_STICKY,

	ET_SHURIKEN,
	ET_AXE,

	ET_LASERBEAM,

	ET_LIGHT,
	ET_DECAL,

	ET_BOMB,
	ET_BOMB_SITE,

	ET_LASER,
	ET_SPIKES,
	ET_SPEAKER,
	ET_CINEMATIC_MAPNAME,
	ET_MAPMODEL,

	// eventual entities: types below this will get event treatment
	ET_EVENT = EVENT_ENTITIES_START,
	ET_SOUNDEVENT,

	EntityType_Count
};

enum DamageCategory {
	DamageCategory_Weapon,
	DamageCategory_Gadget,
	DamageCategory_World,
};

enum WeaponCategory {
	WeaponCategory_Melee,
	WeaponCategory_Primary,
	WeaponCategory_Secondary,
	WeaponCategory_Backup,

	WeaponCategory_Count
};

enum WeaponType : u8 {
	Weapon_None,

	Weapon_Knife,
	Weapon_Bat,
	Weapon_9mm,
	Weapon_Pistol,
	Weapon_Smg,
	Weapon_Deagle,
	Weapon_Shotgun,
	Weapon_SawnOff,
	Weapon_Burst,
	Weapon_Crossbow,
	Weapon_Launcher,
	Weapon_Bazooka,
	Weapon_Assault,
	Weapon_Bubble,
	Weapon_Laser,
	Weapon_Rail,
	Weapon_Sniper,
	Weapon_Scout,
	Weapon_Rifle,
	Weapon_Blaster,
	Weapon_Roadgun,
	Weapon_Sticky,
	Weapon_Sawblade,
	// Weapon_Minigun,

	Weapon_Count
};

enum GadgetType : u8 {
	Gadget_None,

	Gadget_Axe,
	Gadget_Martyr,
	Gadget_Flash,
	Gadget_Rocket,
	Gadget_Shuriken,

	Gadget_Count
};

enum WorldDamage : u8 {
	WorldDamage_Crush,
	WorldDamage_Telefrag,
	WorldDamage_Suicide,
	WorldDamage_Explosion,

	WorldDamage_Trigger,

	WorldDamage_Laser,
	WorldDamage_Spike,
	WorldDamage_Void,
};

struct DamageType {
	u8 encoded;

	DamageType() = default;
	DamageType( WeaponType weapon );
	DamageType( GadgetType gadget );
	DamageType( WorldDamage world );
};

bool operator==( DamageType a, DamageType b );
bool operator!=( DamageType a, DamageType b );

enum PerkType : u8 {
	Perk_None,

	Perk_Hooligan,
	Perk_Midget,
	Perk_Wheel,
	Perk_Jetpack,
	Perk_Ninja,
	Perk_Boomer,

	Perk_Count
};

enum StaminaState : u8 {
	Stamina_Normal,
	Stamina_Reloading,
	Stamina_UsingAbility,
	Stamina_UsedAbility,

	Stamina_Count,
};

struct Loadout {
	WeaponType weapons[ WeaponCategory_Count ];
	GadgetType gadget;
	PerkType perk;
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
	WeaponState_StagedReloading,

	WeaponState_Cooking,
	WeaponState_Throwing,

	WeaponState_Count
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

	RoundType_Count
};

enum RoundState : u8 {
	RoundState_None,
	RoundState_Countdown,
	RoundState_Round,
	RoundState_Finished,
	RoundState_Post,

	RoundState_Count
};

enum BombDown {
	BombDown_Dropped,
	BombDown_Planting,
};

enum BombProgress : u8 {
	BombProgress_Nothing,
	BombProgress_Planting,
	BombProgress_Defusing,

	BombProgress_Count
};

enum Team : u8 {
	Team_None,

	Team_One,
	Team_Two,
	Team_Three,
	Team_Four,
	Team_Five,
	Team_Six,
	Team_Seven,
	Team_Eight,

	Team_Count
};

struct SyncScoreboardPlayer {
	char name[ MAX_NAME_CHARS + 1 ];
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
	Team attacking_team;

	u8 alpha_players_alive;
	u8 alpha_players_total;
	u8 beta_players_alive;
	u8 beta_players_total;
};

struct SyncGameState {
	Gametype gametype;

	MatchState match_state;
	bool paused;
	s64 match_state_start_time;
	s64 match_duration;
	s64 clock_override;

	struct CameraOverride {
		Vec3 origin;
		EulerDegrees3 angles;
	};

	Optional< CameraOverride > camera_override;

	u8 scorelimit;

	char callvote[ 32 ];
	u8 callvote_required_votes;
	u8 callvote_yes_votes;

	u8 round_num;
	RoundState round_state;
	RoundType round_type;

	SyncTeamState teams[ Team_Count ];
	SyncScoreboardPlayer players[ MAX_CLIENTS ];

	StringHash map;

	SyncBombGameState bomb;
	bool exploding;
	s64 exploded_at;

	EulerDegrees3 sun_angles_from;
	EulerDegrees3 sun_angles_to;
	Time sun_moved_from;
	Time sun_moved_to;
};

struct SyncEvent {
	u64 parm;
	s8 type;
};

enum CollisionModelType : u8 {
	CollisionModelType_Point,
	CollisionModelType_AABB,
	CollisionModelType_Sphere,
	CollisionModelType_Capsule,
	CollisionModelType_MapModel,
	CollisionModelType_GLTF,

	CollisionModelType_Count
};

struct CollisionModel {
	CollisionModelType type;

	union {
		MinMax3 aabb;
		Sphere sphere;
		Capsule capsule;
		StringHash map_model;
		StringHash gltf_model;
	};
};

struct EntityID {
	u64 id;
};

enum EntityFlags : u16 {
	SVF_NOCLIENT         = 1 << 0, // don't send entity to clients, even if it has effects
	SVF_SOUNDCULL        = 1 << 1, // distance culling
	SVF_FAKECLIENT       = 1 << 2, // do not try to send anything to this client
	SVF_BROADCAST        = 1 << 3, // global sound
	SVF_ONLYTEAM         = 1 << 4, // this entity is only transmited to clients with the same ent->s.team value
	SVF_FORCEOWNER       = 1 << 5, // this entity forces the entity at s.ownerNum to be included in the snapshot
	SVF_ONLYOWNER        = 1 << 6, // this entity is only transmitted to its owner
	SVF_OWNERANDCHASERS  = 1 << 7, // this entity is only transmitted to its owner and people spectating them
	SVF_FORCETEAM        = 1 << 8, // this entity is always transmitted to clients with the same ent->s.team value
	SVF_NEVEROWNER       = 1 << 9, // this entity is tramitted to everyone but its owner
};

struct SyncEntityState {
	int number;
	EntityID id;

	EntityFlags svflags;

	EntityType type;

	Vec3 origin;
	EulerDegrees3 angles;
	Vec3 origin2; // velocity for players/corpses. often used for endpoints, e.g. ET_BEAM and some events

	StringHash model;
	StringHash model2;
	StringHash mask;

	Optional< CollisionModel > override_collision_model;
	Optional< SolidBits > solidity;

	bool animating;
	float animation_time;

	StringHash material;
	RGBA8 color;

	bool positioned_sound; // ET_SOUNDEVENT

	int ownerNum; // ET_EVENT specific

	unsigned int effects;

	// impulse events -- muzzle flashes, footsteps, etc
	// events only go out for a single frame, they
	// are automatically cleared each frame
	SyncEvent events[ 2 ];

	char site_letter;
	RGBA8 silhouetteColor;
	int radius;                     // spikes always extended, BombDown stuff, EV_BLOOD damage, ...

	bool linearMovement;
	Vec3 linearMovementVelocity;
	Vec3 linearMovementEnd;
	Vec3 linearMovementBegin;
	unsigned int linearMovementDuration;
	int64_t linearMovementTimeStamp;
	int linearMovementTimeDelta;

	PerkType perk;
	WeaponType weapon;
	GadgetType gadget;
	bool teleported;
	Vec3 scale;

	StringHash sound;

	Team team;
};

struct pmove_state_t {
	int pm_type;

	Vec3 origin;
	Vec3 velocity;
	EulerDegrees3 angles;
	int pm_flags;               // ducked, jump_held, etc

	u16 features;

	s16 no_shooting_time;

	s16 no_friction_time;

	float stamina;
	float stamina_stored;
	float jump_buffering;
	StaminaState stamina_state;

	s16 max_speed;
};

struct WeaponSlot {
	WeaponType weapon;
	u8 ammo;
};

struct TouchInfo {
	int entnum;
	DamageType type;
};

struct SyncPlayerState {
	pmove_state_t pmove;

	EulerDegrees3 viewangles; // TODO: EulerDegrees2

	SyncEvent events[ 2 ];
	unsigned int POVnum;        // entity number of the player in POV
	unsigned int playerNum;     // client number
	float viewheight;

	WeaponSlot weapons[ Weapon_Count - 1 ];

	GadgetType gadget;
	PerkType perk;
	u8 gadget_ammo;

	bool ready;
	bool voted;
	bool can_change_loadout;
	bool carrying_bomb;
	bool can_plant;

	s16 health;
	s16 max_health;
	u16 flashed;

	TouchInfo last_touch;

	WeaponState weapon_state;
	u16 weapon_state_time;
	s16 zoom_time;

	WeaponType weapon;
	bool using_gadget;

	WeaponType pending_weapon;
	bool pending_gadget;

	WeaponType last_weapon;

	Team team;
	Team real_team;

	BombProgress progress_type;
	u8 progress;
};

enum UserCommandButton : u8 {
	Button_Attack1 = 1 << 0,
	Button_Attack2 = 1 << 1,
	Button_Ability1 = 1 << 2,
	Button_Ability2 = 1 << 3,
	Button_Reload = 1 << 4,
	Button_Gadget = 1 << 5,
	Button_Plant = 1 << 6,
};

struct UserCommand {
	u8 msec;
	UserCommandButton buttons, down_edges;
	u16 entropy;
	s64 serverTimeStamp;
	EulerDegrees2 angles;
	s8 forwardmove, sidemove;
	WeaponType weaponSwitch;
};

enum ClientCommandType : u8 {
	ClientCommand_New,
	ClientCommand_Baselines,
	ClientCommand_Begin,
	ClientCommand_Disconnect,
	ClientCommand_NoDelta,
	ClientCommand_UserInfo,

	ClientCommand_Position,
	ClientCommand_Say,
	ClientCommand_SayTeam,
	ClientCommand_Noclip,
	ClientCommand_Suicide,
	ClientCommand_Spectate,
	ClientCommand_ChaseNext,
	ClientCommand_ChasePrev,
	ClientCommand_ToggleFreeFly,
	ClientCommand_Timeout,
	ClientCommand_Timein,
	ClientCommand_DemoList,
	ClientCommand_DemoGetURL,
	ClientCommand_Callvote,
	ClientCommand_VoteYes,
	ClientCommand_VoteNo,
	ClientCommand_Ready,
	ClientCommand_Unready,
	ClientCommand_ToggleReady,
	ClientCommand_Join,
	ClientCommand_TypewriterClack,
	ClientCommand_TypewriterSpace,
	ClientCommand_Spray,
	ClientCommand_Vsay,

	ClientCommand_DropBomb,
	ClientCommand_LoadoutMenu,
	ClientCommand_SetLoadout,

	ClientCommand_Count
};
