#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/string.h"

#include "game/g_local.h"

enum BombState {
	BombState_Idle,
	BombState_Carried,
	BombState_Dropped,
	BombState_Planting,
	BombState_Planted,
	BombState_Exploding,
};

enum BombDropReason {
	BombDropReason_Normal,
	BombDropReason_Killed,
	BombDropReason_ChangingTeam,
};

static constexpr u32 bomb_max_plant_speed = 50;
static constexpr u32 bomb_max_plant_height = 100;
static constexpr u32 bomb_drop_retake_delay = 1000;
static constexpr float bomb_arm_defuse_radius = 36.0f;
static constexpr u32 bomb_autodrop_distance = 400;
static constexpr u32 bomb_throw_speed = 550;
static constexpr u32 bomb_explosion_effect_radius = 256;
static constexpr u32 bomb_dead_camera_dist = 256;
static constexpr int initial_attackers = TEAM_ALPHA;
static constexpr int initial_defenders = TEAM_BETA;
static constexpr int site_explosion_points = 10;
static constexpr int site_explosion_max_delay = 1000;
static constexpr float site_explosion_max_dist = 512.0f;
static constexpr MinMax3 bomb_bounds( Vec3( -16.0f ), Vec3( 16.0f, 16.0f, 48.0f ) );
static constexpr float bomb_hud_offset = 32.0f;

static constexpr StringHash model_bomb = "models/bomb/bomb";

enum BombAnnouncement {
	BombAnnouncement_RoundStarted,
	BombAnnouncement_Planted,
	BombAnnouncement_Defused,

	BombAnnouncement_Count
};

static constexpr StringHash snd_announcements_off[ BombAnnouncement_Count ] = {
	"sounds/announcer/bomb/offense/start",
	"sounds/announcer/bomb/offense/planted",
	"sounds/announcer/bomb/offense/defused",
};

static constexpr StringHash snd_announcements_def[ BombAnnouncement_Count ] = {
	"sounds/announcer/bomb/defense/start",
	"sounds/announcer/bomb/defense/planted",
	"sounds/announcer/bomb/defense/defused",
};

static cvar_t * g_bomb_roundtime;
static cvar_t * g_bomb_bombtimer;
static cvar_t * g_bomb_armtime;
static cvar_t * g_bomb_defusetime;

static const u32 max_sites = 26;
static const int countdown_max = 6;

static struct {
	bool round_check_end;
	s64 round_start_time;
	s64 round_state_start;
	s64 round_state_end;
	s32 countdown;
	s32 last_time;
	bool was_1vx;

	int attacking_team;
	int defending_team;

	// TODO: player.as
	u32 kills_this_round[ MAX_CLIENTS ];
	WeaponType loadouts[ MAX_CLIENTS ][ WeaponCategory_Count ];

	s32 carrier;
	s64 carrier_can_plant_time;
	s32 defuser;
	s64 defuse_progress;

	struct {
		BombState state;
		edict_t * model;
		edict_t * hud;
		s64 action_time;
		s64 pick_time;
		s32 dropper;
		bool killed_everyone;
	} bomb;

	struct {
		edict_t * indicator;
		edict_t * hud;
		char letter;
	} sites[ max_sites ];
	u32 num_sites;
	u32 site;
} bomb_state;

static bool BombCanPlant();
static void BombStartPlanting( u32 site );
static void BombSetCarrier( s32 player_num, bool no_sound );
static void RoundWonBy( int winner );
static void RoundNewState( RoundState state );
static void SetTeamProgress( int team, int percent, BombProgress type );
static edict_t * RandomEntity( StringHash classname );
static void UpdateScore( s32 player_num );

static void Show( edict_t * ent ) {
	ent->r.svflags &= ~SVF_NOCLIENT;
	GClip_LinkEntity( ent );
}

static void Hide( edict_t * ent ) {
	ent->r.svflags |= SVF_NOCLIENT;
}

static bool EntCanSee( edict_t * ent, Vec3 point ) {
	Vec3 center = ent->s.origin + 0.5f * ( ent->r.mins + ent->r.maxs );
	trace_t tr;
	G_Trace( &tr, center, Vec3( 0.0f ), Vec3( 0.0f ), point, ent, MASK_SOLID );
	return tr.startsolid || tr.allsolid || tr.ent == -1;
}

static s32 FirstNearbyTeammate( Vec3 origin, int team ) {
	int touch[ MAX_EDICTS ];
	int num_touch = GClip_FindInRadius( origin, bomb_arm_defuse_radius, touch, ARRAY_COUNT( touch ) );
	for( int i = 0; i < num_touch; i++ ) {
		edict_t * ent = &game.edicts[ touch[ i ] ];
		if( ent->s.type != ET_PLAYER || ent->s.team != team || G_ISGHOSTING( ent ) || !EntCanSee( ent, origin ) ) {
			continue;
		}

		return PLAYERNUM( ent );
	}
	return -1;
}

static void Announce( BombAnnouncement announcement ) {
	G_AnnouncerSound( NULL, snd_announcements_off[ announcement ], bomb_state.attacking_team, true, NULL );
	G_AnnouncerSound( NULL, snd_announcements_def[ announcement ], bomb_state.defending_team, true, NULL );
}

// player.as

static void GiveInventory( edict_t * ent ) {
	ClearInventory( &ent->r.client->ps );

	G_GiveWeapon( ent, Weapon_Knife );
	for( u32 i = 0; i < WeaponCategory_Count; i++ ) {
		WeaponType weapon = bomb_state.loadouts[ PLAYERNUM( ent ) ][ i ];
		if( weapon != Weapon_None ) {
			G_GiveWeapon( ent, weapon );
		}
	}

	G_SelectWeapon( ent, 0 );
	G_SelectWeapon( ent, 1 );
}

static void ShowShop( s32 player_num ) {
	if( PLAYERENT( player_num )->s.team == TEAM_SPECTATOR ) {
		return;
	}

	TempAllocator temp = svs.frame_arena.temp();
	DynamicString command( &temp, "changeloadout" );

	for( u32 i = 0; i < WeaponCategory_Count; i++ ) {
		WeaponType weapon = bomb_state.loadouts[ player_num ][ i ];
		if( weapon != Weapon_None ) {
			command.append( " {}", weapon );
		}
	}

	PF_GameCmd( PLAYERENT( player_num ), command.c_str() );
}

