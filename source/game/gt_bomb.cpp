#include "qcommon/base.h"
#include "game/g_local.h"
#include "gameshared/collision.h"

enum BombState {
	BombState_Carried,
	BombState_Dropped,
	BombState_Planting,
	BombState_Planted,
	BombState_Exploding,
	BombState_Defused,
};

enum BombDropReason {
	BombDropReason_Normal,
	BombDropReason_Killed,
	BombDropReason_ChangingTeam,
};

static constexpr float bomb_max_plant_speed = 50.0f;
static constexpr float bomb_max_plant_height = 100.0f;
static constexpr u32 bomb_drop_retake_delay = 1000;
static constexpr float bomb_arm_defuse_radius = 36.0f;
static constexpr float bomb_throw_speed = 550.0f;
static constexpr u32 bomb_explosion_effect_radius = 256;
static constexpr Team initial_attackers = Team_One;
static constexpr Team initial_defenders = Team_Two;
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

static Cvar * g_bomb_roundtime;
static Cvar * g_bomb_bombtimer;

static const float bomb_armtime = 1000.0f; //ms
static const float bomb_defusetime = 4000.0f; //ms
static const u32 max_sites = 26;
static const int countdown_max = 6;

struct BombSite {
	edict_t * indicator;
	edict_t * hud;
	char letter;
};

static struct {
	bool round_check_end;
	s64 round_start_time;
	s64 round_state_start;
	s64 round_state_end;
	s32 countdown;
	s32 last_time;
	bool was_1vx;

	// TODO: player.as
	u32 kills_this_round[ MAX_CLIENTS ];

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

	RespawnQueues respawn_queues;

	BombSite sites[ max_sites ];
	u32 num_sites;
	u32 site;
} bomb_state;

static bool BombCanPlant();
static void BombStartPlanting( edict_t * carrier_ent, u32 site );
static void BombSetCarrier( s32 player_num, bool no_sound );
static void RoundWonBy( Team winner );
static void RoundNewState( RoundState state );
static void SetTeamProgress( Team team, int percent, BombProgress type );
static void UpdateScore( s32 player_num );

static void Show( edict_t * ent ) {
	ent->s.svflags &= ~SVF_NOCLIENT;
	GClip_LinkEntity( ent );
}

static void Hide( edict_t * ent ) {
	ent->s.svflags |= SVF_NOCLIENT;
}

static bool EntCanSee( edict_t * ent, Vec3 point ) {
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &ent->s );
	Vec3 center = ent->s.origin + Center( bounds );
	trace_t trace = G_Trace( center, MinMax3( 0.0f ), point, ent, SolidMask_AnySolid );
	return trace.HitNothing();
}

static s32 FirstNearbyTeammate( Vec3 origin, Team team ) {
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

static Team OtherTeam( Team team ) {
	return team == initial_attackers ? initial_defenders : initial_attackers;
}

static Team AttackingTeam() {
	return server_gs.gameState.bomb.attacking_team;
}

static Team DefendingTeam() {
	return OtherTeam( AttackingTeam() );
}

static void Announce( BombAnnouncement announcement ) {
	G_AnnouncerSound( NULL, snd_announcements_off[ announcement ], AttackingTeam(), true, NULL );
	G_AnnouncerSound( NULL, snd_announcements_def[ announcement ], DefendingTeam(), true, NULL );
}

static void ResetKillCounters() {
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		bomb_state.kills_this_round[ i ] = 0;
	}
}

// site.as

static void BombSiteCarrierTouched( u32 site ) {
	bomb_state.carrier_can_plant_time = level.time;
	if( BombCanPlant() ) {
		edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
		Vec3 velocity = carrier_ent->velocity;
		if( ( carrier_ent->r.client->ucmd.buttons & Button_Plant ) != 0 && level.time - bomb_state.bomb.action_time >= 1000 && Length( velocity ) < bomb_max_plant_speed ) {
			BombStartPlanting( carrier_ent, site );
		}
	}
}

