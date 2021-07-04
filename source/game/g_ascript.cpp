/*
Copyright (C) 2008 German Garcia

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

#include "game/g_local.h"
#include "game/g_as_local.h"
#include "qcommon/cmodel.h"
#include "qcommon/string.h"

#include "game/angelwrap/qas_public.h"

void asemptyfunc() {}

static const asEnumVal_t asSpawnSystemEnumVals[] =
{
	ASLIB_ENUM_VAL( SPAWNSYSTEM_INSTANT ),
	ASLIB_ENUM_VAL( SPAWNSYSTEM_WAVES ),
	ASLIB_ENUM_VAL( SPAWNSYSTEM_HOLD ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asMovetypeEnumVals[] =
{
	ASLIB_ENUM_VAL( MOVETYPE_NONE ),
	ASLIB_ENUM_VAL( MOVETYPE_PLAYER ),
	ASLIB_ENUM_VAL( MOVETYPE_NOCLIP ),
	ASLIB_ENUM_VAL( MOVETYPE_PUSH ),
	ASLIB_ENUM_VAL( MOVETYPE_STOP ),
	ASLIB_ENUM_VAL( MOVETYPE_FLY ),
	ASLIB_ENUM_VAL( MOVETYPE_TOSS ),
	ASLIB_ENUM_VAL( MOVETYPE_LINEARPROJECTILE ),
	ASLIB_ENUM_VAL( MOVETYPE_BOUNCE ),
	ASLIB_ENUM_VAL( MOVETYPE_BOUNCEGRENADE ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asDamageEnumVals[] =
{
	ASLIB_ENUM_VAL( DAMAGE_NO ),
	ASLIB_ENUM_VAL( DAMAGE_YES ),
	ASLIB_ENUM_VAL( DAMAGE_AIM ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asWorldDamageVals[] =
{
	ASLIB_ENUM_VAL( WorldDamage_Slime ),
	ASLIB_ENUM_VAL( WorldDamage_Lava ),
	ASLIB_ENUM_VAL( WorldDamage_Crush ),
	ASLIB_ENUM_VAL( WorldDamage_Telefrag ),
	ASLIB_ENUM_VAL( WorldDamage_Suicide ),
	ASLIB_ENUM_VAL( WorldDamage_Explosion ),

	ASLIB_ENUM_VAL( WorldDamage_Trigger ),

	ASLIB_ENUM_VAL( WorldDamage_Laser ),
	ASLIB_ENUM_VAL( WorldDamage_Spike ),
	ASLIB_ENUM_VAL( WorldDamage_Void ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asConfigstringEnumVals[] =
{
	ASLIB_ENUM_VAL( CS_HOSTNAME ),
	ASLIB_ENUM_VAL( CS_AUTORECORDSTATE ),
	ASLIB_ENUM_VAL( CS_MAXCLIENTS ),
	ASLIB_ENUM_VAL( CS_MATCHSCORE ),
	ASLIB_ENUM_VAL( CS_CALLVOTE ),
	ASLIB_ENUM_VAL( CS_CALLVOTE_REQUIRED_VOTES ),
	ASLIB_ENUM_VAL( CS_CALLVOTE_YES_VOTES ),
	ASLIB_ENUM_VAL( CS_CALLVOTE_NO_VOTES ),
	ASLIB_ENUM_VAL( CS_PLAYERINFOS ),
	ASLIB_ENUM_VAL( CS_GAMECOMMANDS ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asEffectEnumVals[] =
{
	ASLIB_ENUM_VAL( EF_CARRIER ),
	ASLIB_ENUM_VAL( EF_TAKEDAMAGE ),
	ASLIB_ENUM_VAL( EF_HAT ),
	ASLIB_ENUM_VAL( EF_TEAM_SILHOUETTE ),
	ASLIB_ENUM_VAL( EF_WORLD_MODEL ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asMatchStateEnumVals[] =
{
	ASLIB_ENUM_VAL( MatchState_Warmup ),
	ASLIB_ENUM_VAL( MatchState_Countdown ),
	ASLIB_ENUM_VAL( MatchState_Playing ),
	ASLIB_ENUM_VAL( MatchState_PostMatch ),
	ASLIB_ENUM_VAL( MatchState_WaitExit ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asTeamEnumVals[] =
{
	ASLIB_ENUM_VAL( TEAM_SPECTATOR ),
	ASLIB_ENUM_VAL( TEAM_PLAYERS ),
	ASLIB_ENUM_VAL( TEAM_ALPHA ),
	ASLIB_ENUM_VAL( TEAM_BETA ),
	ASLIB_ENUM_VAL( GS_MAX_TEAMS ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asEntityTypeEnumVals[] =
{
	ASLIB_ENUM_VAL( ET_GENERIC ),
	ASLIB_ENUM_VAL( ET_PLAYER ),
	ASLIB_ENUM_VAL( ET_CORPSE ),
	ASLIB_ENUM_VAL( ET_JUMPPAD ),
	ASLIB_ENUM_VAL( ET_PAINKILLER_JUMPPAD ),
	ASLIB_ENUM_VAL( ET_ROCKET ),
	ASLIB_ENUM_VAL( ET_GRENADE ),
	ASLIB_ENUM_VAL( ET_ARBULLET ),
	ASLIB_ENUM_VAL( ET_LASERBEAM ),
	ASLIB_ENUM_VAL( ET_BOMB ),
	ASLIB_ENUM_VAL( ET_BOMB_SITE ),
	ASLIB_ENUM_VAL( ET_LASER ),
	ASLIB_ENUM_VAL( ET_SPIKES ),

	ASLIB_ENUM_VAL( ET_EVENT ),
	ASLIB_ENUM_VAL( ET_SOUNDEVENT ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asSolidEnumVals[] =
{
	ASLIB_ENUM_VAL( SOLID_NOT ),
	ASLIB_ENUM_VAL( SOLID_TRIGGER ),
	ASLIB_ENUM_VAL( SOLID_YES ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asPMoveFeaturesVals[] =
{
	ASLIB_ENUM_VAL( PMFEAT_CROUCH ),
	ASLIB_ENUM_VAL( PMFEAT_WALK ),
	ASLIB_ENUM_VAL( PMFEAT_JUMP ),
	ASLIB_ENUM_VAL( PMFEAT_SPECIAL ),
	ASLIB_ENUM_VAL( PMFEAT_SCOPE ),
	ASLIB_ENUM_VAL( PMFEAT_GHOSTMOVE ),
	ASLIB_ENUM_VAL( PMFEAT_WEAPONSWITCH ),
	ASLIB_ENUM_VAL( PMFEAT_TEAMGHOST ),
	ASLIB_ENUM_VAL( PMFEAT_ALL ),
	ASLIB_ENUM_VAL( PMFEAT_DEFAULT ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asWeaponCategoryEnumVals[] =
{
	ASLIB_ENUM_VAL( WeaponCategory_Primary ),
	ASLIB_ENUM_VAL( WeaponCategory_Secondary ),
	ASLIB_ENUM_VAL( WeaponCategory_Backup ),

	ASLIB_ENUM_VAL( WeaponCategory_Count ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asWeaponTypeEnumVals[] =
{
	ASLIB_ENUM_VAL( Weapon_None ),

	ASLIB_ENUM_VAL( Weapon_Knife ),
	ASLIB_ENUM_VAL( Weapon_Pistol ),
	ASLIB_ENUM_VAL( Weapon_MachineGun ),
	ASLIB_ENUM_VAL( Weapon_Deagle ),
	ASLIB_ENUM_VAL( Weapon_Shotgun ),
	ASLIB_ENUM_VAL( Weapon_BurstRifle ),
	ASLIB_ENUM_VAL( Weapon_StakeGun ),
	ASLIB_ENUM_VAL( Weapon_GrenadeLauncher ),
	ASLIB_ENUM_VAL( Weapon_RocketLauncher ),
	ASLIB_ENUM_VAL( Weapon_AssaultRifle ),
	ASLIB_ENUM_VAL( Weapon_BubbleGun ),
	ASLIB_ENUM_VAL( Weapon_Laser ),
	ASLIB_ENUM_VAL( Weapon_Railgun ),
	ASLIB_ENUM_VAL( Weapon_Sniper ),
	ASLIB_ENUM_VAL( Weapon_Rifle ),
	ASLIB_ENUM_VAL( Weapon_MasterBlaster ),
	ASLIB_ENUM_VAL( Weapon_RoadGun ),
	// ASLIB_ENUM_VAL( Weapon_Minigun ),

	ASLIB_ENUM_VAL( Weapon_Count ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asGadgetTypeEnumVals[] =
{
	ASLIB_ENUM_VAL( Gadget_None ),

	ASLIB_ENUM_VAL( Gadget_ThrowingAxe ),

	ASLIB_ENUM_VAL( Gadget_Count ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asClientStateEnumVals[] =
{
	ASLIB_ENUM_VAL( CS_FREE ),
	ASLIB_ENUM_VAL( CS_ZOMBIE ),
	ASLIB_ENUM_VAL( CS_CONNECTING ),
	ASLIB_ENUM_VAL( CS_CONNECTED ),
	ASLIB_ENUM_VAL( CS_SPAWNED ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asSoundChannelEnumVals[] =
{
	ASLIB_ENUM_VAL( CHAN_AUTO ),
	ASLIB_ENUM_VAL( CHAN_BODY ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asContentsEnumVals[] =
{
	ASLIB_ENUM_VAL( CONTENTS_SOLID ),
	ASLIB_ENUM_VAL( CONTENTS_LAVA ),
	ASLIB_ENUM_VAL( CONTENTS_SLIME ),
	ASLIB_ENUM_VAL( CONTENTS_WATER ),
	ASLIB_ENUM_VAL( CONTENTS_WALLBANGABLE ),
	ASLIB_ENUM_VAL( CONTENTS_AREAPORTAL ),
	ASLIB_ENUM_VAL( CONTENTS_PLAYERCLIP ),
	ASLIB_ENUM_VAL( CONTENTS_WEAPONCLIP ),
	ASLIB_ENUM_VAL( CONTENTS_TELEPORTER ),
	ASLIB_ENUM_VAL( CONTENTS_JUMPPAD ),
	ASLIB_ENUM_VAL( CONTENTS_CLUSTERPORTAL ),
	ASLIB_ENUM_VAL( CONTENTS_DONOTENTER ),
	ASLIB_ENUM_VAL( CONTENTS_TEAMALPHA ),
	ASLIB_ENUM_VAL( CONTENTS_TEAMBETA ),
	ASLIB_ENUM_VAL( CONTENTS_ORIGIN ),
	ASLIB_ENUM_VAL( CONTENTS_BODY ),
	ASLIB_ENUM_VAL( CONTENTS_CORPSE ),
	ASLIB_ENUM_VAL( CONTENTS_DETAIL ),
	ASLIB_ENUM_VAL( CONTENTS_STRUCTURAL ),
	ASLIB_ENUM_VAL( CONTENTS_TRANSLUCENT ),
	ASLIB_ENUM_VAL( CONTENTS_TRIGGER ),
	ASLIB_ENUM_VAL( CONTENTS_NODROP ),
	ASLIB_ENUM_VAL( MASK_ALL ),
	ASLIB_ENUM_VAL( MASK_SOLID ),
	ASLIB_ENUM_VAL( MASK_PLAYERSOLID ),
	ASLIB_ENUM_VAL( MASK_DEADSOLID ),
	ASLIB_ENUM_VAL( MASK_WATER ),
	ASLIB_ENUM_VAL( MASK_OPAQUE ),
	ASLIB_ENUM_VAL( MASK_SHOT ),
	ASLIB_ENUM_VAL( MASK_WALLBANG ),
	ASLIB_ENUM_VAL( MASK_ALPHAPLAYERSOLID ),
	ASLIB_ENUM_VAL( MASK_BETAPLAYERSOLID ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asSurfFlagEnumVals[] =
{
	ASLIB_ENUM_VAL( SURF_NODAMAGE ),
	ASLIB_ENUM_VAL( SURF_SLICK ),
	ASLIB_ENUM_VAL( SURF_SKY ),
	ASLIB_ENUM_VAL( SURF_LADDER ),
	ASLIB_ENUM_VAL( SURF_NOIMPACT ),
	ASLIB_ENUM_VAL( SURF_NOMARKS ),
	ASLIB_ENUM_VAL( SURF_FLESH ),
	ASLIB_ENUM_VAL( SURF_NODRAW ),
	ASLIB_ENUM_VAL( SURF_HINT ),
	ASLIB_ENUM_VAL( SURF_SKIP ),
	ASLIB_ENUM_VAL( SURF_NOLIGHTMAP ),
	ASLIB_ENUM_VAL( SURF_POINTLIGHT ),
	ASLIB_ENUM_VAL( SURF_METALSTEPS ),
	ASLIB_ENUM_VAL( SURF_NOSTEPS ),
	ASLIB_ENUM_VAL( SURF_NONSOLID ),
	ASLIB_ENUM_VAL( SURF_LIGHTFILTER ),
	ASLIB_ENUM_VAL( SURF_ALPHASHADOW ),
	ASLIB_ENUM_VAL( SURF_NODLIGHT ),
	ASLIB_ENUM_VAL( SURF_DUST ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asSVFlagEnumVals[] =
{
	ASLIB_ENUM_VAL( SVF_NOCLIENT ),
	ASLIB_ENUM_VAL( SVF_TRANSMITORIGIN2 ),
	ASLIB_ENUM_VAL( SVF_SOUNDCULL ),
	ASLIB_ENUM_VAL( SVF_FAKECLIENT ),
	ASLIB_ENUM_VAL( SVF_BROADCAST ),
	ASLIB_ENUM_VAL( SVF_CORPSE ),
	ASLIB_ENUM_VAL( SVF_PROJECTILE ),
	ASLIB_ENUM_VAL( SVF_ONLYTEAM ),
	ASLIB_ENUM_VAL( SVF_FORCEOWNER ),
	ASLIB_ENUM_VAL( SVF_ONLYOWNER ),
	ASLIB_ENUM_VAL( SVF_FORCETEAM ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asAxisEnumVals[] =
{
	ASLIB_ENUM_VAL( PITCH ),
	ASLIB_ENUM_VAL( YAW ),
	ASLIB_ENUM_VAL( ROLL ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asBombProgressEnumVals[] =
{
	ASLIB_ENUM_VAL( BombProgress_Nothing ),
	ASLIB_ENUM_VAL( BombProgress_Planting ),
	ASLIB_ENUM_VAL( BombProgress_Defusing ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asBombDownEnumVals[] =
{
	ASLIB_ENUM_VAL( BombDown_Dropped ),
	ASLIB_ENUM_VAL( BombDown_Planting ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asRoundStateEnumVals[] =
{
	ASLIB_ENUM_VAL( RoundState_None ),
	ASLIB_ENUM_VAL( RoundState_Countdown ),
	ASLIB_ENUM_VAL( RoundState_Round ),
	ASLIB_ENUM_VAL( RoundState_Finished ),
	ASLIB_ENUM_VAL( RoundState_Post ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asRoundTypeEnumVals[] =
{
	ASLIB_ENUM_VAL( RoundType_Normal ),
	ASLIB_ENUM_VAL( RoundType_MatchPoint ),
	ASLIB_ENUM_VAL( RoundType_Overtime ),
	ASLIB_ENUM_VAL( RoundType_OvertimeMatchPoint ),

	ASLIB_ENUM_VAL_NULL
};

static const asEnum_t asGameEnums[] =
{
	{ "spawnsystem_e", asSpawnSystemEnumVals },
	{ "movetype_e", asMovetypeEnumVals },

	{ "takedamage_e", asDamageEnumVals },
	{ "WorldDamage", asWorldDamageVals },

	{ "configstrings_e", asConfigstringEnumVals },
	{ "state_effects_e", asEffectEnumVals },
	{ "MatchState", asMatchStateEnumVals },
	{ "teams_e", asTeamEnumVals },
	{ "entitytype_e", asEntityTypeEnumVals },
	{ "solid_e", asSolidEnumVals },
	{ "pmovefeats_e", asPMoveFeaturesVals },

	{ "WeaponCategory", asWeaponCategoryEnumVals },
	{ "WeaponType", asWeaponTypeEnumVals },
	{ "GadgetType", asGadgetTypeEnumVals },

	{ "client_statest_e", asClientStateEnumVals },
	{ "sound_channels_e", asSoundChannelEnumVals },
	{ "contents_e", asContentsEnumVals },
	{ "surfaceflags_e", asSurfFlagEnumVals },
	{ "serverflags_e", asSVFlagEnumVals },

	{ "axis_e", asAxisEnumVals },

	{ "BombProgress", asBombProgressEnumVals },
	{ "BombDown", asBombDownEnumVals },
	{ "RoundState", asRoundStateEnumVals },
	{ "RoundType", asRoundTypeEnumVals },

	ASLIB_ENUM_VAL_NULL
};

static asIObjectType *asEntityArrayType() {
	asIScriptContext *ctx = game.asExport->asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	asIObjectType *ot = engine->GetObjectTypeById( engine->GetTypeIdByDecl( "array<Entity @>" ) );
	return ot;
}

// CLASS: Match

static void objectMatch_launchState( MatchState state, SyncGameState *self ) {
	G_Match_LaunchState( state );
}

static void objectMatch_startAutorecord( SyncGameState *self ) {
	G_Match_Autorecord_Start();
}

static void objectMatch_stopAutorecord( SyncGameState *self ) {
	G_Match_Autorecord_Stop();
}

static bool objectMatch_scoreLimitHit( SyncGameState *self ) {
	return G_Match_ScorelimitHit();
}

static bool objectMatch_timeLimitHit( SyncGameState *self ) {
	return G_Match_TimelimitHit();
}

static bool objectMatch_isPaused( SyncGameState *self ) {
	return GS_MatchPaused( &server_gs );
}

static asstring_t *objectMatch_getScore( SyncGameState *self ) {
	const char *s = PF_GetConfigString( CS_MATCHSCORE );

	return game.asExport->asStringFactoryBuffer( s, strlen( s ) );
}

static void objectMatch_setScore( asstring_t *name, SyncGameState *self ) {
	PF_ConfigString( CS_MATCHSCORE, name->buffer );
}

static void objectMatch_setClockOverride( int64_t time, SyncGameState *self ) {
	self->clock_override = time;
}

static const asFuncdef_t match_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const asBehavior_t match_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const asMethod_t match_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( void, launchState, (MatchState state) const ), asFUNCTION( objectMatch_launchState ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, startAutorecord, ( ) const ), asFUNCTION( objectMatch_startAutorecord ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, stopAutorecord, ( ) const ), asFUNCTION( objectMatch_stopAutorecord ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, scoreLimitHit, ( ) const ), asFUNCTION( objectMatch_scoreLimitHit ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, timeLimitHit, ( ) const ), asFUNCTION( objectMatch_timeLimitHit ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, isPaused, ( ) const ), asFUNCTION( objectMatch_isPaused ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, getScore, ( ) const ), asFUNCTION( objectMatch_getScore ),  asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, setScore, ( String & in ) ), asFUNCTION( objectMatch_setScore ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, setClockOverride, ( int64 milliseconds ) ), asFUNCTION( objectMatch_setClockOverride ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const asProperty_t match_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( uint8, roundNum ), offsetof( SyncGameState, round_num ) },
	{ ASLIB_PROPERTY_DECL( MatchState, matchState ), offsetof( SyncGameState, match_state ) },
	{ ASLIB_PROPERTY_DECL( RoundState, roundState ), offsetof( SyncGameState, round_state ) },
	{ ASLIB_PROPERTY_DECL( RoundType, roundType ), offsetof( SyncGameState, round_type ) },
	{ ASLIB_PROPERTY_DECL( uint8, alphaPlayersTotal ), offsetof( SyncGameState, bomb.alpha_players_total ) },
	{ ASLIB_PROPERTY_DECL( uint8, alphaPlayersAlive ), offsetof( SyncGameState, bomb.alpha_players_alive ) },
	{ ASLIB_PROPERTY_DECL( uint8, betaPlayersTotal ), offsetof( SyncGameState, bomb.beta_players_total ) },
	{ ASLIB_PROPERTY_DECL( uint8, betaPlayersAlive ), offsetof( SyncGameState, bomb.beta_players_alive ) },
	{ ASLIB_PROPERTY_DECL( bool, exploding ), offsetof( SyncGameState, bomb.exploding ) },
	{ ASLIB_PROPERTY_DECL( int64, explodedAt ), offsetof( SyncGameState, bomb.exploded_at ) },
	ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asMatchClassDescriptor =
{
	"Match",                    /* name */
	static_cast<asEObjTypeFlags>( asOBJ_REF | asOBJ_NOHANDLE ), /* object type flags */
	sizeof( SyncGameState ),          /* size */
	match_Funcdefs,             /* funcdefs */
	match_ObjectBehaviors,      /* object behaviors */
	match_Methods,              /* methods */
	match_Properties,           /* properties */

	NULL, NULL                  /* string factory hack */
};