static void SetLoadout( edict_t * ent, Span< const char > loadout ) {
	int player_num = PLAYERNUM( ent );

	for( u32 i = 0; i < WeaponCategory_Count; i++ ) {
		int weapon = ParseInt( &loadout, 0, Parse_DontStopOnNewLine );
		if( weapon == Weapon_None ) {
			break;
		}

		if( weapon < Weapon_None || weapon >= Weapon_Count || weapon == Weapon_Knife ) {
			return;
		}
		WeaponCategory category = GS_GetWeaponDef( WeaponType( weapon ) )->category;

		bomb_state.loadouts[ player_num ][ category ] = WeaponType( weapon );
	}

	TempAllocator temp = svs.frame_arena.temp();
	DynamicString command( &temp, "saveloadout" );

	for( u32 i = 0; i < WeaponCategory_Count; i++ ) {
		WeaponType weapon = bomb_state.loadouts[ player_num ][ i ];
		if( weapon != Weapon_None ) {
			command.append( " {}", weapon );
		}
	}

	PF_GameCmd( ent, command.c_str() );

	if( G_ISGHOSTING( ent ) ) {
		return;
	}

	if( server_gs.gameState.match_state == MatchState_Warmup || server_gs.gameState.match_state == MatchState_Countdown ) {
		GiveInventory( ent );
	}

	if( server_gs.gameState.match_state == MatchState_Playing && server_gs.gameState.round_state == RoundState_Countdown ) {
		GiveInventory( ent );
	}
}

static void ResetKillCounters() {
	for( u32 i = 0; i < MAX_CLIENTS; i++ ) {
		bomb_state.kills_this_round[ i ] = 0;
	}
}

// site.as

static void BombSiteCarrierTouched( u32 site ) {
	bomb_state.carrier_can_plant_time = level.time;
	if( BombCanPlant() ) {
		edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
		Vec3 maxs = carrier_ent->r.maxs;
		Vec3 velocity = carrier_ent->velocity;

		// TODO: this is checking for crouching by checking maxs.z, dno
		if( maxs.z < 40 && level.time - bomb_state.bomb.action_time >= 1000 && Length( velocity ) < bomb_max_plant_speed ) {
			BombStartPlanting( site );
		}
	}
}

static s32 GetSiteFromIndicator( edict_t * ent ) {
	for( u32 i = 0; i < bomb_state.num_sites; i++ ) {
		if( bomb_state.sites[ i ].indicator == ent ) {
			return i;
		}
	}
	return -1;
}

static void ResetBombSites() {
	bomb_state.num_sites = 0;
}

static void SpawnBombSite( edict_t * ent ) {
	u32 i = bomb_state.num_sites;
	if( i >= max_sites ) {
		Com_GGPrint( "Too many bombsites, ignoring" );
		return;
	}

	char letter = 'A' + i;

	bomb_state.sites[ i ].indicator = ent;
	bomb_state.sites[ i ].indicator->s.model = EMPTY_HASH;
	bomb_state.sites[ i ].indicator->nextThink = level.time + 1;
	bomb_state.sites[ i ].indicator->s.team = 0;
	GClip_LinkEntity( bomb_state.sites[ i ].indicator );

	bomb_state.sites[ i ].hud = G_Spawn();
	bomb_state.sites[ i ].hud->classname = "hud_bomb_site";
	bomb_state.sites[ i ].hud->s.type = ET_BOMB_SITE;
	bomb_state.sites[ i ].hud->r.solid = SOLID_NOT;
	bomb_state.sites[ i ].hud->s.origin = bomb_state.sites[ i ].indicator->s.origin;
	bomb_state.sites[ i ].hud->r.svflags = SVF_BROADCAST;
	bomb_state.sites[ i ].hud->s.counterNum = letter;
	GClip_LinkEntity( bomb_state.sites[ i ].hud );

	bomb_state.num_sites++;
}

static void PlantAreaThink( edict_t * ent ) {
	edict_t * target = G_Find( NULL, &edict_t::name, ent->target );
	if( target != NULL ) {
		ent->s.counterNum = GetSiteFromIndicator( target );
		return;
	}
	Com_GGPrint( "plant_area at {} has no targets, removing...", ent->s.origin );
	G_FreeEdict( ent );
}

static void PlantAreaTouch( edict_t * self, edict_t * other, cplane_t * plane, int surfFlags ) {
	if( other->r.client == NULL ) {
		return;
	}

	if( server_gs.gameState.match_state != MatchState_Playing || server_gs.gameState.round_state != RoundState_Round ) {
		return;
	}

	if( bomb_state.bomb.state != BombState_Carried || PLAYERNUM( other ) != bomb_state.carrier ) {
		return;
	}

	BombSiteCarrierTouched( self->s.counterNum );
}

static void SpawnPlantArea( edict_t * ent ) {
	ent->think = PlantAreaThink;
	ent->touch = PlantAreaTouch;
	GClip_SetBrushModel( ent );
	ent->r.solid = SOLID_TRIGGER;
	GClip_LinkEntity( ent );

	ent->nextThink = level.time + 1000; // think this can just be + 1
}

// bomb.as

static void BombTouch( edict_t * self, edict_t * other, cplane_t * plane, int surfFlags ) {
	if( server_gs.gameState.match_state != MatchState_Playing ) {
		return;
	}

	if( bomb_state.bomb.state != BombState_Dropped ) {
		return;
	}

	if( other->r.client == NULL ) {
		return;
	}

	if( other->s.team != bomb_state.attacking_team ) {
		return;
	}

	if( G_ISGHOSTING( other ) || other->health < 0 ) {
		return;
	}

	if( level.time < bomb_state.bomb.pick_time && PLAYERNUM( other ) == bomb_state.bomb.dropper ) {
		return;
	}

	BombSetCarrier( PLAYERNUM( other ), false );
}

static void BombStop( edict_t * self ) {
	if( bomb_state.bomb.state == BombState_Dropped ) {
		bomb_state.bomb.hud->s.origin = bomb_state.bomb.model->s.origin + Vec3( 0.0f, 0.0f, bomb_hud_offset );
		bomb_state.bomb.hud->r.svflags |= SVF_ONLYTEAM;
		bomb_state.bomb.hud->s.radius = BombDown_Dropped;
		Show( bomb_state.bomb.hud );
	}
}