static u32 GetSiteFromIndicator( edict_t * ent ) {
	for( u32 i = 0; i < bomb_state.num_sites; i++ ) {
		if( bomb_state.sites[ i ].indicator == ent ) {
			return i;
		}
	}

	Assert( false );
	return 0;
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

	BombSite * site = &bomb_state.sites[ i ];

	site->indicator = ent;
	site->indicator->s.model = EMPTY_HASH;
	site->indicator->nextThink = level.time + 1;
	GClip_LinkEntity( site->indicator );

	site->hud = G_Spawn();
	site->hud->classname = "hud_bomb_site";
	site->hud->s.type = ET_BOMB_SITE;
	site->hud->s.solidity = Solid_NotSolid;
	site->hud->s.origin = bomb_state.sites[ i ].indicator->s.origin;
	site->hud->s.svflags = EntityFlags( 0 );
	site->hud->s.site_letter = letter;
	GClip_LinkEntity( site->hud );

	site->letter = letter;

	bomb_state.num_sites++;
}

static void PlantAreaThink( edict_t * ent ) {
	edict_t * target = G_Find( NULL, &edict_t::name, ent->target );
	if( target != NULL ) {
		ent->s.ownerNum = GetSiteFromIndicator( target );
		return;
	}
	Com_GGPrint( "plant_area at {} has no targets, removing...", ent->s.origin );
	G_FreeEdict( ent );
}

static void PlantAreaTouch( edict_t * self, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( other->r.client == NULL ) {
		return;
	}

	if( server_gs.gameState.match_state != MatchState_Playing || server_gs.gameState.round_state != RoundState_Round ) {
		return;
	}

	if( bomb_state.bomb.state != BombState_Carried || PLAYERNUM( other ) != bomb_state.carrier ) {
		return;
	}

	BombSiteCarrierTouched( self->s.ownerNum );
}

static void SpawnPlantArea( edict_t * ent ) {
	ent->think = PlantAreaThink;
	ent->touch = PlantAreaTouch;
	ent->s.solidity = Solid_Trigger;
	GClip_LinkEntity( ent );

	ent->nextThink = level.time + 1;
}

// bomb.as

static void BombTouch( edict_t * self, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( server_gs.gameState.match_state != MatchState_Playing ) {
		return;
	}

	if( bomb_state.bomb.state != BombState_Dropped ) {
		return;
	}

	if( other->r.client == NULL ) {
		return;
	}

	if( other->s.team != AttackingTeam() ) {
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
		bomb_state.bomb.hud->s.svflags |= SVF_ONLYTEAM;
		bomb_state.bomb.hud->s.radius = BombDown_Dropped;
		Show( bomb_state.bomb.hud );
	}
}

static void SpawnBomb() {
	bomb_state.bomb.model = G_Spawn();
	bomb_state.bomb.model->classname = "bomb";
	bomb_state.bomb.model->s.type = ET_GENERIC;
	bomb_state.bomb.model->s.team = AttackingTeam();

	bomb_state.bomb.model->s.override_collision_model = CollisionModelAABB( bomb_bounds );

	bomb_state.bomb.model->s.solidity = Solid_World | Solid_Trigger;
	bomb_state.bomb.model->s.model = model_bomb;
	bomb_state.bomb.model->s.effects |= EF_TEAM_SILHOUETTE;
	bomb_state.bomb.model->s.silhouetteColor = RGBA8( 255, 255, 255, 255 );
	bomb_state.bomb.model->touch = BombTouch;
	bomb_state.bomb.model->stop = BombStop;

	bomb_state.bomb.action_time = -1;
}

static void SpawnBombHUD() {
	bomb_state.bomb.hud = G_Spawn();
	bomb_state.bomb.hud->classname = "hud_bomb";
	bomb_state.bomb.hud->s.type = ET_BOMB;
	bomb_state.bomb.hud->s.team = AttackingTeam();
	bomb_state.bomb.hud->s.solidity = Solid_NotSolid;
}

static void BombPickup() {
	PLAYERENT( bomb_state.carrier )->s.effects |= EF_CARRIER;
	PLAYERENT( bomb_state.carrier )->s.model2 = model_bomb;

	Hide( bomb_state.bomb.model );
	Hide( bomb_state.bomb.hud );

	bomb_state.bomb.model->movetype = MOVETYPE_NONE;
	bomb_state.bomb.model->s.solidity = Solid_NotSolid;
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
		G_AnnouncerSound( PLAYERENT( player_num ), "sounds/announcer/bomb/offense/taken", AttackingTeam(), true, NULL );
	}
}