// CLASS: GametypeDesc

static void objectGametypeDescriptor_SetTeamSpawnsystem( int team, int spawnsystem, int wave_time, int wave_maxcount, bool spectate_team, gametype_descriptor_t *self ) {
	G_SpawnQueue_SetTeamSpawnsystem( team, spawnsystem, wave_time, wave_maxcount, spectate_team );
}

static const asFuncdef_t gametypedescr_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const asBehavior_t gametypedescr_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const asMethod_t gametypedescr_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( void, setTeamSpawnsystem, ( int team, int spawnsystem, int wave_time, int wave_maxcount, bool deadcam ) ), asFUNCTION( objectGametypeDescriptor_SetTeamSpawnsystem ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const asProperty_t gametypedescr_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( bool, isTeamBased ), offsetof( gametype_descriptor_t, isTeamBased ) },
	{ ASLIB_PROPERTY_DECL( bool, hasChallengersQueue ), offsetof( gametype_descriptor_t, hasChallengersQueue ) },
	{ ASLIB_PROPERTY_DECL( bool, hasChallengersRoulette ), offsetof( gametype_descriptor_t, hasChallengersRoulette ) },
	{ ASLIB_PROPERTY_DECL( bool, countdownEnabled ), offsetof( gametype_descriptor_t, countdownEnabled ) },
	{ ASLIB_PROPERTY_DECL( bool, matchAbortDisabled ), offsetof( gametype_descriptor_t, matchAbortDisabled ) },
	{ ASLIB_PROPERTY_DECL( bool, shootingDisabled ), offsetof( gametype_descriptor_t, shootingDisabled ) },
	{ ASLIB_PROPERTY_DECL( bool, removeInactivePlayers ), offsetof( gametype_descriptor_t, removeInactivePlayers ) },
	{ ASLIB_PROPERTY_DECL( bool, selfDamage ), offsetof( gametype_descriptor_t, selfDamage ) },

	ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asGametypeClassDescriptor =
{
	"GametypeDesc",                 /* name */
	asOBJ_REF | asOBJ_NOHANDLE,       /* object type flags */
	sizeof( gametype_descriptor_t ),/* size */
	gametypedescr_Funcdefs,         /* funcdefs */
	gametypedescr_ObjectBehaviors,  /* object behaviors */
	gametypedescr_Methods,          /* methods */
	gametypedescr_Properties,       /* properties */

	NULL, NULL                      /* string factory hack */
};