static void BombModelCreate() {
	bomb_state.bomb.model = G_Spawn();
	bomb_state.bomb.model->classname = "bomb";
	bomb_state.bomb.model->s.type = ET_GENERIC;
	bomb_state.bomb.model->r.mins = bomb_bounds.mins;
	bomb_state.bomb.model->r.maxs = bomb_bounds.maxs;
	bomb_state.bomb.model->r.solid = SOLID_TRIGGER;
	bomb_state.bomb.model->s.model = model_bomb;
	bomb_state.bomb.model->s.silhouetteColor = RGBA8( 255, 255, 255, 255 );
	bomb_state.bomb.model->r.svflags |= SVF_BROADCAST;
	bomb_state.bomb.model->touch = BombTouch;
	bomb_state.bomb.model->stop = BombStop;
}

static void BombInit() {
	BombModelCreate();

	bomb_state.bomb.hud = G_Spawn();
	bomb_state.bomb.hud->classname = "hud_bomb";
	bomb_state.bomb.hud->s.type = ET_BOMB;
	bomb_state.bomb.hud->r.solid = SOLID_NOT;
	bomb_state.bomb.hud->r.svflags |= SVF_BROADCAST;
	bomb_state.bomb.action_time = -1;
}

static void BombPickup() {
	PLAYERENT( bomb_state.carrier )->s.effects |= EF_CARRIER;
	PLAYERENT( bomb_state.carrier )->s.model2 = model_bomb;

	Hide( bomb_state.bomb.model );
	Hide( bomb_state.bomb.hud );

	bomb_state.bomb.model->movetype = MOVETYPE_NONE;
	bomb_state.bomb.state = BombState_Carried;
}

static void RemoveCarrier() {
	if( bomb_state.carrier == -1 ) {
		return;
	}

	PLAYERENT( bomb_state.carrier )->s.effects &= ~EF_CARRIER;
	PLAYERENT( bomb_state.carrier )->s.model2 = EMPTY_HASH;

	bomb_state.carrier = -1;
}

static void BombSetCarrier( s32 player_num, bool no_sound ) {
	RemoveCarrier();

	bomb_state.carrier = player_num;
	BombPickup();

	if( !no_sound ) {
		G_AnnouncerSound( PLAYERENT( player_num ), "sounds/announcer/bomb/offense/taken", bomb_state.attacking_team, true, NULL );
	}
}

static void BombDrop( BombDropReason reason ) {
	SetTeamProgress( bomb_state.attacking_team, 0, BombProgress_Nothing );
	edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
	Vec3 start = carrier_ent->s.origin;
	Vec3 end( 0.0f );
	Vec3 velocity( 0.0f );

	switch( reason ) {
		case BombDropReason_Normal:
		case BombDropReason_Killed: {
			bomb_state.bomb.pick_time = level.time + bomb_drop_retake_delay;
			bomb_state.bomb.dropper = bomb_state.carrier;
			start.z += carrier_ent->viewheight;
			Vec3 forward, right, up;
			AngleVectors( carrier_ent->s.angles, &forward, &right, &up );

			end = start + forward * 24.0f;
			end.z -= 16.0f;

			velocity = carrier_ent->velocity + forward * 200.0f;
			velocity.z = bomb_throw_speed;

			if( reason == BombDropReason_Killed ) {
				velocity.z *= 0.5f;
			}
		} break;

		case BombDropReason_ChangingTeam: {
			bomb_state.bomb.dropper = -1;
			start = end = carrier_ent->s.origin;
			velocity = carrier_ent->velocity;
		} break;
	}

	trace_t tr;
	G_Trace( &tr, start, bomb_bounds.mins, bomb_bounds.maxs, end, carrier_ent, MASK_SOLID );

	bomb_state.bomb.model->movetype = MOVETYPE_TOSS;
	bomb_state.bomb.model->r.owner = carrier_ent;
	bomb_state.bomb.model->s.origin = tr.endpos;
	bomb_state.bomb.model->velocity = velocity;
	Show( bomb_state.bomb.model );
	RemoveCarrier();
	bomb_state.bomb.state = BombState_Dropped;
}

static void BombStartPlanting( u32 site ) {
	// TODO
	bomb_state.site = site;

	edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
	Vec3 start = carrier_ent->s.origin;

	Vec3 end = start;
	end.z -= 512.0f;

	trace_t tr;
	G_Trace( &tr, start, bomb_bounds.mins, bomb_bounds.maxs, end, carrier_ent, MASK_SOLID );

	Vec3 angles( 0.0f, RandomUniformFloat( &svs.rng, 0.0f, 360.0f ), 0.0f );

	bomb_state.bomb.model->s.origin = tr.endpos;
	bomb_state.bomb.model->s.angles = angles;
	Show( bomb_state.bomb.model );

	bomb_state.bomb.hud->s.origin = tr.endpos + Vec3( 0.0f, 0.0f, bomb_hud_offset );
	bomb_state.bomb.hud->s.angles = angles;
	bomb_state.bomb.hud->r.svflags |= SVF_ONLYTEAM;
	bomb_state.bomb.hud->s.radius = BombDown_Planting;
	Show( bomb_state.bomb.hud );

	carrier_ent->s.effects &= ~EF_CARRIER;
	carrier_ent->s.model2 = EMPTY_HASH;

	bomb_state.bomb.action_time = level.time;
	bomb_state.bomb.state = BombState_Planting;

	G_Sound( bomb_state.bomb.model, 0, "models/bomb/plant" );
}

static void BombPlanted() {
	bomb_state.bomb.action_time = level.time + int( g_bomb_bombtimer->value * 1000.0f );
	bomb_state.bomb.model->s.sound = "models/bomb/fuse";
	bomb_state.bomb.model->s.effects &= ~EF_TEAM_SILHOUETTE;

	// show to defs too
	bomb_state.bomb.hud->r.svflags &= ~SVF_ONLYTEAM;

	// start fuse animation
	bomb_state.bomb.model->s.animating = true;
	bomb_state.bomb.hud->s.animating = true; // do you need this?

	Announce( BombAnnouncement_Planted );

	TempAllocator temp = svs.frame_arena.temp();
	G_CenterPrintMsg( NULL, "Bomb planted at %c!", bomb_state.sites[ bomb_state.site ].letter );

	RemoveCarrier();
	bomb_state.defuse_progress = 0;
	bomb_state.bomb.state = BombState_Planted;
}