static void DropBomb( BombDropReason reason ) {
	SetTeamProgress( AttackingTeam(), 0, BombProgress_Nothing );
	edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
	Vec3 start = carrier_ent->s.origin + Vec3( 0.0f, 0.0f, carrier_ent->viewheight );
	Vec3 end( 0.0f );
	Vec3 velocity( 0.0f );

	switch( reason ) {
		case BombDropReason_Normal:
		case BombDropReason_Killed: {
			bomb_state.bomb.pick_time = level.time + bomb_drop_retake_delay;
			bomb_state.bomb.dropper = bomb_state.carrier;
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

	trace_t trace = G_Trace( start, bomb_bounds, end, carrier_ent, SolidMask_AnySolid );

	bomb_state.bomb.model->movetype = MOVETYPE_TOSS;
	bomb_state.bomb.model->r.owner = carrier_ent;
	bomb_state.bomb.model->s.origin = trace.endpos;
	bomb_state.bomb.model->velocity = velocity;
	bomb_state.bomb.model->s.solidity = Solid_World | Solid_Trigger;
	Show( bomb_state.bomb.model );
	RemoveCarrier();
	bomb_state.bomb.state = BombState_Dropped;
}

static void BombStartPlanting( edict_t * carrier_ent, u32 site ) {
	// TODO
	carrier_ent->r.client->ps.pmove.max_speed = 0;

	bomb_state.site = site;

	Vec3 start = carrier_ent->s.origin + Vec3( 0.0f, 0.0f, carrier_ent->viewheight );

	Vec3 end = start;
	end.z -= 512.0f;

	trace_t trace = G_Trace( start, bomb_bounds, end, carrier_ent, SolidMask_AnySolid );

	EulerDegrees3 angles( 0.0f, RandomUniformFloat( &svs.rng, 0.0f, 360.0f ), 0.0f );

	bomb_state.bomb.model->s.origin = trace.endpos;
	bomb_state.bomb.model->s.angles = angles;
	Show( bomb_state.bomb.model );

	bomb_state.bomb.hud->s.origin = trace.endpos + Vec3( 0.0f, 0.0f, bomb_hud_offset );
	bomb_state.bomb.hud->s.angles = angles;
	bomb_state.bomb.hud->s.svflags |= SVF_ONLYTEAM;
	bomb_state.bomb.hud->s.radius = BombDown_Planting;
	Show( bomb_state.bomb.hud );

	carrier_ent->s.effects &= ~EF_CARRIER;
	carrier_ent->s.model2 = EMPTY_HASH;

	bomb_state.bomb.action_time = level.time;
	bomb_state.bomb.state = BombState_Planting;

	G_Sound( bomb_state.bomb.model, "models/bomb/plant" );
}

static void BombPlanted() {
	edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
	carrier_ent->r.client->ps.pmove.max_speed = -1;

	bomb_state.bomb.action_time = level.time + int( g_bomb_bombtimer->number * 1000.0f );
	bomb_state.bomb.model->s.sound = "models/bomb/fuse";
	bomb_state.bomb.model->s.effects &= ~EF_TEAM_SILHOUETTE;

	// show to defs too
	bomb_state.bomb.hud->s.svflags &= ~SVF_ONLYTEAM;

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

	Hide( bomb_state.bomb.hud );

	Announce( BombAnnouncement_Defused );

	bomb_state.bomb.state = BombState_Defused;

	TempAllocator temp = svs.frame_arena.temp();
	G_PrintMsg( NULL, "%s defused the bomb!\n", PLAYERENT( bomb_state.defuser )->r.client->name );

	G_Sound( bomb_state.bomb.model, "models/bomb/tss" );

	RoundWonBy( DefendingTeam() );

	bomb_state.defuser = -1;
}

static void BombExplode() {
	if( server_gs.gameState.round_state == RoundState_Round ) {
		RoundWonBy( AttackingTeam() );
	}

	Hide( bomb_state.bomb.hud );
	Hide( bomb_state.bomb.model );

	bomb_state.bomb.killed_everyone = false;

	bomb_state.bomb.state = BombState_Exploding;
	bomb_state.defuser = -1;

	server_gs.gameState.exploding = true;
	server_gs.gameState.exploded_at = svs.gametime; // TODO: only place where gameTime is used, dno

	G_SpawnEvent( EV_BOMB_EXPLOSION, bomb_explosion_effect_radius, &bomb_state.bomb.model->s.origin );

	G_Sound( bomb_state.bomb.model, "models/bomb/explode" );
}

static void BombThink() {
	switch( bomb_state.bomb.state ) {
		case BombState_Carried:
			return;

		case BombState_Dropped: {
			// respawn bomb if it falls in the void
			if( bomb_state.bomb.model->s.origin.z < -1024.0f ) {
				G_FreeEdict( bomb_state.bomb.model );
			}
		} break;

		case BombState_Planting: {
			edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
			if( !EntCanSee( carrier_ent, bomb_state.bomb.model->s.origin ) ||
				Length( carrier_ent->s.origin - bomb_state.bomb.model->s.origin ) > bomb_arm_defuse_radius ||
				!( carrier_ent->r.client->ucmd.buttons & Button_Plant ) ) {
				carrier_ent->r.client->ps.pmove.max_speed = -1;
				SetTeamProgress( AttackingTeam(), 0, BombProgress_Nothing );
				BombPickup();
				break;
			}

			float frac = float( level.time - bomb_state.bomb.action_time ) / bomb_armtime;
			if( frac >= 1.0f ) {
				SetTeamProgress( AttackingTeam(), 0, BombProgress_Nothing );
				BombPlanted();
				break;
			}

			if( frac != 0.0f ) {
				SetTeamProgress( AttackingTeam(), int( frac * 100.0f ), BombProgress_Planting );
			}
		} break;

		case BombState_Planted: {
			float animation_time = ( g_bomb_bombtimer->number - ( bomb_state.bomb.action_time - level.time ) * 0.001f ) / g_bomb_bombtimer->number;
			bomb_state.bomb.model->s.animation_time = animation_time;
			bomb_state.bomb.hud->s.animation_time = animation_time; // dno if this is needed?

			if( bomb_state.defuser == -1 ) {
				bomb_state.defuser = FirstNearbyTeammate( bomb_state.bomb.model->s.origin, DefendingTeam() );
			}

			if( bomb_state.defuser != -1 ) {
				edict_t * defuser_ent = PLAYERENT( bomb_state.defuser );
				if( G_ISGHOSTING( defuser_ent ) || !EntCanSee( defuser_ent, bomb_state.bomb.model->s.origin ) || Length( defuser_ent->s.origin - bomb_state.bomb.model->s.origin ) > bomb_arm_defuse_radius ) {
					bomb_state.defuser = -1;
				}
			}

			if( bomb_state.defuser == -1 && bomb_state.defuse_progress > 0 ) {
				bomb_state.defuse_progress = 0;
				SetTeamProgress( DefendingTeam(), 0, BombProgress_Nothing );
			}
			else {
				bomb_state.defuse_progress += game.frametime;
				float frac = bomb_state.defuse_progress / bomb_defusetime;
				if( frac >= 1.0f ) {
					BombDefused();
					SetTeamProgress( DefendingTeam(), 100, BombProgress_Defusing );
					break;
				}
				SetTeamProgress( DefendingTeam(), int( frac * 100.0f ), BombProgress_Defusing );
			}

			if( level.time >= bomb_state.bomb.action_time ) {
				BombExplode();
			}
		} break;

		case BombState_Exploding: {
			// BombSiteStepExplosion( bomb_state.site );
			if( !bomb_state.bomb.killed_everyone ) {
				bomb_state.bomb.model->projectileInfo.maxDamage = 1.0f;
				bomb_state.bomb.model->projectileInfo.minDamage = 1.0f;
				bomb_state.bomb.model->projectileInfo.maxKnockback = 400.0f;
				bomb_state.bomb.model->projectileInfo.minKnockback = 1.0f;
				bomb_state.bomb.model->projectileInfo.radius = 9999;

				// apply a 1 damage explosion just for the kb
				G_RadiusDamage( bomb_state.bomb.model, NULL, Vec3( 0.0f, 0.0f, 1.0f ), bomb_state.bomb.model, WorldDamage_Explosion );

				for( int i = 0; i < server_gs.maxclients; i++ ) {
					G_Damage( PLAYERENT( i ), world, world, Vec3( 0.0f ), Vec3( 0.0f ), bomb_state.bomb.model->s.origin, 100.0f, 0.0f, 0, WorldDamage_Explosion );
				}

				bomb_state.bomb.killed_everyone = true;
			}
		} break;

		case BombState_Defused:
			break;
	}
}

static bool BombCanPlant() {
	edict_t * carrier_ent = PLAYERENT( bomb_state.carrier );
	Vec3 start = carrier_ent->s.origin;
	Vec3 end = start;
	end.z -= bomb_max_plant_height;
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &carrier_ent->s );

	trace_t trace = G_Trace( start, bounds, end, carrier_ent, SolidMask_AnySolid );

	return ISWALKABLEPLANE( trace.normal );
}

static void BombGiveToRandom() {
	int total_num_players = server_gs.gameState.teams[ AttackingTeam() ].num_players;
	int num_bots = 0;
	int num_players = total_num_players;
	for( int i = 0; i < total_num_players; i++ ) {
		s32 player_num = server_gs.gameState.teams[ AttackingTeam() ].player_indices[ i ] - 1;
		edict_t * ent = PLAYERENT( player_num );
		if( ent->s.type == ET_GHOST ) {
			G_DebugPrint( "BombGiveToRandom: %s spectating", server_gs.gameState.players[ PLAYERNUM( ent ) ] );
			num_players--;
		}
		else if( HasAnyBit( ent->s.svflags, SVF_FAKECLIENT ) ) {
			G_DebugPrint( "BombGiveToRandom: %s bot", server_gs.gameState.players[ PLAYERNUM( ent ) ] );
			num_bots++;
		}
	}

	bool all_bots = num_bots == num_players;
	s32 carrier = RandomUniform( &svs.rng, 0, all_bots ? num_players : num_players - num_bots );
	G_DebugPrint( "BombGiveToRandom: picked %d", carrier );
	s32 seen = 0;

	for( int i = 0; i < total_num_players; i++ ) {
		edict_t * ent = &game.edicts[ server_gs.gameState.teams[ AttackingTeam() ].player_indices[ i ] ];
		s32 player_num = PLAYERNUM( ent );
		if( ent->s.type != ET_GHOST && ( all_bots || !HasAnyBit( ent->s.svflags, SVF_FAKECLIENT ) ) ) {
			if( seen == carrier ) {
				G_DebugPrint( "BombGiveToRandom: picked %s", server_gs.gameState.players[ player_num ] );
				BombSetCarrier( player_num, true );
				break;
			}
			else {
				G_DebugPrint( "BombGiveToRandom: %s (%d) wasn't it", server_gs.gameState.players[ PLAYERNUM( ent ) ], seen );
			}
			seen++;
		}
		else {
			G_DebugPrint( "BombGiveToRandom: %s doesn't count at all", server_gs.gameState.players[ PLAYERNUM( ent ) ] );
		}
	}
}

// round.as

static void EnableMovementFor( s32 playernum ) {
	edict_t * ent = PLAYERENT( playernum );
	ent->r.client->ps.pmove.max_speed = -1;
	ent->r.client->ps.pmove.features = ent->r.client->ps.pmove.features | PMFEAT_ABILITIES;
}

static void EnableMovement() {
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( PF_GetClientState( i ) >= CS_SPAWNED && ent->s.team != Team_None ) {
			EnableMovementFor( i );
		}
	}
}