// CLASS: Team
static edict_t *objectTeamlist_GetPlayerEntity( int index, SyncTeamState * obj ) {
	if( index < 0 || index >= obj->num_players ) {
		return NULL;
	}

	if( obj->player_indices[ index ] < 1 || obj->player_indices[ index ] > server_gs.maxclients ) {
		return NULL;
	}

	return &game.edicts[ obj->player_indices[ index ] ];
}

static asstring_t *objectTeamlist_getName( SyncTeamState * obj ) {
	const char *name = GS_TeamName( obj - server_gs.gameState.teams );

	return game.asExport->asStringFactoryBuffer( name, name ? strlen( name ) : 0 );
}

static void objectTeamlist_SetScore( int score, SyncTeamState * obj ) {
	obj->score = score;
}

static void objectTeamlist_AddScore( int add, SyncTeamState * obj ) {
	obj->score += add;
}

static const asFuncdef_t teamlist_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const asBehavior_t teamlist_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const asMethod_t teamlist_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( Entity @, ent, ( int index ) ), asFUNCTION( objectTeamlist_GetPlayerEntity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_name, ( ) const ), asFUNCTION( objectTeamlist_getName ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, setScore, ( int ) const ), asFUNCTION( objectTeamlist_SetScore ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, addScore, ( int ) const ), asFUNCTION( objectTeamlist_AddScore ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const asProperty_t teamlist_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( const uint8, score ), offsetof( SyncTeamState, score ) },
	{ ASLIB_PROPERTY_DECL( const uint8, numPlayers ), offsetof( SyncTeamState, num_players ) },

	ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asTeamListClassDescriptor =
{
	"Team",                     /* name */
	asOBJ_REF | asOBJ_NOCOUNT,  /* object type flags */
	sizeof( SyncTeamState ),    /* size */
	teamlist_Funcdefs,          /* funcdefs */
	teamlist_ObjectBehaviors,   /* object behaviors */
	teamlist_Methods,           /* methods */
	teamlist_Properties,        /* properties */

	NULL, NULL                  /* string factory hack */
};

// CLASS: Stats
static void objectScoreStats_AddScore( int add, score_stats_t *obj ) {
	obj->score += add;
}

static void objectScoreStats_SetScore( int score, score_stats_t *obj ) {
	obj->score = score;
}

static void objectScoreStats_Clear( score_stats_t *obj ) {
	memset( obj, 0, sizeof( *obj ) );
}


static const asFuncdef_t scorestats_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const asBehavior_t scorestats_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const asMethod_t scorestats_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( void, addScore, ( int ) ), asFUNCTION( objectScoreStats_AddScore ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, setScore, ( int ) ), asFUNCTION( objectScoreStats_SetScore ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, clear, ( ) ), asFUNCTION( objectScoreStats_Clear ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const asProperty_t scorestats_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( const int, kills ), offsetof( score_stats_t, kills ) },
	{ ASLIB_PROPERTY_DECL( const int, deaths ), offsetof( score_stats_t, deaths ) },
	{ ASLIB_PROPERTY_DECL( const int, suicides ), offsetof( score_stats_t, suicides ) },
	{ ASLIB_PROPERTY_DECL( const int, score ), offsetof( score_stats_t, score ) },
	{ ASLIB_PROPERTY_DECL( const int, totalDamageGiven ), offsetof( score_stats_t, total_damage_given ) },
	{ ASLIB_PROPERTY_DECL( const int, totalDamageReceived ), offsetof( score_stats_t, total_damage_received ) },

	ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asScoreStatsClassDescriptor =
{
	"Stats",                    /* name */
	asOBJ_REF | asOBJ_NOCOUNT,    /* object type flags */
	sizeof( score_stats_t ),    /* size */
	scorestats_Funcdefs,        /* funcdefs */
	scorestats_ObjectBehaviors, /* object behaviors */
	scorestats_Methods,         /* methods */
	scorestats_Properties,      /* properties */

	NULL, NULL                  /* string factory hack */
};

// CLASS: Client
static int objectGameClient_PlayerNum( gclient_t *self ) {
	return PLAYERNUM( self );
}

static bool objectGameClient_isBot( gclient_t *self ) {
	int playerNum = objectGameClient_PlayerNum( self );
	if( playerNum < 0 && playerNum >= server_gs.maxclients ) {
		return false;
	}

	const edict_t * ent = PLAYERENT( playerNum );
	return ( ent->r.svflags & SVF_FAKECLIENT ) != 0;
}

static int objectGameClient_ClientState( gclient_t *self ) {
	return PF_GetClientState( (int)( self - game.clients ) );
}

static void objectGameClient_ClearPlayerStateEvents( gclient_t *self ) {
	G_ClearPlayerStateEvents( self );
}

static asstring_t *objectGameClient_getName( gclient_t *self ) {
	return game.asExport->asStringFactoryBuffer( self->netname, strlen( self->netname ) );
}

static void objectGameClient_Respawn( bool ghost, gclient_t *self ) {
	int playerNum = objectGameClient_PlayerNum( self );

	if( playerNum >= 0 && playerNum < server_gs.maxclients ) {
		G_ClientRespawn( &game.edicts[playerNum + 1], ghost );
	}
}

static edict_t *objectGameClient_GetEntity( gclient_t *self ) {
	int playerNum = objectGameClient_PlayerNum( self );
	if( playerNum < 0 || playerNum >= server_gs.maxclients ) {
		return NULL;
	}

	return PLAYERENT( playerNum );
}

static void objectGameClient_GiveWeapon( WeaponType weapon, gclient_t * self ) {
	assert( weapon > Weapon_None && weapon < Weapon_Count );

	int playerNum = objectGameClient_PlayerNum( self );
	if( playerNum < 0 || playerNum >= server_gs.maxclients ) {
		return;
	}

	SyncPlayerState * ps = &PLAYERENT( playerNum )->r.client->ps;

	for( size_t i = 0; i < ARRAY_COUNT( ps->weapons ); i++ ) {
		if( ps->weapons[ i ].weapon == weapon || ps->weapons[ i ].weapon == Weapon_None ) {
			ps->weapons[ i ].weapon = weapon;
			ps->weapons[ i ].ammo = GS_GetWeaponDef( weapon )->clip_size;
			break;
		}
	}
}

static void objectGameClient_InventoryClear( gclient_t *self ) {
	ClearInventory( &self->ps );
}

static void objectGameClient_SelectWeapon( int index, gclient_t *self ) {
	if( self->ps.weapons[ index ].weapon == Weapon_None )
		return;

	self->ps.weapon = Weapon_None;
	self->ps.pending_weapon = self->ps.weapons[ index ].weapon;
	self->ps.weapon_state = WeaponState_DispatchQuiet;
	self->ps.weapon_state_time = 0;
}

static void objectGameClient_addAward( asstring_t *msg, gclient_t *self ) {
	if( !msg ) {
		return;
	}

	int playerNum = objectGameClient_PlayerNum( self );
	if( playerNum < 0 || playerNum >= server_gs.maxclients ) {
		return;
	}

	G_PlayerAward( PLAYERENT( playerNum ), msg->buffer );
}

static void objectGameClient_execGameCommand( asstring_t *msg, gclient_t *self ) {
	int playerNum;

	if( !msg ) {
		return;
	}

	playerNum = objectGameClient_PlayerNum( self );
	if( playerNum < 0 || playerNum >= server_gs.maxclients ) {
		return;
	}

	PF_GameCmd( PLAYERENT( playerNum ), msg->buffer );
}

static asstring_t *objectGameClient_getUserInfoKey( asstring_t *key, gclient_t *self ) {
	char *s;

	if( !key || !key->buffer || !key->buffer[0] ) {
		return game.asExport->asStringFactoryBuffer( NULL, 0 );
	}

	s = Info_ValueForKey( self->userinfo, key->buffer );
	if( !s || !*s ) {
		return game.asExport->asStringFactoryBuffer( NULL, 0 );
	}

	return game.asExport->asStringFactoryBuffer( s, strlen( s ) );
}

static void objectGameClient_printMessage( asstring_t *str, gclient_t *self ) {
	int playerNum;

	if( !str || !str->buffer ) {
		return;
	}

	playerNum = objectGameClient_PlayerNum( self );
	if( playerNum < 0 || playerNum >= server_gs.maxclients ) {
		return;
	}

	G_PrintMsg( PLAYERENT( playerNum ), "%s", str->buffer );
}

static void objectGameClient_ChaseCam( asstring_t *str, bool teamonly, gclient_t *self ) {
	int playerNum;

	playerNum = objectGameClient_PlayerNum( self );
	if( playerNum < 0 || playerNum >= server_gs.maxclients ) {
		return;
	}

	G_ChasePlayer( PLAYERENT( playerNum ), str ? str->buffer : NULL, teamonly, 0 );
}