static void BombDefused() {
	bomb_state.bomb.model->s.sound = EMPTY_HASH;
	bomb_state.bomb.hud->s.animating = false;

	Announce( BombAnnouncement_Defused );

	bomb_state.bomb.state = BombState_Idle;

	TempAllocator temp = svs.frame_arena.temp();
	G_PrintMsg( NULL, "%s defused the bomb!", PLAYERENT( bomb_state.defuser )->r.client->netname );

	G_Sound( bomb_state.bomb.model, CHAN_AUTO, "models/bomb/tss" );

	RoundWonBy( bomb_state.defending_team );

	bomb_state.defuser = -1;
}

static void BombExplode() {
	if( server_gs.gameState.round_state == RoundState_Round ) {
		RoundWonBy( bomb_state.attacking_team );
	}

	Hide( bomb_state.bomb.hud );
	Hide( bomb_state.bomb.model );

	bomb_state.bomb.killed_everyone = false;

	bomb_state.bomb.state = BombState_Exploding;
	bomb_state.defuser = -1;

	server_gs.gameState.bomb.exploding = true;
	server_gs.gameState.bomb.exploded_at = svs.gametime; // TODO: only place where gameTime is used, dno

	G_SpawnEvent( EV_EXPLOSION1, bomb_explosion_effect_radius, &bomb_state.bomb.model->s.origin );

	G_Sound( bomb_state.bomb.model, CHAN_AUTO, "models/bomb/explode" );
}

static void ResetBomb() {
	Hide( bomb_state.bomb.model );
	bomb_state.bomb.model->s.effects |= EF_TEAM_SILHOUETTE;

	bomb_state.bomb.model->s.team = bomb_state.attacking_team;
	bomb_state.bomb.hud->s.team = bomb_state.attacking_team;

	bomb_state.bomb.state = BombState_Idle;
}

static void BombThink() {
	switch( bomb_state.bomb.state ) {
		case BombState_Idle:
			break;

		case BombState_Dropped: {
			// respawn bomb if it falls in the void
			if( bomb_state.bomb.model->s.origin.z < -1024.0f ) {
				bomb_state.bomb.model->s.origin = RandomEntity( "spawn_bomb_attacking" )->s.origin;
				bomb_state.bomb.model->velocity = Vec3( 0.0f );
				bomb_state.bomb.model->s.teleported = true;

				constexpr StringHash vfx_bomb_respawn = "models/bomb/respawn";

				G_Sound( bomb_state.bomb.model, CHAN_AUTO, "models/bomb/respawn" );
				G_SpawnEvent( EV_VFX, vfx_bomb_respawn.hash, &bomb_state.bomb.model->s.origin );
			}
		} break;

		case BombState_Planting: {
			edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
			if( !EntCanSee( carrier_ent, bomb_state.bomb.model->s.origin ) || Length( carrier_ent->s.origin - bomb_state.bomb.model->s.origin ) > bomb_arm_defuse_radius ) {
				SetTeamProgress( bomb_state.attacking_team, 0, BombProgress_Nothing );
				BombPickup();
				break;
			}

			float frac = float( level.time - bomb_state.bomb.action_time ) / ( g_bomb_armtime->value * 1000.0f );
			if( frac >= 1.0f ) {
				SetTeamProgress( bomb_state.attacking_team, 0, BombProgress_Nothing );
				BombPlanted();
				break;
			}

			if( frac != 0.0f ) {
				SetTeamProgress( bomb_state.attacking_team, int( frac * 100.0f ), BombProgress_Planting );
			}
		} break;

		case BombState_Planted: {
			float animation_time = ( g_bomb_bombtimer->value - ( bomb_state.bomb.action_time - level.time ) * 0.001f ) / g_bomb_bombtimer->value;
			bomb_state.bomb.model->s.animation_time = animation_time;
			bomb_state.bomb.hud->s.animation_time = animation_time; // dno if this is needed?

			if( bomb_state.defuser == -1 ) {
				bomb_state.defuser = FirstNearbyTeammate( bomb_state.bomb.model->s.origin, bomb_state.defending_team );
			}

			if( bomb_state.defuser != -1 ) {
				edict_t * defuser_ent = PLAYERENT( bomb_state.defuser );
				if( G_ISGHOSTING( defuser_ent ) || !EntCanSee( defuser_ent, bomb_state.bomb.model->s.origin ) || Length( defuser_ent->s.origin - bomb_state.bomb.model->s.origin ) > bomb_arm_defuse_radius ) {
					bomb_state.defuser = -1;
				}
			}

			if( bomb_state.defuser == -1 && bomb_state.defuse_progress > 0 ) {
				bomb_state.defuse_progress = 0;
				SetTeamProgress( bomb_state.defending_team, 0, BombProgress_Nothing );
			}
			else {
				bomb_state.defuse_progress += game.frametime;
				float frac = bomb_state.defuse_progress / ( g_bomb_defusetime->value * 1000.0f );
				if( frac >= 1.0f ) {
					BombDefused();
					SetTeamProgress( bomb_state.defending_team, 100, BombProgress_Defusing );
					break;
				}
				SetTeamProgress( bomb_state.defending_team, int( frac * 100.0f ), BombProgress_Defusing );
			}

			if( level.time >= bomb_state.bomb.action_time ) {
				BombExplode();
			}
		} break;

		case BombState_Exploding: {
			// BombSiteStepExplosion( bomb_state.site );
			if( level.time - server_gs.gameState.bomb.exploded_at >= 1000 && !bomb_state.bomb.killed_everyone ) {
				bomb_state.bomb.model->projectileInfo.maxDamage = 1.0f;
				bomb_state.bomb.model->projectileInfo.minDamage = 1.0f;
				bomb_state.bomb.model->projectileInfo.maxKnockback = 400.0f;
				bomb_state.bomb.model->projectileInfo.minKnockback = 1.0f;
				bomb_state.bomb.model->projectileInfo.radius = 9999;

				// apply a 1 damage explosion just for the kb
				G_RadiusDamage( bomb_state.bomb.model, NULL, NULL, bomb_state.bomb.model, WorldDamage_Explosion );

				for( int i = 0; i < MAX_CLIENTS; i++ ) {
					G_Damage( PLAYERENT( i ), world, world, Vec3( 0.0f ), Vec3( 0.0f ), bomb_state.bomb.model->s.origin, 100.0f, 0.0f, 0, WorldDamage_Explosion );
				}

				bomb_state.bomb.killed_everyone = true;
			}
		} break;
	}
}