static void DisableMovementFor( s32 playernum ) {
	edict_t * ent = PLAYERENT( playernum );
	ent->r.client->ps.pmove.max_speed = 100;
	ent->r.client->ps.pmove.features &= ~( PMFEAT_ABILITIES );
	ent->r.client->ps.pmove.no_shooting_time = 5000;
}

static void DisableMovement() {
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( PF_GetClientState( i ) >= CS_SPAWNED && ent->s.team != Team_None ) {
			DisableMovementFor( i );
		}
	}
}

static void PlayXvXSound( Team team_that_died ) {
	u32 alive = PlayersAliveOnTeam( team_that_died );

	Team other_team = OtherTeam( team_that_died );
	u32 alive_other_team = PlayersAliveOnTeam( other_team );

	if( alive == 1 ) {
		if( alive_other_team == 1 ) {
			if( bomb_state.was_1vx ) {
				G_AnnouncerSound( NULL, "sounds/announcer/bomb/1v1", Team_Count, false, NULL );
			}
		}
		else if( alive_other_team >= 3 ) {
			G_AnnouncerSound( NULL, "sounds/announcer/bomb/1vx", team_that_died, false, NULL );
			G_AnnouncerSound( NULL, "sounds/announcer/bomb/xv1", other_team, false, NULL );
			bomb_state.was_1vx = true;
		}
	}
}