static void objectGameClient_SetChaseActive( bool active, gclient_t *self ) {
	int playerNum;

	playerNum = objectGameClient_PlayerNum( self );
	if( playerNum < 0 || playerNum >= server_gs.maxclients ) {
		return;
	}

	self->resp.chase.active = active;
}

static bool objectGameClient_GetChaseActive( gclient_t *self ) {
	return self->resp.chase.active;
}

static const asFuncdef_t gameclient_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const asBehavior_t gameclient_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const asMethod_t gameclient_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( int, get_playerNum, ( ) const ), asFUNCTION( objectGameClient_PlayerNum ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, isBot, ( ) const ), asFUNCTION( objectGameClient_isBot ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, state, ( ) const ), asFUNCTION( objectGameClient_ClientState ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, respawn, ( bool ghost ) ), asFUNCTION( objectGameClient_Respawn ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, clearPlayerStateEvents, ( ) ), asFUNCTION( objectGameClient_ClearPlayerStateEvents ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, get_name, ( ) const ), asFUNCTION( objectGameClient_getName ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Entity @, getEnt, ( ) const ), asFUNCTION( objectGameClient_GetEntity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, giveWeapon, ( WeaponType weapon ) ), asFUNCTION( objectGameClient_GiveWeapon ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, inventoryClear, ( ) ), asFUNCTION( objectGameClient_InventoryClear ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, selectWeapon, ( int tag ) ), asFUNCTION( objectGameClient_SelectWeapon ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, addAward, ( const String &in ) ), asFUNCTION( objectGameClient_addAward ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, execGameCommand, ( const String &in ) ), asFUNCTION( objectGameClient_execGameCommand ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( const String @, getUserInfoKey, ( const String &in ) const ), asFUNCTION( objectGameClient_getUserInfoKey ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, printMessage, ( const String &in ) ), asFUNCTION( objectGameClient_printMessage ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, chaseCam, ( const String @, bool teamOnly ) ), asFUNCTION( objectGameClient_ChaseCam ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_chaseActive, ( const bool active ) ), asFUNCTION( objectGameClient_SetChaseActive ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, get_chaseActive, ( ) const ), asFUNCTION( objectGameClient_GetChaseActive ), asCALL_CDECL_OBJLAST },
	ASLIB_METHOD_NULL
};

static const asProperty_t gameclient_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( Stats, stats ), offsetof( gclient_t, level.stats ) },
	{ ASLIB_PROPERTY_DECL( const bool, connecting ), offsetof( gclient_t, connecting ) },
	{ ASLIB_PROPERTY_DECL( int, team ), offsetof( gclient_t, team ) },
	{ ASLIB_PROPERTY_DECL( const bool, isOperator ), offsetof( gclient_t, isoperator ) },
	{ ASLIB_PROPERTY_DECL( const int64, queueTimeStamp ), offsetof( gclient_t, queueTimeStamp ) },
	{ ASLIB_PROPERTY_DECL( const int, muted ), offsetof( gclient_t, muted ) },
	{ ASLIB_PROPERTY_DECL( const bool, chaseActive ), offsetof( gclient_t, resp.chase.active ) },
	{ ASLIB_PROPERTY_DECL( int, chaseTarget ), offsetof( gclient_t, resp.chase.target ) },
	{ ASLIB_PROPERTY_DECL( bool, chaseTeamonly ), offsetof( gclient_t, resp.chase.teamonly ) },
	{ ASLIB_PROPERTY_DECL( int, chaseFollowMode ), offsetof( gclient_t, resp.chase.followmode ) },
	{ ASLIB_PROPERTY_DECL( const int, ping ), offsetof( gclient_t, r.ping ) },
	{ ASLIB_PROPERTY_DECL( uint8, weapon ), offsetof( gclient_t, ps.weapon ) },
	{ ASLIB_PROPERTY_DECL( uint8, pendingWeapon ), offsetof( gclient_t, ps.pending_weapon ) },
	{ ASLIB_PROPERTY_DECL( bool, canChangeLoadout ), offsetof( gclient_t, ps.can_change_loadout ) },
	{ ASLIB_PROPERTY_DECL( bool, carryingBomb ), offsetof( gclient_t, ps.carrying_bomb ) },
	{ ASLIB_PROPERTY_DECL( bool, canPlant ), offsetof( gclient_t, ps.can_plant ) },
	{ ASLIB_PROPERTY_DECL( int64, lastActivity ), offsetof( gclient_t, level.last_activity ) },
	{ ASLIB_PROPERTY_DECL( uint8, progressType ), offsetof( gclient_t, ps.progress_type ) },
	{ ASLIB_PROPERTY_DECL( uint8, progress ), offsetof( gclient_t, ps.progress ) },
	{ ASLIB_PROPERTY_DECL( const int64, uCmdTimeStamp ), offsetof( gclient_t, ucmd.serverTimeStamp ) },
	{ ASLIB_PROPERTY_DECL( uint16, pmoveFeatures ), offsetof( gclient_t, ps.pmove.features ) },
	{ ASLIB_PROPERTY_DECL( int16, pmoveMaxSpeed ), offsetof( gclient_t, ps.pmove.max_speed ) },
	{ ASLIB_PROPERTY_DECL( int16, pmoveJumpSpeed ), offsetof( gclient_t, ps.pmove.jump_speed ) },
	{ ASLIB_PROPERTY_DECL( int16, pmoveDashSpeed ), offsetof( gclient_t, ps.pmove.dash_speed ) },

	ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asGameClientDescriptor =
{
	"Client",                   /* name */
	asOBJ_REF | asOBJ_NOCOUNT,    /* object type flags */
	sizeof( gclient_t ),        /* size */
	gameclient_Funcdefs,        /* funcdefs */
	gameclient_ObjectBehaviors, /* object behaviors */
	gameclient_Methods,         /* methods */
	gameclient_Properties,      /* properties */

	NULL, NULL                  /* string factory hack */
};

// CLASS: Entity
static asvec3_t objectGameEntity_GetVelocity( edict_t *obj ) {
	asvec3_t velocity;

	velocity.v = obj->velocity;

	return velocity;
}

static void objectGameEntity_SetVelocity( asvec3_t *vel, edict_t *self ) {
	self->velocity = vel->v;

	if( self->r.client && PF_GetClientState( PLAYERNUM( self ) ) >= CS_SPAWNED ) {
		self->r.client->ps.pmove.velocity = vel->v;
	}
}

static asvec3_t objectGameEntity_GetAVelocity( edict_t *obj ) {
	asvec3_t avelocity;

	avelocity.v = obj->avelocity;

	return avelocity;
}

static void objectGameEntity_SetAVelocity( asvec3_t *vel, edict_t *self ) {
	self->avelocity = vel->v;
}

static asvec3_t objectGameEntity_GetOrigin( edict_t *obj ) {
	asvec3_t origin;

	origin.v = obj->s.origin;
	return origin;
}

static void objectGameEntity_SetOrigin( asvec3_t *vec, edict_t *self ) {
	if( self->r.client && PF_GetClientState( PLAYERNUM( self ) ) >= CS_SPAWNED ) {
		self->r.client->ps.pmove.origin = vec->v;
	}
	self->s.origin = vec->v;
}

static asvec3_t objectGameEntity_GetOrigin2( edict_t *obj ) {
	asvec3_t origin;

	origin.v = obj->s.origin2;
	return origin;
}

static void objectGameEntity_SetOrigin2( asvec3_t *vec, edict_t *self ) {
	self->s.origin2 = vec->v;
}

static asvec3_t objectGameEntity_GetAngles( edict_t *obj ) {
	asvec3_t angles;

	angles.v = obj->s.angles;
	return angles;
}

static void objectGameEntity_SetAngles( asvec3_t *vec, edict_t *self ) {
	self->s.angles = vec->v;

	if( self->r.client && PF_GetClientState( PLAYERNUM( self ) ) >= CS_SPAWNED ) {
		self->r.client->ps.viewangles = vec->v;

		// update the delta angle
		for( int i = 0; i < 3; i++ ) {
			self->r.client->ps.pmove.delta_angles[ i ] = ANGLE2SHORT( self->r.client->ps.viewangles[ i ] ) - self->r.client->ucmd.angles[ i ];
		}
	}
}

static void objectGameEntity_GetSize( asvec3_t *mins, asvec3_t *maxs, edict_t *self ) {
	maxs->v = self->r.maxs;
	mins->v = self->r.mins;
}

static void objectGameEntity_SetSize( asvec3_t *mins, asvec3_t *maxs, edict_t *self ) {
	self->r.mins = mins->v;
	self->r.maxs = maxs->v;
}

static asvec3_t objectGameEntity_GetMovedir( edict_t *self ) {
	asvec3_t movedir;

	movedir.v = self->moveinfo.movedir;
	return movedir;
}

static void objectGameEntity_SetMovedir( edict_t *self ) {
	G_SetMovedir( &self->s.angles, &self->moveinfo.movedir );
}

static bool objectGameEntity_IsGhosting( edict_t *self ) {
	if( self->r.client && PF_GetClientState( PLAYERNUM( self ) ) < CS_SPAWNED ) {
		return true;
	}

	return G_ISGHOSTING( self ) ? true : false;
}

static int objectGameEntity_EntNum( edict_t *self ) {
	return ( ENTNUM( self ) );
}

static int objectGameEntity_PlayerNum( edict_t *self ) {
	return ( PLAYERNUM( self ) );
}

static void objectGameEntity_GhostClient( edict_t *self ) {
	if( self->r.client ) {
		G_GhostClient( self );
	}
}

static void objectGameEntity_SetupModel( edict_t *self ) {
	GClip_SetBrushModel( self );
}

static void objectGameEntity_UseTargets( edict_t *activator, edict_t *self ) {
	G_UseTargets( self, activator );
}

static CScriptArrayInterface *objectGameEntity_findTargets( edict_t *self ) {
	asIObjectType *ot = asEntityArrayType();
	CScriptArrayInterface *arr = game.asExport->asCreateArrayCpp( 0, ot );

	if( self->target != EMPTY_HASH ) {
		int count = 0;
		edict_t *ent = NULL;
		while( ( ent = G_Find( ent, &edict_t::name, self->target ) ) != NULL ) {
			arr->Resize( count + 1 );
			*( (edict_t **)arr->At( count ) ) = ent;
			count++;
		}
	}

	return arr;
}

static void objectGameEntity_TeleportEffect( bool in, edict_t *self ) {
	G_TeleportEffect( self, in );
}