static bool BombCanPlant() {
	edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
	Vec3 start = carrier_ent->s.origin;
	Vec3 end = start;
	end.z -= bomb_max_plant_height;
	Vec3 mins = carrier_ent->r.mins;
	Vec3 maxs = carrier_ent->r.maxs;

	trace_t tr;
	G_Trace( &tr, start, mins, maxs, end, carrier_ent, MASK_SOLID );

	return !tr.startsolid && tr.plane.normal.z >= cosf( Radians( 30 ) );
}

static void BombGiveToRandom() {
	int num_bots = 0;
	int num_players = server_gs.gameState.teams[ bomb_state.attacking_team ].num_players;
	for( int i = 0; i < num_players; i++ ) {
		s32 player_num = server_gs.gameState.teams[ bomb_state.attacking_team ].player_indices[ i ] - 1;
		edict_t * ent = PLAYERENT( player_num );
		if( ( ent->r.svflags & SVF_FAKECLIENT ) != 0 ) {
			num_bots++;
		}
	}

	bool all_bots = num_bots == num_players;
	int n = all_bots ? num_players : num_players - num_bots;
	s32 carrier = RandomUniform( &svs.rng, 0, n );
	s32 seen = 0;

	for( int i = 0; i < num_players; i++ ) {
		s32 player_num = server_gs.gameState.teams[ bomb_state.attacking_team ].player_indices[ i ] - 1;
		edict_t * ent = PLAYERENT( player_num );
		if( all_bots || ( ent->r.svflags & SVF_FAKECLIENT ) == 0 ) {
			if( seen == carrier ) {
				BombSetCarrier( player_num, true );
				break;
			}
			seen++;
		}
	}
}

// round.as

static void RespawnAllPlayers( bool ghost = false ) {
	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( PF_GetClientState( i ) >= CS_SPAWNED ) {
			GClip_UnlinkEntity( ent );
		}
	}

	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( PF_GetClientState( i ) >= CS_SPAWNED ) {
			G_ClientRespawn( ent, ghost );
		}
	}
}

static u32 PlayersAliveOnTeam( int team ) {
	u32 alive = 0;
	for( u32 i = 0; i < server_gs.gameState.teams[ team ].num_players; i++ ) {
		edict_t * ent = PLAYERENT( server_gs.gameState.teams[ team ].player_indices[ i ] - 1 );
		if( !G_ISGHOSTING( ent ) && ent->health > 0 ) {
			alive++;
		}
	}
	return alive;
}

static void EnableMovementFor( s32 playernum ) {
	edict_t * ent = PLAYERENT( playernum );
	ent->r.client->ps.pmove.max_speed = -1;
	ent->r.client->ps.pmove.dash_speed = -1;
	ent->r.client->ps.pmove.features = ent->r.client->ps.pmove.features | PMFEAT_JUMP | PMFEAT_SPECIAL;
}

static void EnableMovement() {
	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( PF_GetClientState( i ) >= CS_SPAWNED && ent->s.team != TEAM_SPECTATOR ) {
			EnableMovementFor( i );
		}
	}
}

static void DisableMovementFor( s32 playernum ) {
	edict_t * ent = PLAYERENT( playernum );
	ent->r.client->ps.pmove.max_speed = 100;
	ent->r.client->ps.pmove.dash_speed = 0;
	ent->r.client->ps.pmove.features &= ~( PMFEAT_JUMP | PMFEAT_SPECIAL );
	ent->r.client->ps.pmove.no_shooting_time = 5000;
}

static void DisableMovement() {
	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( PF_GetClientState( i ) >= CS_SPAWNED && ent->s.team != TEAM_SPECTATOR ) {
			DisableMovementFor( i );
		}
	}
}

static void CheckPlayersAlive( int team ) {
	u32 alive = PlayersAliveOnTeam( team );
	if( alive == 0 ) {
		if( team == bomb_state.attacking_team ) {
			if( bomb_state.bomb.state != BombState_Planted ) {
				RoundWonBy( bomb_state.defending_team );
			}
		}
		else {
			RoundWonBy( bomb_state.attacking_team );
		}
		return;
	}

	int other_team = team == bomb_state.attacking_team ? bomb_state.attacking_team : bomb_state.defending_team;
	u32 alive_other_team = PlayersAliveOnTeam( other_team );

	if( alive == 1 ) {
		if( alive_other_team == 1 ) {
			if( bomb_state.was_1vx ) {
				G_AnnouncerSound( NULL, "sounds/announcer/bomb/1v1", GS_MAX_TEAMS, false, NULL );
			}
		}
		else if( alive_other_team >= 3 ) {
			G_AnnouncerSound( NULL, "sounds/announcer/bomb/1vx", team, false, NULL );
			G_AnnouncerSound( NULL, "sounds/announcer/bomb/xv1", other_team, false, NULL );
			bomb_state.was_1vx = true;
		}
	}
}

static void SetTeams() {
	u32 limit = g_scorelimit->integer;
	u8 round_num = server_gs.gameState.round_num;
	if( limit == 0 || round_num > ( limit - 1 ) * 2 ) {
		bool odd = round_num % 2 == 1;
		bomb_state.attacking_team = odd ? initial_attackers : initial_defenders;
		bomb_state.defending_team = odd ? initial_defenders : initial_attackers;
		return;
	}

	bool first_half = round_num < limit;
	bomb_state.attacking_team = first_half ? initial_attackers : initial_defenders;
	bomb_state.defending_team = first_half ? initial_defenders : initial_attackers;
}