static void SetTeams() {
	u8 round_num = server_gs.gameState.round_num;
	if( server_gs.gameState.scorelimit == 0 || round_num > ( server_gs.gameState.scorelimit - 1 ) * 2 ) {
		bool odd = round_num % 2 == 1;
		server_gs.gameState.bomb.attacking_team = odd ? initial_attackers : initial_defenders;
		return;
	}

	bool first_half = round_num < server_gs.gameState.scorelimit;
	server_gs.gameState.bomb.attacking_team = first_half ? initial_attackers : initial_defenders;
}

static void NewGame() {
	level.gametype.autoRespawn = false;
	server_gs.gameState.round_num = 0;

	RoundNewState( RoundState_Countdown );
}

static void RoundWonBy( Team winner ) {
	Team loser = winner == AttackingTeam() ? DefendingTeam() : AttackingTeam();

	G_AnnouncerSound( NULL, "sounds/announcer/bomb/team_scored", winner, true, NULL );
	G_AnnouncerSound( NULL, "sounds/announcer/bomb/enemy_scored", loser, true, NULL );

	server_gs.gameState.teams[ winner ].score++;

	RoundNewState( RoundState_Finished );
}

static void EndGame() {
	RoundNewState( RoundState_None );
	GhostEveryone();
	G_AnnouncerSound( NULL, "sounds/announcer/game_over", Team_Count, true, NULL );
}