static void objectGameEntity_sustainDamage( edict_t *inflictor, edict_t *attacker, asvec3_t *dir, float damage, float knockback, WorldDamage mod, edict_t *self ) {
	G_Damage( self, inflictor, attacker,
			  dir ? dir->v : Vec3( 0.0f ), dir ? dir->v : Vec3( 0.0f ),
			  inflictor ? inflictor->s.origin : self->s.origin,
			  damage, knockback, 0, mod );
}

static void objectGameEntity_splashDamage( edict_t *attacker, int radius, float damage, float knockback, edict_t *self ) {
	if( radius < 1 ) {
		return;
	}

	self->projectileInfo.maxDamage = damage;
	self->projectileInfo.minDamage = 1;
	self->projectileInfo.maxKnockback = knockback;
	self->projectileInfo.minKnockback = 1;
	self->projectileInfo.radius = radius;

	G_RadiusDamage( self, attacker, NULL, self, WorldDamage_Explosion );
}

static void objectGameEntity_explosionEffect( int radius, edict_t *self ) {
	int eventType, eventRadius;

	if( radius < 8 ) {
		return;
	}

	// turn entity into event
	if( radius > 255 * 8 ) {
		eventType = EV_EXPLOSION2;
		eventRadius = (int)( radius * 1 / 16 ) & 0xFF;
	} else {
		eventType = EV_EXPLOSION1;
		eventRadius = (int)( radius * 1 / 8 ) & 0xFF;
	}

	if( eventRadius < 1 ) {
		eventRadius = 1;
	}

	Vec3 center = self->s.origin + 0.5f * ( self->r.maxs + self->r.mins );

	G_SpawnEvent( eventType, eventRadius, &center );
}

static const asFuncdef_t gedict_Funcdefs[] =
{
	{ ASLIB_FUNCTION_DECL( void, entThink, ( Entity @ent ) ) },
	{ ASLIB_FUNCTION_DECL( void, entTouch, ( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags ) ) },
	{ ASLIB_FUNCTION_DECL( void, entUse, ( Entity @ent, Entity @other, Entity @activator ) ) },
	{ ASLIB_FUNCTION_DECL( void, entPain, ( Entity @ent, Entity @other, float kick, float damage ) ) },
	{ ASLIB_FUNCTION_DECL( void, entDie, ( Entity @ent, Entity @inflicter, Entity @attacker ) ) },
	{ ASLIB_FUNCTION_DECL( void, entStop, ( Entity @ent ) ) },

	ASLIB_FUNCDEF_NULL
};

static const asBehavior_t gedict_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const asMethod_t gedict_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( Vec3, get_velocity, ( ) const ), asFUNCTION( objectGameEntity_GetVelocity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_velocity, ( const Vec3 &in ) ), asFUNCTION( objectGameEntity_SetVelocity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_avelocity, ( ) const ), asFUNCTION( objectGameEntity_GetAVelocity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_avelocity, ( const Vec3 &in ) ), asFUNCTION( objectGameEntity_SetAVelocity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_origin, ( ) const ), asFUNCTION( objectGameEntity_GetOrigin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_origin, ( const Vec3 &in ) ), asFUNCTION( objectGameEntity_SetOrigin ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_origin2, ( ) const ), asFUNCTION( objectGameEntity_GetOrigin2 ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_origin2, ( const Vec3 &in ) ), asFUNCTION( objectGameEntity_SetOrigin2 ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_angles, ( ) const ), asFUNCTION( objectGameEntity_GetAngles ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_angles, ( const Vec3 &in ) ), asFUNCTION( objectGameEntity_SetAngles ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, getSize, ( Vec3 & out, Vec3 & out ) ), asFUNCTION( objectGameEntity_GetSize ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, setSize, ( const Vec3 &in, const Vec3 &in ) ), asFUNCTION( objectGameEntity_SetSize ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_movedir, ( ) const ), asFUNCTION( objectGameEntity_GetMovedir ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, set_movedir, ( ) ), asFUNCTION( objectGameEntity_SetMovedir ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, freeEntity, ( ) ), asFUNCTION( G_FreeEdict ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, linkEntity, ( ) ), asFUNCTION( GClip_LinkEntity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, unlinkEntity, ( ) ), asFUNCTION( GClip_UnlinkEntity ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, isGhosting, ( ) const ), asFUNCTION( objectGameEntity_IsGhosting ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_entNum, ( ) const ), asFUNCTION( objectGameEntity_EntNum ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_playerNum, ( ) const ), asFUNCTION( objectGameEntity_PlayerNum ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, ghost, ( ) ), asFUNCTION( objectGameEntity_GhostClient ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, spawnqueueAdd, ( ) ), asFUNCTION( G_SpawnQueue_AddClient ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, teleportEffect, ( bool ) ), asFUNCTION( objectGameEntity_TeleportEffect ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, respawnEffect, ( ) ), asFUNCTION( G_RespawnEffect ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, setupModel, ( ) ), asFUNCTION( objectGameEntity_SetupModel ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( array<Entity @> @, findTargets, ( ) const ), asFUNCTION( objectGameEntity_findTargets ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, useTargets, ( const Entity @activator ) ), asFUNCTION( objectGameEntity_UseTargets ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, sustainDamage, ( Entity @inflicter, Entity @attacker, const Vec3 &in dir, float damage, float knockback, WorldDamage mod ) ), asFUNCTION( objectGameEntity_sustainDamage ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, splashDamage, ( Entity @attacker, int radius, float damage, float knockback ) ), asFUNCTION( objectGameEntity_splashDamage ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( void, explosionEffect, ( int radius ) ), asFUNCTION( objectGameEntity_explosionEffect ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const asProperty_t gedict_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( Client @, client ), offsetof( edict_t, r.client ) },
	{ ASLIB_PROPERTY_DECL( Entity @, groundEntity ), offsetof( edict_t, groundentity ) },
	{ ASLIB_PROPERTY_DECL( Entity @, owner ), offsetof( edict_t, r.owner ) },
	{ ASLIB_PROPERTY_DECL( Entity @, enemy ), offsetof( edict_t, enemy ) },
	{ ASLIB_PROPERTY_DECL( Entity @, activator ), offsetof( edict_t, activator ) },
	{ ASLIB_PROPERTY_DECL( int, type ), offsetof( edict_t, s.type ) },
	{ ASLIB_PROPERTY_DECL( uint64, model ), offsetof( edict_t, s.model.hash ) },
	{ ASLIB_PROPERTY_DECL( uint64, model2 ), offsetof( edict_t, s.model2.hash ) },
	{ ASLIB_PROPERTY_DECL( bool, animating ), offsetof( edict_t, s.animating ) },
	{ ASLIB_PROPERTY_DECL( float, animation_time ), offsetof( edict_t, s.animation_time ) },
	{ ASLIB_PROPERTY_DECL( int, radius ), offsetof( edict_t, s.radius ) },
	{ ASLIB_PROPERTY_DECL( int, ownerNum ), offsetof( edict_t, s.ownerNum ) },
	{ ASLIB_PROPERTY_DECL( int, counterNum ), offsetof( edict_t, s.counterNum ) },
	{ ASLIB_PROPERTY_DECL( uint, silhouetteColor ), offsetof( edict_t, s.silhouetteColor ) },
	{ ASLIB_PROPERTY_DECL( int, weapon ), offsetof( edict_t, s.weapon ) },
	{ ASLIB_PROPERTY_DECL( bool, teleported ), offsetof( edict_t, s.teleported ) },
	{ ASLIB_PROPERTY_DECL( uint, effects ), offsetof( edict_t, s.effects ) },
	{ ASLIB_PROPERTY_DECL( uint64, sound ), offsetof( edict_t, s.sound ) },
	{ ASLIB_PROPERTY_DECL( int, team ), offsetof( edict_t, s.team ) },
	{ ASLIB_PROPERTY_DECL( const bool, inuse ), offsetof( edict_t, r.inuse ) },
	{ ASLIB_PROPERTY_DECL( uint, svflags ), offsetof( edict_t, r.svflags ) },
	{ ASLIB_PROPERTY_DECL( int, solid ), offsetof( edict_t, r.solid ) },
	{ ASLIB_PROPERTY_DECL( int, clipMask ), offsetof( edict_t, r.clipmask ) },
	{ ASLIB_PROPERTY_DECL( int, spawnFlags ), offsetof( edict_t, spawnflags ) },
	{ ASLIB_PROPERTY_DECL( int, style ), offsetof( edict_t, style ) },
	{ ASLIB_PROPERTY_DECL( int, moveType ), offsetof( edict_t, movetype ) },
	{ ASLIB_PROPERTY_DECL( int64, nextThink ), offsetof( edict_t, nextThink ) },
	{ ASLIB_PROPERTY_DECL( float, health ), offsetof( edict_t, health ) },
	{ ASLIB_PROPERTY_DECL( int, viewHeight ), offsetof( edict_t, viewheight ) },
	{ ASLIB_PROPERTY_DECL( int, takeDamage ), offsetof( edict_t, takedamage ) },
	{ ASLIB_PROPERTY_DECL( int, damage ), offsetof( edict_t, dmg ) },
	{ ASLIB_PROPERTY_DECL( int, projectileMaxDamage ), offsetof( edict_t, projectileInfo.maxDamage ) },
	{ ASLIB_PROPERTY_DECL( int, projectileMinDamage ), offsetof( edict_t, projectileInfo.minDamage ) },
	{ ASLIB_PROPERTY_DECL( int, projectileMaxKnockback ), offsetof( edict_t, projectileInfo.maxKnockback ) },
	{ ASLIB_PROPERTY_DECL( int, projectileMinKnockback ), offsetof( edict_t, projectileInfo.minKnockback ) },
	{ ASLIB_PROPERTY_DECL( float, projectileSplashRadius ), offsetof( edict_t, projectileInfo.radius ) },
	{ ASLIB_PROPERTY_DECL( int, count ), offsetof( edict_t, count ) },
	{ ASLIB_PROPERTY_DECL( float, wait ), offsetof( edict_t, wait ) },
	{ ASLIB_PROPERTY_DECL( float, delay ), offsetof( edict_t, delay ) },
	{ ASLIB_PROPERTY_DECL( float, random ), offsetof( edict_t, random ) },
	{ ASLIB_PROPERTY_DECL( int, waterLevel ), offsetof( edict_t, waterlevel ) },
	{ ASLIB_PROPERTY_DECL( int, mass ), offsetof( edict_t, mass ) },
	{ ASLIB_PROPERTY_DECL( int64, timeStamp ), offsetof( edict_t, timeStamp ) },

	{ ASLIB_PROPERTY_DECL( entThink @, think ), offsetof( edict_t, asThinkFunc ) },
	{ ASLIB_PROPERTY_DECL( entTouch @, touch ), offsetof( edict_t, asTouchFunc ) },
	{ ASLIB_PROPERTY_DECL( entUse @, use ), offsetof( edict_t, asUseFunc ) },
	{ ASLIB_PROPERTY_DECL( entPain @, pain ), offsetof( edict_t, asPainFunc ) },
	{ ASLIB_PROPERTY_DECL( entDie @, die ), offsetof( edict_t, asDieFunc ) },
	{ ASLIB_PROPERTY_DECL( entStop @, stop ), offsetof( edict_t, asStopFunc ) },

	ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asGameEntityClassDescriptor =
{
	"Entity",                   /* name */
	asOBJ_REF | asOBJ_NOCOUNT,  /* object type flags */
	sizeof( edict_t ),          /* size */
	gedict_Funcdefs,            /* funcdefs */
	gedict_ObjectBehaviors,     /* object behaviors */
	gedict_Methods,             /* methods */
	gedict_Properties,          /* properties */

	NULL, NULL                  /* string factory hack */
};

// CLASS: Trace
typedef struct
{
	trace_t trace;
} astrace_t;

void objectTrace_DefaultConstructor( astrace_t *self ) {
	memset( &self->trace, 0, sizeof( trace_t ) );
}

void objectTrace_CopyConstructor( astrace_t *other, astrace_t *self ) {
	self->trace = other->trace;
}

static bool objectTrace_doTrace4D( asvec3_t *start, asvec3_t *mins, asvec3_t *maxs, asvec3_t *end, int ignore, int contentMask, int timeDelta, astrace_t *self ) {
	if( !start || !end ) { // should never happen unless the coder explicitly feeds null
		Com_Printf( "* WARNING: gametype plug-in script attempted to call method 'trace.doTrace' with a null vector pointer\n* Tracing skept" );
		return false;
	}

	server_gs.api.Trace( &self->trace, start->v, mins ? mins->v : Vec3( 0.0f ), maxs ? maxs->v : Vec3( 0.0f ), end->v, ignore, contentMask, 0 );

	if( self->trace.startsolid || self->trace.allsolid ) {
		return true;
	}

	return ( self->trace.ent != -1 ) ? true : false;
}

static bool objectTrace_doTrace( asvec3_t *start, asvec3_t *mins, asvec3_t *maxs, asvec3_t *end, int ignore, int contentMask, astrace_t *self ) {
	return objectTrace_doTrace4D( start, mins, maxs, end, ignore, contentMask, 0, self );
}

static asvec3_t objectTrace_getEndPos( astrace_t *self ) {
	asvec3_t asvec;

	asvec.v = self->trace.endpos;
	return asvec;
}

static asvec3_t objectTrace_getPlaneNormal( astrace_t *self ) {
	asvec3_t asvec;

	asvec.v = self->trace.plane.normal;
	return asvec;
}

static const asFuncdef_t astrace_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const asBehavior_t astrace_ObjectBehaviors[] =
{
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( ) ), asFUNCTION( objectTrace_DefaultConstructor ), asCALL_CDECL_OBJLAST },
	{ asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL( void, f, ( const Trace &in ) ), asFUNCTION( objectTrace_CopyConstructor ), asCALL_CDECL_OBJLAST },

	ASLIB_BEHAVIOR_NULL
};