static void NewGame() {
	server_gs.gameState.round_num = 0;
	SetTeams();

	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		G_SpawnQueue_SetTeamSpawnsystem( team, SPAWNSYSTEM_HOLD, 0, 0, true );

		for( int i = 0; i < server_gs.gameState.teams[ team ].num_players; i++ ) {
			int entnum = server_gs.gameState.teams[ team ].player_indices[ i ];
			*G_ClientGetStats( PLAYERENT( entnum - 1 ) ) = { };
			G_ClientRespawn( &game.edicts[ entnum ], true );
		}
	}

	RoundNewState( RoundState_Countdown );
}

static void RoundWonBy( int winner ) {
	int loser = winner == bomb_state.attacking_team ? bomb_state.defending_team : bomb_state.attacking_team;
	TempAllocator temp = svs.frame_arena.temp();
	G_CenterPrintMsg( NULL, S_COLOR_CYAN "%s", winner == bomb_state.attacking_team ? "OFFENSE WINS!" : "DEFENSE WINS!" );

	G_AnnouncerSound( NULL, "sounds/announcer/bomb/team_scored", winner, true, NULL );
	G_AnnouncerSound( NULL, "sounds/announcer/bomb/enemy_scored", loser, true, NULL );

	server_gs.gameState.teams[ winner ].score++;

	RoundNewState( RoundState_Finished );
}

static void EndGame() {
	RoundNewState( RoundState_None );

	level.gametype.countdownEnabled = false;
	RespawnAllPlayers( true );

	// GENERIC_UpdateMatchScore();

	G_AnnouncerSound( NULL, "sounds/announcer/game_over", GS_MAX_TEAMS, true, NULL );
}

static bool ScoreLimitHit() {
	return G_Match_ScorelimitHit() && Abs( int( server_gs.gameState.teams[ TEAM_ALPHA ].score ) - int( server_gs.gameState.teams[ TEAM_BETA ].score ) ) > 1;
}

static void SetRoundType() {
	RoundType type = RoundType_Normal;

	u32 limit = g_scorelimit->integer;

	u8 alpha_score = server_gs.gameState.teams[ TEAM_ALPHA ].score;
	u8 beta_score = server_gs.gameState.teams[ TEAM_BETA ].score;
	bool match_point = alpha_score == limit - 1 || beta_score == limit - 1;
	bool overtime = server_gs.gameState.round_num > ( limit - 1 ) * 2;

	if( overtime ) {
		type = alpha_score == beta_score ? RoundType_Overtime : RoundType_OvertimeMatchPoint;
	}
	else if( match_point ) {
		type = RoundType_MatchPoint;
	}

	server_gs.gameState.round_type = type;
}

static void RoundNewState( RoundState state ) {
	if( state > RoundState_Post ) {
		state = RoundState_Countdown;
	}

	server_gs.gameState.round_state = state;
	bomb_state.round_start_time = level.time;

	switch( state ) {
		case RoundState_None:
			break;

		case RoundState_Countdown: {
			server_gs.gameState.round_num++;
			bomb_state.countdown = countdown_max;
			SetTeams();
			bomb_state.round_check_end = true;
			bomb_state.round_state_end = level.time + 5000;
			level.gametype.removeInactivePlayers = false;
			server_gs.gameState.bomb.exploding = false;
			bomb_state.was_1vx = false;
			ResetBombSites();
			G_ResetLevel();
			ResetBomb();
			ResetKillCounters();
			RespawnAllPlayers();
			DisableMovement();
			SetRoundType();
			BombGiveToRandom();
		} break;

		case RoundState_Round: {
			bomb_state.round_check_end = true;
			bomb_state.round_state_end = level.time + int( g_bomb_roundtime->value * 1000.0f );
			level.gametype.removeInactivePlayers = true;
			EnableMovement();
			Announce( BombAnnouncement_RoundStarted );
		} break;

		case RoundState_Finished: {
			bomb_state.round_check_end = true;
			bomb_state.round_state_end = level.time + 1500;
		} break;

		case RoundState_Post: {
			if( ScoreLimitHit() ) {
				G_Match_LaunchState( MatchState( server_gs.gameState.match_state + 1 ) );
				return;
			}

			bomb_state.round_check_end = true;
			bomb_state.round_state_end = level.time + 3000;
		} break;
	}
}

static void RoundThink() {
	if( server_gs.gameState.round_state == RoundState_None ) {
		return;
	}

	if( server_gs.gameState.round_state == RoundState_Countdown ) {
		int remaining_seconds = int( ( bomb_state.round_state_end - level.time ) * 0.001f ) + 1;

		if( remaining_seconds < 0 ) {
			remaining_seconds = 0;
		}

		if( remaining_seconds < bomb_state.countdown ) {
			bomb_state.countdown = remaining_seconds;

			if( remaining_seconds == countdown_max ) {
				G_AnnouncerSound( NULL, "sounds/announcer/ready", GS_MAX_TEAMS, false, NULL );
			}
			else {
				if( remaining_seconds < 4 ) {
					TempAllocator temp = svs.frame_arena.temp();
					G_AnnouncerSound( NULL, StringHash( temp( "sounds/announcer/{}", remaining_seconds ) ), GS_MAX_TEAMS, false, NULL );
				}
			}
		}

		server_gs.gameState.bomb.alpha_players_total = PlayersAliveOnTeam( TEAM_ALPHA );
		server_gs.gameState.bomb.beta_players_total = PlayersAliveOnTeam( TEAM_BETA );

		bomb_state.last_time = bomb_state.round_state_end - level.time + int( g_bomb_roundtime->value * 1000.0f );
		server_gs.gameState.clock_override = bomb_state.last_time;
	}

	if( bomb_state.round_check_end && level.time > bomb_state.round_state_end ) {
		if( server_gs.gameState.round_state == RoundState_Round && bomb_state.bomb.state != BombState_Planted ) {
			RoundWonBy( bomb_state.defending_team );
			bomb_state.last_time = 1; // kinda hacky, this shows at 0:00
			G_CenterPrintMsg( NULL, S_COLOR_RED "Timelimit Hit!" );
			return;
		}
		else {
			RoundNewState( RoundState( server_gs.gameState.round_state + 1 ) );
			return;
		}
	}

	if( server_gs.gameState.round_state == RoundState_Round ) {
		// monitor the bomb's health
		if( bomb_state.bomb.model == NULL || !bomb_state.bomb.model->r.inuse ) {
			BombModelCreate();
			RoundWonBy( bomb_state.defending_team );
			G_CenterPrintMsg( NULL, S_COLOR_RED "The attacking team has lost the bomb!!!" );
			return;
		}
		if( bomb_state.bomb.state == BombState_Planted ) {
			bomb_state.last_time = -1;
		}
		else {
			bomb_state.last_time = bomb_state.round_state_end - level.time;
		}

		server_gs.gameState.clock_override = bomb_state.last_time;
	}
	else {
		if( bomb_state.bomb.state == BombState_Planting ) {
			SetTeamProgress( bomb_state.attacking_team, 0, BombProgress_Nothing );
			BombPickup();
		}
		if( bomb_state.defuse_progress > 0 ) {
			SetTeamProgress( bomb_state.defending_team, 0, BombProgress_Defusing );
		}

		server_gs.gameState.clock_override = bomb_state.last_time;
	}

	if( server_gs.gameState.round_state >= RoundState_Round ) {
		BombThink();
	}
}