static bool ScoreLimitHit() {
	return G_Match_ScorelimitHit() && Abs( int( server_gs.gameState.teams[ Team_One ].score ) - int( server_gs.gameState.teams[ Team_Two ].score ) ) > 1;
}

static void SetRoundType() {
	RoundType type = RoundType_Normal;

	u8 alpha_score = server_gs.gameState.teams[ Team_One ].score;
	u8 beta_score = server_gs.gameState.teams[ Team_Two ].score;
	bool match_point = alpha_score == server_gs.gameState.scorelimit - 1 || beta_score == server_gs.gameState.scorelimit - 1;
	bool overtime = server_gs.gameState.round_num > ( server_gs.gameState.scorelimit - 1 ) * 2;

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
			server_gs.gameState.exploding = false;
			bomb_state.was_1vx = false;
			ResetBombSites();
			G_ResetLevel();
			SpawnBomb();
			SpawnBombHUD();
			ResetKillCounters();
			GhostEveryone();
			SpawnTeams( &bomb_state.respawn_queues );
			DisableMovement();
			SetRoundType();
			BombGiveToRandom();
			G_SpawnEvent( EV_FLASH_WINDOW, 0, NULL );
			G_SunCycle( 3000 );
		} break;

		case RoundState_Round: {
			bomb_state.round_check_end = true;
			bomb_state.round_state_end = level.time + int( g_bomb_roundtime->number * 1000.0f );
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
				G_Match_LaunchState( MatchState_PostMatch );
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

	RemoveDisconnectedPlayersFromRespawnQueues( &bomb_state.respawn_queues );

	if( server_gs.gameState.round_state == RoundState_Countdown ) {
		int remaining_seconds = int( ( bomb_state.round_state_end - level.time ) * 0.001f ) + 1;

		if( remaining_seconds < 0 ) {
			remaining_seconds = 0;
		}

		if( remaining_seconds < bomb_state.countdown ) {
			bomb_state.countdown = remaining_seconds;

			if( remaining_seconds == countdown_max ) {
				G_AnnouncerSound( NULL, "sounds/announcer/ready", Team_Count, false, NULL );
			}
			else {
				if( remaining_seconds < 4 ) {
					TempAllocator temp = svs.frame_arena.temp();
					G_AnnouncerSound( NULL, StringHash( temp( "sounds/announcer/{}", remaining_seconds ) ), Team_Count, false, NULL );
				}
			}
		}

		server_gs.gameState.bomb.alpha_players_total = PlayersAliveOnTeam( Team_One );
		server_gs.gameState.bomb.beta_players_total = PlayersAliveOnTeam( Team_Two );

		bomb_state.last_time = bomb_state.round_state_end - level.time + int( g_bomb_roundtime->number * 1000.0f );
		server_gs.gameState.clock_override = bomb_state.last_time;
	}

	if( bomb_state.round_check_end && level.time > bomb_state.round_state_end ) {
		if( server_gs.gameState.round_state == RoundState_Round ) {
			if( bomb_state.bomb.state != BombState_Planted ) {
				RoundWonBy( DefendingTeam() );
				bomb_state.last_time = 1; // kinda hacky, this shows at 0:00
				G_CenterPrintMsg( NULL, S_COLOR_RED "Timelimit Hit!" );
				return;
			}
		}
		else {
			RoundNewState( RoundState( server_gs.gameState.round_state + 1 ) );
			return;
		}
	}

	if( server_gs.gameState.round_state == RoundState_Round ) {
		// respawn the bomb if it died
		if( !bomb_state.bomb.model->r.inuse ) {
			SpawnBomb();
			Show( bomb_state.bomb.model );
			bomb_state.bomb.state = BombState_Dropped;

			bomb_state.bomb.model->movetype = MOVETYPE_TOSS;
			bomb_state.bomb.model->s.origin = G_PickRandomEnt( &edict_t::classname, "spawn_bomb_attacking" )->s.origin;
			bomb_state.bomb.model->velocity = Vec3( 0.0f, 0.0f, bomb_throw_speed );

			constexpr StringHash vfx_bomb_respawn = "models/bomb/respawn";

			G_Sound( bomb_state.bomb.model, "models/bomb/respawn" );
			G_SpawnEvent( EV_VFX, vfx_bomb_respawn.hash, &bomb_state.bomb.model->s.origin );

			return;
		}

		// check if everyone on one team died
		u32 attackers_alive = PlayersAliveOnTeam( AttackingTeam() );
		u32 defenders_alive = PlayersAliveOnTeam( DefendingTeam() );

		if( defenders_alive == 0 ) {
			RoundWonBy( AttackingTeam() );
		}
		else if( attackers_alive == 0 && bomb_state.bomb.state != BombState_Planted ) {
			RoundWonBy( DefendingTeam() );
		}

		// set the clock
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
			SetTeamProgress( AttackingTeam(), 0, BombProgress_Nothing );
			BombPickup();
		}
		if( bomb_state.defuse_progress > 0 ) {
			SetTeamProgress( DefendingTeam(), 0, BombProgress_Defusing );
		}

		server_gs.gameState.clock_override = bomb_state.last_time;
	}

	if( server_gs.gameState.round_state >= RoundState_Round ) {
		BombThink();
	}
}