static const asMethod_t astrace_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( bool, doTrace, ( const Vec3 &in, const Vec3 &in, const Vec3 &in, const Vec3 &in, int ignore, int contentMask ) const ), asFUNCTION( objectTrace_doTrace ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( bool, doTrace4D, ( const Vec3 &in, const Vec3 &in, const Vec3 &in, const Vec3 &in, int ignore, int contentMask, int timeDelta ) const ), asFUNCTION( objectTrace_doTrace4D ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_endPos, ( ) const ), asFUNCTION( objectTrace_getEndPos ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( Vec3, get_planeNormal, ( ) const ), asFUNCTION( objectTrace_getPlaneNormal ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const asProperty_t astrace_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( const bool, allSolid ), offsetof( astrace_t, trace.allsolid ) },
	{ ASLIB_PROPERTY_DECL( const bool, startSolid ), offsetof( astrace_t, trace.startsolid ) },
	{ ASLIB_PROPERTY_DECL( const float, fraction ), offsetof( astrace_t, trace.fraction ) },
	{ ASLIB_PROPERTY_DECL( const int, surfFlags ), offsetof( astrace_t, trace.surfFlags ) },
	{ ASLIB_PROPERTY_DECL( const int, contents ), offsetof( astrace_t, trace.contents ) },
	{ ASLIB_PROPERTY_DECL( const int, entNum ), offsetof( astrace_t, trace.ent ) },
	{ ASLIB_PROPERTY_DECL( const float, planeDist ), offsetof( astrace_t, trace.plane.dist ) },

	ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asTraceClassDescriptor =
{
	"Trace",                    /* name */
	asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CK,   /* object type flags */
	sizeof( astrace_t ),        /* size */
	astrace_Funcdefs,           /* funcdefs */
	astrace_ObjectBehaviors,    /* object behaviors */
	astrace_Methods,            /* methods */
	astrace_Properties,         /* properties */

	NULL, NULL                  /* string factory hack */
};

static const asClassDescriptor_t * const asGameClassesDescriptors[] =
{
	&asMatchClassDescriptor,
	&asGametypeClassDescriptor,
	&asTeamListClassDescriptor,
	&asScoreStatsClassDescriptor,
	&asGameClientDescriptor,
	&asGameEntityClassDescriptor,
	&asTraceClassDescriptor,

	NULL
};

static edict_t *asFunc_G_Spawn( asstring_t *classname ) {
	edict_t *ent;

	if( !level.canSpawnEntities ) {
		Com_Printf( "* WARNING: Spawning entities is disallowed during initialization. Returning null object\n" );
		return NULL;
	}

	ent = G_Spawn();

	if( classname && classname->len ) {
		ent->classname = StringHash( classname->buffer );
	}

	return ent;
}

static edict_t *asFunc_GetEntity( int entNum ) {
	if( entNum < 0 || entNum >= game.numentities ) {
		return NULL;
	}

	return &game.edicts[ entNum ];
}

static gclient_t *asFunc_GetClient( int clientNum ) {
	if( clientNum < 0 || clientNum >= server_gs.maxclients ) {
		return NULL;
	}

	return &game.clients[ clientNum ];
}

static SyncTeamState * asFunc_GetTeamlist( int teamNum ) {
	assert( teamNum >= TEAM_SPECTATOR || teamNum < GS_MAX_TEAMS );

	return &server_gs.gameState.teams[ teamNum ];
}

static void asFunc_G_Match_RemoveProjectiles( edict_t *owner ) {
	G_Match_RemoveProjectiles( owner );
}

static void asFunc_G_Match_RemoveAllProjectiles() {
	G_Match_RemoveProjectiles( NULL );
}

static void asFunc_G_ResetLevel() {
	G_ResetLevel();
}

static void asFunc_G_Match_FreeBodyQueue() {
	G_Match_FreeBodyQueue();
}

static void asFunc_Print( const asstring_t *str ) {
	if( !str || !str->buffer ) {
		return;
	}

	Com_Printf( "%s", str->buffer );
}

static void asFunc_PrintMsg( edict_t *ent, asstring_t *str ) {
	if( !str || !str->buffer ) {
		return;
	}

	G_PrintMsg( ent, "%s", str->buffer );
}

static void asFunc_CenterPrintMsg( edict_t *ent, asstring_t *str ) {
	if( !str || !str->buffer ) {
		return;
	}

	G_CenterPrintMsg( ent, "%s", str->buffer );
}

static void asFunc_ClearCenterPrint( edict_t *ent ) {
	G_ClearCenterPrint( ent );
}

static void asFunc_Error( const asstring_t *str ) {
	Com_Error( ERR_DROP, "%s", str && str->buffer ? str->buffer : "" );
}

static void asFunc_G_Sound( edict_t *owner, int channel, u64 sound ) {
	G_Sound( owner, channel, StringHash( sound ) );
}

static void asFunc_G_VFX( Vec3 pos, u64 vfx ) {
	G_SpawnEvent( EV_VFX, vfx, &pos );
}

static int asFunc_PointContents( asvec3_t *vec ) {
	if( !vec ) {
		return 0;
	}

	return G_PointContents( vec->v );
}

static void asFunc_Cbuf_ExecuteText( asstring_t *str ) {
	if( !str || !str->buffer || !str->buffer[0] ) {
		return;
	}

	Cbuf_ExecuteText( EXEC_APPEND, str->buffer );
}

static WeaponCategory asFunc_GetWeaponCategory( WeaponType weapon ) {
	return GS_GetWeaponDef( weapon )->category;
}

static asstring_t * asFunc_GetWeaponShortName( WeaponType weapon ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );
	return game.asExport->asStringFactoryBuffer( def->short_name, strlen( def->short_name ) );
}

static u64 asFunc_Hash64( asstring_t *str ) {
	if( !str || !str->buffer ) {
		return 0;
	}

	return Hash64( str->buffer );
}

static void asFunc_RegisterCommand( asstring_t *str ) {
	if( !str || !str->buffer || !str->len ) {
		return;
	}

	G_AddCommand( str->buffer, NULL );
}

static asstring_t *asFunc_GetConfigString( int index ) {
	const char *cs = PF_GetConfigString( index );
	return game.asExport->asStringFactoryBuffer( (char *)cs, cs ? strlen( cs ) : 0 );
}

static void asFunc_SetConfigString( int index, asstring_t *str ) {
	if( !str || !str->buffer ) {
		return;
	}

	// write protect some configstrings
	if( index <= SERVER_PROTECTED_CONFIGSTRINGS ) {
		Com_Printf( "WARNING: ConfigString %i is write protected\n", index );
		return;
	}

	PF_ConfigString( index, str->buffer );
}

static CScriptArrayInterface *asFunc_G_FindInRadius( asvec3_t *org, float radius ) {
	asIObjectType *ot = asEntityArrayType();

	int touch[MAX_EDICTS];
	int numtouch = GClip_FindInRadius( org->v, radius, touch, MAX_EDICTS );
	CScriptArrayInterface *arr = game.asExport->asCreateArrayCpp( numtouch, ot );
	for( int i = 0; i < numtouch; i++ ) {
		*( (edict_t **)arr->At( i ) ) = game.edicts + touch[i];
	}

	return arr;
}

static CScriptArrayInterface *asFunc_G_FindByClassname( asstring_t *str ) {
	StringHash classname = StringHash( str->buffer );

	asIObjectType *ot = asEntityArrayType();
	CScriptArrayInterface *arr = game.asExport->asCreateArrayCpp( 0, ot );

	int count = 0;
	edict_t *ent = NULL;
	while( ( ent = G_Find( ent, &edict_t::classname, classname ) ) != NULL ) {
		arr->Resize( count + 1 );
		*( (edict_t **)arr->At( count ) ) = ent;
		count++;
	}

	return arr;
}

static edict_t * asFunc_G_Find( edict_t * cursor, asstring_t * str ) {
	StringHash value = StringHash( str->buffer );
	return G_Find( cursor, &edict_t::classname, value );
}

void G_Aasdf(); // TODO
static void asFunc_G_LoadMap( asstring_t *str ) {
	G_LoadMap( str->buffer );
	G_Aasdf();
}

static asstring_t *asFunc_G_GetWorldspawnKey( asstring_t * key ) {
	Span< const char > value = ParseWorldspawnKey( MakeSpan( CM_EntityString( svs.cms ) ), key->buffer );
	return game.asExport->asStringFactoryBuffer( value.ptr, value.n );
}

static void asFunc_PositionedSound( asvec3_t *origin, int channel, u64 sound ) {
	if( !origin ) {
		return;
	}

	G_PositionedSound( origin->v, channel, StringHash( sound ) );
}

static void asFunc_G_GlobalSound( int channel, u64 sound ) {
	G_GlobalSound( channel, StringHash( sound ) );
}

static void asFunc_G_LocalSound( gclient_t *target, int channel, u64 sound ) {
	edict_t *ent = NULL;

	if( !target ) {
		return;
	}

	if( target ) {
		int playerNum = target - game.clients;

		if( playerNum < 0 || playerNum >= server_gs.maxclients ) {
			return;
		}

		ent = game.edicts + playerNum + 1;
	}

	if( ent ) {
		G_LocalSound( ent, channel, StringHash( sound ) );
	}
}

static void asFunc_G_AnnouncerSound( gclient_t *target, u64 sound, int team, bool queued, gclient_t *ignore ) {
	edict_t *ent = NULL, *passent = NULL;
	int playerNum;

	if( target ) {
		playerNum = target - game.clients;

		if( playerNum < 0 || playerNum >= server_gs.maxclients ) {
			return;
		}

		ent = game.edicts + playerNum + 1;
	}

	if( ignore ) {
		playerNum = ignore - game.clients;

		if( playerNum >= 0 && playerNum < server_gs.maxclients ) {
			passent = game.edicts + playerNum + 1;
		}
	}


	G_AnnouncerSound( ent, StringHash( sound ), team, queued, passent );
}

static const asglobfuncs_t asGameGlobFuncs[] =
{
	{ "Entity @G_SpawnEntity( const String &in )", asFUNCTION( asFunc_G_Spawn ), NULL },
	{ "Entity @G_GetEntity( int entNum )", asFUNCTION( asFunc_GetEntity ), NULL },
	{ "Client @G_GetClient( int clientNum )", asFUNCTION( asFunc_GetClient ), NULL },
	{ "Team @G_GetTeam( int team )", asFUNCTION( asFunc_GetTeamlist ), NULL },
	{ "array<Entity @> @G_FindInRadius( const Vec3 &in, float radius )", asFUNCTION( asFunc_G_FindInRadius ), NULL },
	{ "array<Entity @> @G_FindByClassname( const String &in )", asFUNCTION( asFunc_G_FindByClassname ), NULL },
	{ "Entity @G_Find( Entity @last, const String &in )", asFUNCTION( asFunc_G_Find ), NULL },

	{ "void G_LoadMap( const String &name )", asFUNCTION( asFunc_G_LoadMap ), NULL },
	{ "const String @G_GetWorldspawnKey( const String &key )", asFUNCTION( asFunc_G_GetWorldspawnKey ), NULL },

	// misc management utils
	{ "void G_RemoveProjectiles( Entity @ )", asFUNCTION( asFunc_G_Match_RemoveProjectiles ), NULL },
	{ "void G_RemoveAllProjectiles()", asFUNCTION( asFunc_G_Match_RemoveAllProjectiles ), NULL },
	{ "void G_ResetLevel()", asFUNCTION( asFunc_G_ResetLevel ), NULL },
	{ "void G_RemoveDeadBodies()", asFUNCTION( asFunc_G_Match_FreeBodyQueue ), NULL },

	// misc
	{ "void G_Print( const String &in )", asFUNCTION( asFunc_Print ), NULL },
	{ "void G_PrintMsg( Entity @, const String &in )", asFUNCTION( asFunc_PrintMsg ), NULL },
	{ "void G_CenterPrintMsg( Entity @, const String &in )", asFUNCTION( asFunc_CenterPrintMsg ), NULL },
	{ "void G_ClearCenterPrint( Entity @ )", asFUNCTION( asFunc_ClearCenterPrint ), NULL },
	{ "void Com_Error( const String &in )", asFUNCTION( asFunc_Error ), NULL },
	{ "void G_Sound( Entity @, int channel, uint64 sound )", asFUNCTION( asFunc_G_Sound ), NULL },
	{ "void G_PositionedSound( const Vec3 &in, int channel, uint64 sound )", asFUNCTION( asFunc_PositionedSound ), NULL },
	{ "void G_GlobalSound( int channel, uint64 sound )", asFUNCTION( asFunc_G_GlobalSound ), NULL },
	{ "void G_LocalSound( Client @, int channel, uint64 sound )", asFUNCTION( asFunc_G_LocalSound ), NULL },
	{ "void G_AnnouncerSound( Client @, uint64 sound, int team, bool queued, Client @ )", asFUNCTION( asFunc_G_AnnouncerSound ), NULL },
	{ "void G_VFX( const Vec3 &in, uint64 vfx )", asFUNCTION( asFunc_G_VFX ), NULL },
	{ "int G_PointContents( const Vec3 &in origin )", asFUNCTION( asFunc_PointContents ), NULL },
	{ "void G_CmdExecute( const String & )", asFUNCTION( asFunc_Cbuf_ExecuteText ), NULL },

	{ "void G_RegisterCommand( const String &in )", asFUNCTION( asFunc_RegisterCommand ), NULL },
	{ "const String @G_ConfigString( int index )", asFUNCTION( asFunc_GetConfigString ), NULL },
	{ "void G_ConfigString( int index, const String &in )", asFUNCTION( asFunc_SetConfigString ), NULL },

	{ "WeaponCategory GetWeaponCategory( WeaponType )", asFUNCTION( asFunc_GetWeaponCategory ), NULL },
	{ "const String @GetWeaponShortName( WeaponType )", asFUNCTION( asFunc_GetWeaponShortName ), NULL },

	{ "uint64 Hash64( const String &in )", asFUNCTION( asFunc_Hash64 ), NULL },

	{ NULL }
};

static const asglobproperties_t asGlobProps[] =
{
	{ "const int64 levelTime", &level.time },
	{ "const uint frameTime", &game.frametime },
	{ "const int64 gameTime", &svs.gametime },
	{ "const int64 realTime", &svs.realtime },

	{ "const int maxEntities", &game.maxentities },
	{ "const int numEntities", &game.numentities },
	{ "const int maxClients", &server_gs.maxclients },
	{ "GametypeDesc gametype", &level.gametype },
	{ "Match match", &server_gs.gameState },

	{ NULL }
};

// map entity spawning
bool G_asCallMapEntitySpawnScript( Span< const char > classname, edict_t *ent ) {
	int error;
	asIScriptContext *asContext;
	asIScriptEngine *asEngine = game.asEngine;
	asIScriptModule *asSpawnModule;
	asIScriptFunction *asSpawnFunc;

	if( !asEngine ) {
		return false;
	}

	TempAllocator temp = svs.frame_arena.temp();
	DynamicString signature( &temp, "void {}( Entity @ ent )", classname );

	// lookup the spawn function in gametype module first, fallback to map script
	asSpawnModule = asEngine->GetModule( GAMETYPE_SCRIPTS_MODULE_NAME );
	asSpawnFunc = asSpawnModule ? asSpawnModule->GetFunctionByDecl( signature.c_str() ) : NULL;
	if( !asSpawnFunc ) {
		asSpawnModule = asEngine->GetModule( MAP_SCRIPTS_MODULE_NAME );
		asSpawnFunc = asSpawnModule ? asSpawnModule->GetFunctionByDecl( signature.c_str() ) : NULL;
	}

	if( !asSpawnFunc ) {
		return false;
	}

	// this is in case we might want to call G_asReleaseEntityBehaviors
	// in the spawn function (an object may release itself, ugh)
	ent->asSpawnFunc = asSpawnFunc;

	// call the spawn function
	asContext = game.asExport->asAcquireContext( asEngine );
	error = asContext->Prepare( asSpawnFunc );
	if( error < 0 ) {
		return false;
	}

	// Now we need to pass the parameters to the script function.
	asContext->SetArgObject( 0, ent );

	error = asContext->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		GT_asShutdownScript();
		ent->asSpawnFunc = NULL;
		return false;
	}

	return true;
}