static edict_t * RandomEntity( StringHash classname ) {
	edict_t * ent = NULL;
	DynamicArray< edict_t * > ent_list( sys_allocator );
	while( true ) {
		ent = G_Find( ent, &edict_t::classname, classname );
		if( ent == NULL ) {
			break;
		}
		ent_list.add( ent );
	}
	if( ent_list.size() == 0 ) {
		return NULL;
	}
	return RandomElement( &svs.rng, ent_list.ptr(), ent_list.size() );
}

// main.as

static void UpdateScore( s32 player_num ) {
	score_stats_t * stats = G_ClientGetStats( PLAYERENT( player_num ) );
	stats->score = int( stats->kills * 0.5 + stats->total_damage_given * 0.01 );
}

static void SetTeamProgress( int team, int percent, BombProgress type ) {
	for( u32 i = 0; i < server_gs.gameState.teams[ team ].num_players; i++ ) {
		edict_t * ent = PLAYERENT( server_gs.gameState.teams[ team ].player_indices[ i ] - 1 );
		gclient_t * client = ent->r.client;
		if( G_ISGHOSTING( ent ) ) {
			client->ps.progress = 0;
			client->ps.progress_type = BombProgress_Nothing;
			continue;
		}
		client->ps.progress = percent;
		client->ps.progress_type = type;
	}
}

static bool GT_Bomb_Command( gclient_t * client, const char * cmd_, const char * args_, int argc ) {
	if( cmd_ == NULL || args_ == NULL ) {
		return false;
	}
	Span< const char > cmd = MakeSpan( cmd_ );
	Span< const char > args = MakeSpan( args_ );

	if( cmd == "drop" ) {
		if( PLAYERNUM( client ) == bomb_state.carrier && bomb_state.bomb.state == BombState_Carried ) {
			BombDrop( BombDropReason_Normal );
		}
		return true;
	}

	if( cmd == "gametypemenu" ) {
		ShowShop( PLAYERNUM( client ) );
		return true;
	}

	if( cmd == "weapselect" ) {
		SetLoadout( PLAYERENT( PLAYERNUM( client ) ), args );
		return true;
	}

	return false;
}

static edict_t * GT_Bomb_SelectSpawnPoint( edict_t * ent ) {
	edict_t * spawn = RandomEntity( "spawn_gladiator" );
	if( spawn != NULL ) {
		return spawn;
	}
	if( ent->s.team == bomb_state.attacking_team ) {
		spawn = RandomEntity( "spawn_bomb_attacking" );
		if( spawn != NULL ) {
			return spawn;
		}
		return RandomEntity( "team_CTF_betaspawn" );
	}

	spawn = RandomEntity( "spawn_bomb_defending" );
	if( spawn != NULL ) {
		return spawn;
	}
	return RandomEntity( "team_CTF_alphaspawn" );
}

static void GT_Bomb_PlayerConnected( edict_t * ent ) {
	char * loadout = Info_ValueForKey( ent->r.client->userinfo, "cg_loadout" );
	if( loadout != NULL ) {
		SetLoadout( ent, MakeSpan( loadout ) );
	}
}

static void GT_Bomb_PlayerRespawning( edict_t * ent ) {
	if( PLAYERNUM( ent ) == bomb_state.carrier ) {
		BombDrop( BombDropReason_ChangingTeam );
	}
}

static void GT_Bomb_PlayerRespawned( edict_t * ent, int old_team, int new_team ) {
	gclient_t * client = ent->r.client;
	client->ps.pmove.features |= PMFEAT_TEAMGHOST;

	MatchState match_state = server_gs.gameState.match_state;
	RoundState round_state = server_gs.gameState.round_state;

	if( match_state <= MatchState_Warmup ) {
		client->ps.can_change_loadout = new_team >= TEAM_ALPHA;
	}

	if( new_team != old_team && match_state == MatchState_Playing ) {
		if( old_team != TEAM_SPECTATOR ) {
			CheckPlayersAlive( old_team );
		}

		if( round_state == RoundState_Countdown && new_team != TEAM_SPECTATOR ) {
			G_ClientRespawn( ent, false ); // TODO: why does this not really respawn them?
			return;
		}
	}

	if( new_team == TEAM_SPECTATOR ) {
		return;
	}

	if( match_state == MatchState_Warmup ) {
		G_RespawnEffect( ent );
	}

	if( round_state == RoundState_Countdown ) {
		DisableMovementFor( PLAYERNUM( ent ) );
	}

	GiveInventory( ent );
}