// main.as

static void UpdateScore( s32 player_num ) {
	score_stats_t * stats = G_ClientGetStats( PLAYERENT( player_num ) );
	stats->score = int( stats->kills * 0.5 + stats->total_damage_given * 0.01 );
}

static void SetTeamProgress( Team team, int percent, BombProgress type ) {
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

static const edict_t * Bomb_SelectSpawnPoint( const edict_t * ent ) {
	if( ent->s.team == AttackingTeam() ) {
		edict_t * spawn = G_PickRandomEnt( &edict_t::classname, "spawn_bomb_attacking" );
		if( spawn != NULL ) {
			return spawn;
		}
		return G_PickRandomEnt( &edict_t::classname, "team_CTF_betaspawn" );
	}

	edict_t * spawn = G_PickRandomEnt( &edict_t::classname, "spawn_bomb_defending" );
	if( spawn != NULL ) {
		return spawn;
	}
	return G_PickRandomEnt( &edict_t::classname, "team_CTF_alphaspawn" );
}

static const edict_t * Bomb_SelectDeadcam() {
	if( bomb_state.bomb.state < BombState_Planted )
		return NULL;
	return G_PickTarget( bomb_state.sites[ bomb_state.site ].indicator->deadcam );
}

static void Bomb_PlayerConnected( edict_t * ent ) {
	SetLoadout( ent, Info_ValueForKey( ent->r.client->userinfo, "cg_loadout" ), true );
}

static void Bomb_PlayerRespawning( edict_t * ent ) {
	if( PLAYERNUM( ent ) == bomb_state.carrier ) {
		DropBomb( BombDropReason_ChangingTeam );
	}
}

static void Bomb_PlayerRespawned( edict_t * ent, Team old_team, Team new_team ) {
	if( old_team != new_team ) {
		RemovePlayerFromRespawnQueues( &bomb_state.respawn_queues, PLAYERNUM( ent ) );

		if( new_team != Team_None ) {
			EnqueueRespawn( &bomb_state.respawn_queues, new_team, PLAYERNUM( ent ) );
		}
	}

	MatchState match_state = server_gs.gameState.match_state;
	RoundState round_state = server_gs.gameState.round_state;

	if( match_state <= MatchState_Warmup ) {
		ent->r.client->ps.can_change_loadout = new_team >= Team_One;
	}

	if( new_team != old_team && match_state == MatchState_Playing ) {
		if( old_team != Team_None ) {
			PlayXvXSound( old_team );
		}
		if( round_state == RoundState_Countdown && new_team != Team_None ) {
			G_ClientRespawn( ent, false ); // TODO: why does this not really respawn them?
			return;
		}
	}

	if( new_team == Team_None ) {
		return;
	}

	if( round_state == RoundState_Countdown ) {
		DisableMovementFor( PLAYERNUM( ent ) );
	}

	GiveInventory( ent );
}

static void Bomb_PlayerKilled( edict_t * victim, edict_t * attacker, edict_t * inflictor ) {
	if( server_gs.gameState.match_state != MatchState_Playing || server_gs.gameState.round_state >= RoundState_Finished ) {
		return;
	}

	if( PLAYERNUM( victim ) == bomb_state.carrier ) {
		DropBomb( BombDropReason_Killed );
	}

	if( attacker != NULL && attacker->r.client != NULL && attacker->s.team != victim->s.team ) {
		bomb_state.kills_this_round[ PLAYERNUM( attacker ) ]++;

		u32 required_for_ace = attacker->s.team == Team_One ? server_gs.gameState.bomb.beta_players_total : server_gs.gameState.bomb.alpha_players_total;
		if( required_for_ace >= 3 && bomb_state.kills_this_round[ PLAYERNUM( attacker ) ] == required_for_ace ) {
			G_AnnouncerSound( NULL, "sounds/announcer/bomb/ace", Team_Count, false, NULL );
		}
	}

	PlayXvXSound( victim->s.team );
}

static void Bomb_Think() {
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		UpdateScore( i );
	}

	if( server_gs.gameState.match_state != MatchState_Playing ) {
		return;
	}

	RoundThink();

	server_gs.gameState.bomb.alpha_players_alive = PlayersAliveOnTeam( Team_One );
	server_gs.gameState.bomb.beta_players_alive = PlayersAliveOnTeam( Team_Two );

	for( int i = 0; i < server_gs.maxclients; i++ ) {
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
}

static void Bomb_MatchStateStarted() {
	switch( server_gs.gameState.match_state ) {
		case MatchState_Warmup:
			break;

		case MatchState_Countdown:
			G_AnnouncerSound( NULL, "sounds/announcer/get_ready_to_fight", Team_Count, false, NULL );
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

static void Bomb_Init() {
	server_gs.gameState.gametype = Gametype_Bomb;
	server_gs.gameState.scorelimit = 10;
	server_gs.gameState.bomb.attacking_team = initial_attackers;

	bomb_state = { };
	bomb_state.carrier = -1;
	bomb_state.defuser = -1;

	InitRespawnQueues( &bomb_state.respawn_queues );

	G_AddCommand( ClientCommand_DropBomb, []( edict_t * ent, msg_t args ) {
		if( PLAYERNUM( ent ) == bomb_state.carrier && bomb_state.bomb.state == BombState_Carried ) {
			DropBomb( BombDropReason_Normal );
		}
	} );

	G_AddCommand( ClientCommand_LoadoutMenu, ShowShop );

	G_AddCommand( ClientCommand_SetLoadout, []( edict_t * ent, msg_t args ) {
		SetLoadout( ent, MSG_ReadString( &args ), false );
	} );

	g_bomb_roundtime = NewCvar( "g_bomb_roundtime", "60", CvarFlag_Archive );
	g_bomb_bombtimer = NewCvar( "g_bomb_bombtimer", "30", CvarFlag_Archive );
}

static bool Bomb_SpawnEntity( StringHash classname, edict_t * ent ) {
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

GametypeDef GetBombGametype() {
	GametypeDef gt = { };

	gt.Init = Bomb_Init;
	gt.MatchStateStarted = Bomb_MatchStateStarted;
	gt.Think = Bomb_Think;
	gt.PlayerConnected = Bomb_PlayerConnected;
	gt.PlayerRespawning = Bomb_PlayerRespawning;
	gt.PlayerRespawned = Bomb_PlayerRespawned;
	gt.PlayerKilled = Bomb_PlayerKilled;
	gt.SelectSpawnPoint = Bomb_SelectSpawnPoint;
	gt.SelectDeadcam = Bomb_SelectDeadcam;
	gt.MapHotloaded = ResetBombSites;
	gt.SpawnEntity = Bomb_SpawnEntity;

	gt.numTeams = 2;
	gt.removeInactivePlayers = true;
	gt.selfDamage = true;
	gt.autoRespawn = true;

	return gt;
}