/*
* G_asClearEntityBehaviors
*/
void G_asClearEntityBehaviors( edict_t *ent ) {
	ent->asThinkFunc = NULL;
	ent->asTouchFunc = NULL;
	ent->asUseFunc = NULL;
	ent->asStopFunc = NULL;
	ent->asPainFunc = NULL;
	ent->asDieFunc = NULL;
}

/*
* G_asReleaseEntityBehaviors
*
* Release callback function references held by the engine for script spawned entities
*/
void G_asReleaseEntityBehaviors( edict_t *ent ) {
	if( ent->asThinkFunc ) {
		ent->asThinkFunc->Release();
	}
	if( ent->asTouchFunc ) {
		ent->asTouchFunc->Release();
	}
	if( ent->asUseFunc ) {
		ent->asUseFunc->Release();
	}
	if( ent->asStopFunc ) {
		ent->asStopFunc->Release();
	}
	if( ent->asPainFunc ) {
		ent->asPainFunc->Release();
	}
	if( ent->asDieFunc ) {
		ent->asDieFunc->Release();
	}

	G_asClearEntityBehaviors( ent );
}

//"void %s_think( Entity @ent )"
void G_asCallMapEntityThink( edict_t *ent ) {
	int error;
	asIScriptContext *ctx;

	if( !ent->asThinkFunc ) {
		return;
	}

	ctx = game.asExport->asAcquireContext( game.asEngine );

	error = ctx->Prepare( ent->asThinkFunc );
	if( error < 0 ) {
		return;
	}

	// Now we need to pass the parameters to the script function.
	ctx->SetArgObject( 0, ent );

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		GT_asShutdownScript();
	}
}

// "void %s_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )"
void G_asCallMapEntityTouch( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	int error;
	asIScriptContext *ctx;
	asvec3_t normal;

	if( !ent->asTouchFunc ) {
		return;
	}

	ctx = game.asExport->asAcquireContext( game.asEngine );

	error = ctx->Prepare( ent->asTouchFunc );
	if( error < 0 ) {
		return;
	}

	if( plane ) {
		normal.v = plane->normal;
	} else {
		normal.v = Vec3( 0.0f );
	}

	// Now we need to pass the parameters to the script function.
	ctx->SetArgObject( 0, ent );
	ctx->SetArgObject( 1, other );
	ctx->SetArgObject( 2, &normal );
	ctx->SetArgDWord( 3, surfFlags );

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		GT_asShutdownScript();
	}
}