static void GT_Bomb_PlayerKilled( edict_t * victim, edict_t * attacker, edict_t * inflictor ) {
	if( server_gs.gameState.match_state != MatchState_Playing || server_gs.gameState.round_state >= RoundState_Finished ) {
		return;
	}

	if( PLAYERNUM( victim ) == bomb_state.carrier ) {
		BombDrop( BombDropReason_Killed );
	}

	if( attacker != NULL && attacker->r.client != NULL && attacker->s.team != victim->s.team ) {
		bomb_state.kills_this_round[ PLAYERNUM( attacker ) ]++;

		u32 required_for_ace = attacker->s.team == TEAM_ALPHA ? server_gs.gameState.bomb.beta_players_total : server_gs.gameState.bomb.alpha_players_total;
		if( required_for_ace >= 3 && bomb_state.kills_this_round[ PLAYERNUM( attacker ) ] == required_for_ace ) {
			G_AnnouncerSound( NULL, "sounds/announcer/bomb/ace", GS_MAX_TEAMS, false, NULL );
		}
	}

	CheckPlayersAlive( victim->s.team );
}

static void GT_Bomb_Think() {
	if( G_Match_TimelimitHit() ) {
		G_Match_LaunchState( MatchState( server_gs.gameState.match_state + 1 ) );
		G_ClearCenterPrint( NULL );
	}

	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		UpdateScore( i );
	}

	if( server_gs.gameState.match_state < MatchState_Playing ) {
		return;
	}

	RoundThink();

	u32 aliveAlpha = PlayersAliveOnTeam( TEAM_ALPHA );
	u32 aliveBeta = PlayersAliveOnTeam( TEAM_BETA );

	server_gs.gameState.bomb.alpha_players_alive = aliveAlpha;
	server_gs.gameState.bomb.beta_players_alive = aliveBeta;

	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		edict_t * ent = PLAYERENT( i );
		gclient_t * client = ent->r.client;
		if( PF_GetClientState( i ) != CS_SPAWNED ) {
			continue;
		}

		client->ps.can_change_loadout = !G_ISGHOSTING( ent ) && server_gs.gameState.round_state == RoundState_Countdown;
		client->ps.carrying_bomb = false;
		client->ps.can_plant = false;
	}

	if( bomb_state.bomb.state == BombState_Carried ) {
		edict_t * carrier = PLAYERENT( bomb_state.carrier );
		carrier->r.client->ps.carrying_bomb = true;
		carrier->r.client->ps.can_plant = bomb_state.carrier_can_plant_time >= level.time - 50;
	}

	// GENERIC_UpdateMatchScore(); TODO
}

static bool GT_Bomb_MatchStateFinished( MatchState incomingMatchState ) {
	if( server_gs.gameState.match_state <= MatchState_Warmup && incomingMatchState > MatchState_Warmup && incomingMatchState < MatchState_PostMatch ) {
		G_Match_Autorecord_Start();
	}

	if( server_gs.gameState.match_state == MatchState_PostMatch ) {
		G_Match_Autorecord_Stop();
	}

	return true;
}

static void GT_Bomb_MatchStateStarted() {
	switch( server_gs.gameState.match_state ) {
		case MatchState_Warmup:
			level.gametype.countdownEnabled = false;

			bomb_state.attacking_team = initial_attackers;
			bomb_state.defending_team = initial_defenders;

			for( int t = TEAM_PLAYERS; t < GS_MAX_TEAMS; t++ ) {
				G_SpawnQueue_SetTeamSpawnsystem( t, SPAWNSYSTEM_INSTANT, 0, 0, false );
			}
			break;

		case MatchState_Countdown:
			level.gametype.countdownEnabled = true;
			G_AnnouncerSound( NULL, "sounds/announcer/get_ready_to_fight", GS_MAX_TEAMS, false, NULL );
			break;

		case MatchState_Playing:
			NewGame();
			break;

		case MatchState_PostMatch:
			EndGame();
			break;

		case MatchState_WaitExit:
			break;
	}
}

static void GT_Bomb_SpawnGametype() {
	BombInit();

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( ent->r.inuse ) {
			GT_Bomb_PlayerConnected( ent );
		}
	}
}

static void GT_Bomb_InitGametype() {
	bomb_state = { };

	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		G_SpawnQueue_SetTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );
	}

	G_AddCommand( "drop", NULL ); // TODO: this sucks
	G_AddCommand( "gametypemenu", NULL );
	G_AddCommand( "weapselect", NULL );

	bomb_state.carrier = -1;
	bomb_state.defuser = -1;

	g_bomb_roundtime = Cvar_Get( "g_bomb_roundtime", "61", CVAR_ARCHIVE );
	g_bomb_bombtimer = Cvar_Get( "g_bomb_bombtimer", "30", CVAR_ARCHIVE );
	g_bomb_armtime = Cvar_Get( "g_bomb_armtime", "1", CVAR_ARCHIVE );
	g_bomb_defusetime = Cvar_Get( "g_bomb_defusetime", "4", CVAR_ARCHIVE );
}

static void GT_Bomb_Shutdown() {
}

static bool GT_Bomb_SpawnEntity( StringHash classname, edict_t * ent ) {
	if( classname == "bomb_site" || classname == "misc_capture_area_indicator" ) {
		SpawnBombSite( ent );
		return true;
	}

	if( classname == "plant_area" || classname == "trigger_capture_area" ) {
		SpawnPlantArea( ent );
		return true;
	}

	if( classname == "spawn_bomb_attacking" || classname == "spawn_bomb_defending" ) {
		DropSpawnToFloor( ent );
		return true;
	}

	return false;
}

Gametype GetBombGametype() {
	Gametype gt = { };

	gt.Init = GT_Bomb_InitGametype;
	gt.Init2 = GT_Bomb_SpawnGametype;
	gt.MatchStateStarted = GT_Bomb_MatchStateStarted;
	gt.MatchStateFinished = GT_Bomb_MatchStateFinished;
	gt.Think = GT_Bomb_Think;
	gt.PlayerConnected = GT_Bomb_PlayerConnected;
	gt.PlayerRespawning = GT_Bomb_PlayerRespawning;
	gt.PlayerRespawned = GT_Bomb_PlayerRespawned;
	gt.PlayerKilled = GT_Bomb_PlayerKilled;
	gt.SelectSpawnPoint = GT_Bomb_SelectSpawnPoint;
	gt.Command = GT_Bomb_Command;
	gt.Shutdown = GT_Bomb_Shutdown;
	gt.SpawnEntity = GT_Bomb_SpawnEntity;

	gt.isTeamBased = true;
	gt.countdownEnabled = false;
	gt.removeInactivePlayers = true;
	gt.selfDamage = true;

	return gt;
}