// "void %s_use( Entity @ent, Entity @other, Entity @activator )"
void G_asCallMapEntityUse( edict_t *ent, edict_t *other, edict_t *activator ) {
	int error;
	asIScriptContext *ctx;

	if( !ent->asUseFunc ) {
		return;
	}

	ctx = game.asExport->asAcquireContext( game.asEngine );

	error = ctx->Prepare( ent->asUseFunc );
	if( error < 0 ) {
		return;
	}

	// Now we need to pass the parameters to the script function.
	ctx->SetArgObject( 0, ent );
	ctx->SetArgObject( 1, other );
	ctx->SetArgObject( 2, activator );

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		GT_asShutdownScript();
	}
}

// "void %s_pain( Entity @ent, Entity @other, float kick, float damage )"
void G_asCallMapEntityPain( edict_t *ent, edict_t *other, float kick, float damage ) {
	int error;
	asIScriptContext *ctx;

	if( !ent->asPainFunc ) {
		return;
	}

	ctx = game.asExport->asAcquireContext( game.asEngine );

	error = ctx->Prepare( ent->asPainFunc );
	if( error < 0 ) {
		return;
	}

	// Now we need to pass the parameters to the script function.
	ctx->SetArgObject( 0, ent );
	ctx->SetArgObject( 1, other );
	ctx->SetArgFloat( 2, kick );
	ctx->SetArgFloat( 3, damage );

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		GT_asShutdownScript();
	}
}

// "void %s_die( Entity @ent, Entity @inflicter, Entity @attacker )"
void G_asCallMapEntityDie( edict_t *ent, edict_t *inflicter, edict_t *attacker, int damage ) {
	int error;
	asIScriptContext *ctx;

	if( !ent->asDieFunc ) {
		return;
	}

	ctx = game.asExport->asAcquireContext( game.asEngine );

	error = ctx->Prepare( ent->asDieFunc );
	if( error < 0 ) {
		return;
	}

	// Now we need to pass the parameters to the script function.
	ctx->SetArgObject( 0, ent );
	ctx->SetArgObject( 1, inflicter );
	ctx->SetArgObject( 2, attacker );

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		GT_asShutdownScript();
	}
}

//"void %s_stop( Entity @ent )"
void G_asCallMapEntityStop( edict_t *ent ) {
	int error;
	asIScriptContext *ctx;

	if( !ent->asStopFunc ) {
		return;
	}

	ctx = game.asExport->asAcquireContext( game.asEngine );

	error = ctx->Prepare( ent->asStopFunc );
	if( error < 0 ) {
		return;
	}

	// Now we need to pass the parameters to the script function.
	ctx->SetArgObject( 0, ent );

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		GT_asShutdownScript();
	}
}

/*
* G_ExecutionErrorReport
*/
bool G_ExecutionErrorReport( int error ) {
	if( error == asEXECUTION_FINISHED ) {
		return false;
	}
	return true;
}

/*
* G_LoadGameScript
*/
asIScriptModule *G_LoadGameScript( const char *dir, const char *filename, const char *ext ) {
	return game.asExport->asLoadScriptProject( game.asEngine, GAME_SCRIPTS_DIRECTORY, dir, filename, ext );
}

/*
* G_ResetGameModuleScriptData
*/
static void G_ResetGameModuleScriptData() {
	game.asEngine = NULL;
}

/*
* G_asRegisterEnums
*/
static void G_asRegisterEnums( asIScriptEngine *asEngine, const asEnum_t *asEnums ) {
	int i, j;
	const asEnum_t *asEnum;
	const asEnumVal_t *asEnumVal;

	asEngine->SetDefaultNamespace( "" );

	for( i = 0, asEnum = asEnums; asEnum->name != NULL; i++, asEnum++ ) {
		asEngine->RegisterEnum( asEnum->name );

		for( j = 0, asEnumVal = asEnum->values; asEnumVal->name != NULL; j++, asEnumVal++ )
			asEngine->RegisterEnumValue( asEnum->name, asEnumVal->name, asEnumVal->value );
	}
}

/*
* G_asRegisterObjectClassNames
*/
static void G_asRegisterObjectClassNames( asIScriptEngine *asEngine,
	const asClassDescriptor_t *const *asClassesDescriptors ) {
	int i;
	const asClassDescriptor_t *cDescr;

	asEngine->SetDefaultNamespace( "" );

	for( i = 0; ; i++ ) {
		if( !( cDescr = asClassesDescriptors[i] ) ) {
			break;
		}
		asEngine->RegisterObjectType( cDescr->name, cDescr->size, cDescr->typeFlags );
	}
}

/*
* G_asRegisterObjectClasses
*/
static void G_asRegisterObjectClasses( asIScriptEngine *asEngine,
	const asClassDescriptor_t *const *asClassesDescriptors ) {
	int i, j;
	const asClassDescriptor_t *cDescr;

	asEngine->SetDefaultNamespace( "" );

	// now register object and global behaviors, then methods and properties
	for( i = 0; ; i++ ) {
		if( !( cDescr = asClassesDescriptors[i] ) ) {
			break;
		}

		// funcdefs
		if( cDescr->funcdefs ) {
			for( j = 0; ; j++ ) {
				const asFuncdef_t *funcdef = &cDescr->funcdefs[j];
				if( !funcdef->declaration ) {
					break;
				}
				asEngine->RegisterFuncdef( funcdef->declaration );
			}
		}

		// object behaviors
		if( cDescr->objBehaviors ) {
			for( j = 0; ; j++ ) {
				const asBehavior_t *objBehavior = &cDescr->objBehaviors[j];
				if( !objBehavior->declaration ) {
					break;
				}
				asEngine->RegisterObjectBehaviour(
					cDescr->name, objBehavior->behavior, objBehavior->declaration,
					objBehavior->funcPointer, objBehavior->callConv );
			}
		}

		// object methods
		if( cDescr->objMethods ) {
			for( j = 0; ; j++ ) {
				const asMethod_t *objMethod = &cDescr->objMethods[j];
				if( !objMethod->declaration ) {
					break;
				}

				asEngine->RegisterObjectMethod( cDescr->name,
					objMethod->declaration, objMethod->funcPointer,
					objMethod->callConv );
			}
		}

		// object properties
		if( cDescr->objProperties ) {
			for( j = 0; ; j++ ) {
				const asProperty_t *objProperty = &cDescr->objProperties[j];
				if( !objProperty->declaration ) {
					break;
				}

				asEngine->RegisterObjectProperty( cDescr->name,
					objProperty->declaration, objProperty->offset );
			}
		}
	}
}

/*
* G_asRegisterGlobalFunctions
*/
static void G_asRegisterGlobalFunctions( asIScriptEngine *asEngine,
	const asglobfuncs_t *funcs ) {
	int error;
	int count = 0, failedcount = 0;
	const asglobfuncs_t *func;

	asEngine->SetDefaultNamespace( "" );

	for( func = funcs; func->declaration; func++ ) {
		error = asEngine->RegisterGlobalFunction( func->declaration, func->pointer, asCALL_CDECL );

		if( error < 0 ) {
			failedcount++;
			continue;
		}

		count++;
	}

	// get AS function pointers
	for( func = funcs; func->declaration; func++ ) {
		if( func->asFuncPtr ) {
			*func->asFuncPtr = asEngine->GetGlobalFunctionByDecl( func->declaration );
		}
	}
}

/*
* G_asRegisterGlobalProperties
*/
static void G_asRegisterGlobalProperties( asIScriptEngine *asEngine,
	const asglobproperties_t *props ) {
	int error;
	int count = 0, failedcount = 0;
	const asglobproperties_t *prop;

	asEngine->SetDefaultNamespace( "" );

	for( prop = props; prop->declaration; prop++ ) {
		error = asEngine->RegisterGlobalProperty( prop->declaration, prop->pointer );
		if( error < 0 ) {
			failedcount++;
			continue;
		}

		count++;
	}
}

/*
* G_InitializeGameModuleSyntax
*/
static void G_InitializeGameModuleSyntax( asIScriptEngine *asEngine ) {
	// register global enums
	G_asRegisterEnums( asEngine, asGameEnums );

	// first register all class names so methods using custom classes work
	G_asRegisterObjectClassNames( asEngine, asGameClassesDescriptors );

	// register classes
	G_asRegisterObjectClasses( asEngine, asGameClassesDescriptors );

	// register global functions
	G_asRegisterGlobalFunctions( asEngine, asGameGlobFuncs );

	// register global properties
	G_asRegisterGlobalProperties( asEngine, asGlobProps );
}

/*
* G_asInitGameModuleEngine
*/
void G_asInitGameModuleEngine() {
	bool asGeneric;
	asIScriptEngine *asEngine;

	G_ResetGameModuleScriptData();

	// initialize the engine
	game.asExport = QAS_GetAngelExport();

	asEngine = game.asExport->asCreateEngine( &asGeneric );
	if( !asEngine ) {
		Com_Printf( "* Couldn't initialize angelscript.\n" );
		return;
	}

	if( asGeneric ) {
		Com_Printf( "* Generic calling convention detected, aborting.\n" );
		G_asShutdownGameModuleEngine();
		return;
	}

	game.asEngine = asEngine;

	G_InitializeGameModuleSyntax( asEngine );
}

/*
* G_asShutdownGameModuleEngine
*/
void G_asShutdownGameModuleEngine() {
	if( game.asEngine == NULL ) {
		return;
	}

	game.asExport->asReleaseEngine( game.asEngine );
	G_ResetGameModuleScriptData();
}

/*
* G_asGarbageCollect
*
* Perform garbage collection procedure
*/
void G_asGarbageCollect( bool force ) {
	static int64_t lastTime = 0;
	unsigned int currentSize, totalDestroyed, totalDetected;
	asIScriptEngine *asEngine;

	if( !game.asExport ) {
		return;
	}

	asEngine = game.asEngine;
	if( !asEngine ) {
		return;
	}

	if( lastTime > svs.gametime ) {
		force = true;
	}

	if( force || lastTime + g_asGC_interval->value * 1000 < svs.gametime ) {
		asEngine->GetGCStatistics( &currentSize, &totalDestroyed, &totalDetected );

		if( g_asGC_stats->integer ) {
			Com_Printf( "GC: t=%" PRIi64 " size=%u destroyed=%u detected=%u\n", svs.gametime, currentSize, totalDestroyed, totalDetected );
		}

		asEngine->GarbageCollect();

		lastTime = svs.gametime;
	}
}
