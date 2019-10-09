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

// cg_local.h -- local definitions for client game module

#include "qcommon/types.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"
#include "ref.h"

#include "cg_public.h"
#include "cg_syscalls.h"

#include "client/sound.h"
#include "client/renderer/renderer.h"
#include "client/renderer/text.h"

#define CG_OBITUARY_HUD     1
#define CG_OBITUARY_CENTER  2
#define CG_OBITUARY_CONSOLE 4

#define ITEM_RESPAWN_TIME   1000

#define VSAY_TIMEOUT 2500

#define GAMECHAT_STRING_SIZE    1024
#define GAMECHAT_STACK_SIZE     20

enum {
	LOCALEFFECT_EV_PLAYER_TELEPORT_IN,
	LOCALEFFECT_EV_PLAYER_TELEPORT_OUT,
	LOCALEFFECT_VSAY_TIMEOUT,
	LOCALEFFECT_ROCKETTRAIL_LAST_DROP,
	LOCALEFFECT_ROCKETFIRE_LAST_DROP,
	LOCALEFFECT_GRENADETRAIL_LAST_DROP,
	LOCALEFFECT_BLOODTRAIL_LAST_DROP,
	LOCALEFFECT_FLAGTRAIL_LAST_DROP,
	LOCALEFFECT_LASERBEAM,
	LOCALEFFECT_LASERBEAM_SMOKE_TRAIL,
	LOCALEFFECT_EV_WEAPONBEAM,
	MAX_LOCALEFFECTS = 64,
};

typedef struct {
	entity_state_t current;
	entity_state_t prev;        // will always be valid, but might just be a copy of current

	int serverFrame;            // if not current, this ent isn't in the frame
	int64_t fly_stoptime;

	int64_t respawnTime;

	entity_t ent;                   // interpolated, to be added to render list
	unsigned int type;
	unsigned int effects;

	vec3_t velocity;

	bool canExtrapolate;
	bool canExtrapolatePrev;
	vec3_t prevVelocity;
	int microSmooth;
	vec3_t microSmoothOrigin;
	vec3_t microSmoothOrigin2;

	// effects
	vec3_t trailOrigin;         // for particle trails

	// local effects from events timers
	int64_t localEffects[MAX_LOCALEFFECTS];

	// attached laser beam
	vec3_t laserOrigin;
	vec3_t laserPoint;
	vec3_t laserOriginOld;
	vec3_t laserPointOld;

	bool linearProjectileCanDraw;
	vec3_t linearProjectileViewerSource;
	vec3_t linearProjectileViewerVelocity;

	vec3_t teleportedTo;
	vec3_t teleportedFrom;
	byte_vec4_t outlineColor;

	// used for client side animation of player models
	int lastVelocitiesFrames[4];
	float lastVelocities[4][4];
	bool jumpedLeft;
	vec3_t animVelocity;
	float yawVelocity;
} centity_t;

#include "cg_pmodels.h"

typedef struct {
	// sounds
	const SoundAsset * sfxWeaponUp;
	const SoundAsset * sfxWeaponUpNoAmmo;

	const SoundAsset * sfxWeaponHit[4];
	const SoundAsset * sfxWeaponKill;
	const SoundAsset * sfxWeaponHitTeam;

	const SoundAsset * sfxItemRespawn;
	const SoundAsset * sfxTeleportIn;
	const SoundAsset * sfxTeleportOut;
	const SoundAsset * sfxShellHit;

	const SoundAsset * sfxBladeFleshHit[3];
	const SoundAsset * sfxBladeWallHit[2];

	const SoundAsset * sfxRic[2];

	const SoundAsset * sfxRiotgunHit;

	const SoundAsset * sfxGrenadeBounce[2];
	const SoundAsset * sfxGrenadeExplosion;

	const SoundAsset * sfxRocketLauncherHit;

	const SoundAsset * sfxPlasmaHit;

	const SoundAsset * sfxLasergunHum;
	const SoundAsset * sfxLasergunStop;
	const SoundAsset * sfxLasergunHit[3];

	const SoundAsset * sfxElectroboltHit;

	const SoundAsset * sfxVSaySounds[VSAY_TOTAL];

	const SoundAsset * sfxSpikesArm;
	const SoundAsset * sfxSpikesDeploy;
	const SoundAsset * sfxSpikesGlint;
	const SoundAsset * sfxSpikesRetract;

	const SoundAsset * sfxFall;

	// models
	const Model * modDash;
	const Model * modGib;

	const Model * modPlasmaExplosion;

	const Model * modBulletExplode;
	const Model * modBladeWallHit;
	const Model * modBladeWallExplo;

	const Model * modElectroBoltWallHit;

	const Model * modLasergunWallExplo;

	const Material * shaderParticle;
	const Material * shaderRocketExplosion;
	const Material * shaderRocketExplosionRing;
	const Material * shaderGrenadeExplosion;
	const Material * shaderGrenadeExplosionRing;
	const Material * shaderBulletExplosion;
	const Material * shaderRaceGhostEffect;
	const Material * shaderWaterBubble;
	const Material * shaderSmokePuff;

	const Material * shaderSmokePuff1;
	const Material * shaderSmokePuff2;
	const Material * shaderSmokePuff3;

	const Material * shaderRocketFireTrailPuff;
	const Material * shaderGrenadeTrailSmokePuff;
	const Material * shaderRocketTrailSmokePuff;
	const Material * shaderBloodTrailPuff;
	const Material * shaderBloodTrailLiquidPuff;
	const Material * shaderBloodImpactPuff;
	const Material * shaderTeamMateIndicator;
	const Material * shaderTeamCarrierIndicator;
	const Material * shaderBombIcon;
	const Material * shaderTeleporterSmokePuff;
	const Material * shaderBladeMark;
	const Material * shaderBulletMark;
	const Material * shaderExplosionMark;
	const Material * shaderEnergyMark;
	const Material * shaderLaser;
	const Material * shaderNet;
	const Material * shaderTeleportShellGfx;

	const Material * shaderPlasmaMark;
	const Material * shaderEBBeam;
	const Material * shaderLGBeam;
	const Material * shaderEBImpact;

	const Material * shaderPlayerShadow;

	const Material * shaderTick;

	const Material * shaderWeaponIcon[WEAP_TOTAL];
	const Material * shaderKeyIcon[KEYICON_TOTAL];

	const Material * shaderAlive;
	const Material * shaderDead;
	const Material * shaderReady;
} cgs_media_t;

typedef struct {
	char name[MAX_QPATH];
	char cleanname[MAX_QPATH];
	int hand;
	struct shader_s *icon;
} cg_clientInfo_t;

#define MAX_ANGLES_KICKS 3

typedef struct {
	int64_t timestamp;
	int64_t kicktime;
	float v_roll, v_pitch;
} cg_kickangles_t;

#define MAX_COLORBLENDS 3

typedef struct {
	int64_t timestamp;
	int64_t blendtime;
	float blend[4];
} cg_viewblend_t;

#define PREDICTED_STEP_TIME 150 // stairs smoothing time
#define MAX_AWARD_LINES 3
#define MAX_AWARD_DISPLAYTIME 5000

// view types
enum {
	VIEWDEF_DEMOCAM,
	VIEWDEF_PLAYERVIEW,
};

typedef struct {
	int type;
	int POVent;
	bool thirdperson;
	bool playerPrediction;
	bool drawWeapon;
	bool draw2D;
	float fov_x, fov_y;
	float fracDistFOV;
	vec3_t origin;
	vec3_t angles;
	mat3_t axis;
	vec3_t velocity;
	refdef_t refdef;
} cg_viewdef_t;

#include "cg_democams.h"

// this is not exactly "static" but still...
typedef struct {
	const char *serverName;
	const char *demoName;
	unsigned int playerNum;

	// materials
	const Material * white_material;

	// fonts
	int fontSystemTinySize;
	int fontSystemSmallSize;
	int fontSystemMediumSize;
	int fontSystemBigSize;

	float textSizeTiny;
	float textSizeSmall;
	float textSizeMedium;
	float textSizeBig;

	const Font * fontMontserrat;
	const Font * fontMontserratBold;
	const Font * fontMontserratItalic;
	const Font * fontMontserratBoldItalic;

	cgs_media_t media;

	bool precacheDone;

	bool demoPlaying;
	bool pure;
	unsigned snapFrameTime;
	unsigned extrapolationTime;

	char *demoAudioStream;

	//
	// locally derived information from server state
	//
	char configStrings[MAX_CONFIGSTRINGS][MAX_CONFIGSTRING_CHARS];
	char baseConfigStrings[MAX_CONFIGSTRINGS][MAX_CONFIGSTRING_CHARS];

	bool hasGametypeMenu;

	char weaponModels[WEAP_TOTAL][MAX_QPATH];
	int numWeaponModels;
	weaponinfo_t *weaponInfos[WEAP_TOTAL];    // indexed list of weapon model infos
	orientation_t weaponItemTag;

	cg_clientInfo_t clientInfo[MAX_CLIENTS];

	const Model *modelDraw[MAX_MODELS];

	const MapMetadata * map;

	PlayerModelMetadata *pModelsIndex[MAX_MODELS];
	PlayerModelMetadata *basePModelInfo; //fall back replacements

	// force models
	PlayerModelMetadata *teamModelInfo[2];

	const SoundAsset *soundPrecache[MAX_SOUNDS];
	const Material *imagePrecache[MAX_IMAGES];

	int precacheModelsStart;
	int precacheSoundsStart;
	int precacheShadersStart;
	int precacheClientsStart;

	char checkname[MAX_QPATH];
	int precacheCount, precacheTotal, precacheStart;
	int64_t precacheStartMsec;
} cg_static_t;

typedef struct {
	int64_t time;
	float delay;

	int64_t monotonicTime;

	int64_t realTime;
	int frameTime;
	int realFrameTime;
	int frameCount;

	int64_t firstViewRealTime;
	int viewFrameCount;

	snapshot_t frame, oldFrame;
	bool frameSequenceRunning;
	bool oldAreabits;
	bool fireEvents;
	bool firstFrame;

	float predictedOrigins[CMD_BACKUP][3];              // for debug comparing against server

	float predictedStep;                // for stair up smoothing
	int64_t predictedStepTime;

	int64_t predictingTimeStamp;
	int64_t predictedEventTimes[PREDICTABLE_EVENTS_MAX];
	vec3_t predictionError;
	player_state_t predictedPlayerState;     // current in use, predicted or interpolated
	int predictedWeaponSwitch;              // inhibit shooting prediction while a weapon change is expected
	int predictedGroundEntity;

	// prediction optimization (don't run all ucmds in not needed)
	int64_t predictFrom;
	entity_state_t predictFromEntityState;
	player_state_t predictFromPlayerState;

	int lastWeapon;

	mat3_t autorotateAxis;

	float lerpfrac;                     // between oldframe and frame
	float xerpTime;
	float oldXerpTime;
	float xerpSmoothFrac;

	int effects;

	bool showScoreboard;            // demos and multipov

	unsigned int multiviewPlayerNum;       // for multipov chasing, takes effect on next snap

	int pointedNum;
	int64_t pointRemoveTime;
	int pointedHealth;

	//
	// all cyclic walking effects
	//
	float xyspeed;

	float oldBobTime;
	int bobCycle;                   // odd cycles are right foot going forward
	float bobFracSin;               // sin(bobfrac*M_PI)

	//
	// kick angles and color blend effects
	//

	cg_kickangles_t kickangles[MAX_ANGLES_KICKS];
	cg_viewblend_t colorblends[MAX_COLORBLENDS];
	int64_t damageBlends[4];
	int64_t fallEffectTime;
	int64_t fallEffectRebounceTime;

	// awards
	char award_lines[MAX_AWARD_LINES][MAX_CONFIGSTRING_CHARS];
	int64_t award_times[MAX_AWARD_LINES];
	int award_head;

	// statusbar program
	struct cg_layoutnode_s *statusBar;

	cg_viewweapon_t weapon;
	cg_viewdef_t view;
} cg_state_t;

extern cg_static_t cgs;
extern cg_state_t cg;

extern mempool_t *cg_mempool;

#define ISVIEWERENTITY( entNum )  ( ( cg.predictedPlayerState.POVnum > 0 ) && ( (int)cg.predictedPlayerState.POVnum == entNum ) && ( cg.view.type == VIEWDEF_PLAYERVIEW ) )
#define ISBRUSHMODEL( x ) ( ( ( x > 0 ) && ( (int)x < trap_CM_NumInlineModels() ) ) ? true : false )

#define ISREALSPECTATOR()       ( cg.frame.playerState.stats[STAT_REALTEAM] == TEAM_SPECTATOR )

extern centity_t cg_entities[MAX_EDICTS];

//
// cg_ents.c
//
extern cvar_t *cg_gun;

bool CG_NewFrameSnap( snapshot_t *frame, snapshot_t *lerpframe );
struct cmodel_s *CG_CModelForEntity( int entNum );
void CG_SoundEntityNewState( centity_t *cent );
void CG_AddEntities( void );
void CG_GetEntitySpatilization( int entNum, vec3_t origin, vec3_t velocity );
void CG_LerpEntities( void );
void CG_LerpGenericEnt( centity_t *cent );

void CG_AddColoredOutLineEffect( entity_t *ent, int effects, uint8_t r, uint8_t g, uint8_t b, uint8_t a );

//
// cg_draw.c
//
int CG_HorizontalAlignForWidth( int x, Alignment alignment, int width );
int CG_VerticalAlignForHeight( int y, Alignment alignment, int height );
Vec2 WorldToScreen( Vec3 v );
Vec2 WorldToScreenClamped( Vec3 v, Vec2 screen_border, bool * clamped );

//
// cg_media.c
//
void CG_RegisterMediaSounds( void );
void CG_RegisterMediaModels( void );
void CG_RegisterMediaShaders( void );
void CG_RegisterFonts( void );

//
// cg_players.c
//
extern cvar_t *cg_hand;

void CG_ResetClientInfos( void );
void CG_LoadClientInfo( int client );
void CG_RegisterPlayerSounds( PlayerModelMetadata * metadata );
void CG_PlayerSound( int entnum, int entchannel, PlayerSound ps, float volume, float attn );

//
// cg_predict.c
//
extern cvar_t *cg_showMiss;

void CG_PredictedEvent( int entNum, int ev, int parm );
void CG_Predict_ChangeWeapon( int new_weapon );
void CG_PredictMovement( void );
void CG_CheckPredictionError( void );
void CG_BuildSolidList( void );
void CG_Trace( trace_t *t, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int ignore, int contentmask );
int CG_PointContents( const vec3_t point );
void CG_Predict_TouchTriggers( pmove_t *pm, vec3_t previous_origin );

//
// cg_screen.c
//
extern cvar_t *cg_showFPS;
extern cvar_t *cg_showAwards;

void CG_ScreenInit( void );
void CG_Draw2D( void );
void CG_DrawHUD( void );
void CG_DrawLoading( void );
void CG_CenterPrint( const char *str );

void CG_EscapeKey( void );
void CG_LoadStatusBar( void );

bool CG_LoadingItemName( const char *str );

void CG_DrawCrosshair();
void CG_ScreenCrosshairDamageUpdate( void );

void CG_DrawKeyState( int x, int y, int w, int h, const char *key );

int CG_ParseValue( const char **s );

void CG_DrawClock( int x, int y, Alignment alignment, const Font * font, float font_size, Vec4 color, bool border );
void CG_DrawPlayerNames( const Font * font, float font_size, Vec4 color, bool border );
void CG_DrawNet( int x, int y, int w, int h, Alignment alignment, Vec4 color );

void CG_ClearPointedNum( void );

void CG_InitDamageNumbers();
void CG_AddDamageNumber( entity_state_t * ent );
void CG_DrawDamageNumbers();

void CG_AddBombHudEntity( centity_t * cent );
void CG_DrawBombHUD();
void CG_ResetBombHUD();

//
// cg_hud.c
//
void CG_SC_ResetObituaries( void );
void CG_SC_Obituary( void );
void CG_ExecuteLayoutProgram( struct cg_layoutnode_s *rootnode );
void CG_ClearAwards( void );

//
// cg_damage_indicator.c
//
void CG_ResetDamageIndicator( void );
void CG_DamageIndicatorAdd( int damage, const vec3_t dir );

//
// cg_scoreboard.c
//
void CG_DrawScoreboard();
void CG_ScoresOn_f();
void CG_ScoresOff_f();
void SCR_UpdateScoreboardMessage( const char * string );
bool CG_ScoreboardShown();

//
// cg_main.c
//
extern cvar_t *developer;
extern cvar_t *cg_showClamp;

// wsw
extern cvar_t *cg_showObituaries;
extern cvar_t *cg_damageNumbers;
extern cvar_t *cg_volume_hitsound;    // hit sound volume
extern cvar_t *cg_autoaction_demo;
extern cvar_t *cg_autoaction_screenshot;
extern cvar_t *cg_autoaction_spectator;
extern cvar_t *cg_volume_players; // players sound volume
extern cvar_t *cg_volume_effects; // world sound volume
extern cvar_t *cg_volume_announcer; // announcer sounds volume
extern cvar_t *cg_volume_voicechats; //vsays volume
extern cvar_t *cg_projectileFireTrail;
extern cvar_t *cg_bloodTrail;
extern cvar_t *cg_showBloodTrail;
extern cvar_t *cg_projectileFireTrailAlpha;
extern cvar_t *cg_bloodTrailAlpha;

extern cvar_t *cg_cartoonEffects;

extern cvar_t *cg_explosionsRing;
extern cvar_t *cg_explosionsDust;
extern cvar_t *cg_outlineModels;
extern cvar_t *cg_outlineWorld;
extern cvar_t *cg_outlinePlayers;

extern cvar_t *cg_fov;
extern cvar_t *cg_zoomfov;
extern cvar_t *cg_particles;
extern cvar_t *cg_voiceChats;
extern cvar_t *cg_projectileAntilagOffset;
extern cvar_t *cg_chatFilter;

extern cvar_t *cg_allyColor;
extern cvar_t *cg_allyModel;
extern cvar_t *cg_allyForceModel;

extern cvar_t *cg_enemyColor;
extern cvar_t *cg_enemyModel;
extern cvar_t *cg_enemyForceModel;

#define CG_Malloc( size ) _Mem_AllocExt( cg_mempool, size, 16, 1, 0, 0, __FILE__, __LINE__ );
#define CG_Free( data ) Mem_Free( data )

void CG_Init( const char *serverName, unsigned int playerNum,
			  bool demoplaying, const char *demoName, bool pure, unsigned snapFrameTime );
void CG_Shutdown( void );

#ifndef _MSC_VER
void CG_Printf( const char *format, ... ) __attribute( ( format( printf, 1, 2 ) ) );
void CG_LocalPrint( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
void CG_Error( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) ) __attribute__( ( noreturn ) );
#else
void CG_Printf( _Printf_format_string_ const char *format, ... );
void CG_LocalPrint( _Printf_format_string_ const char *format, ... );
void CG_Error( _Printf_format_string_ const char *format, ... );
#endif

void CG_Reset( void );
void CG_Precache( void );
char *_CG_CopyString( const char *in, const char *filename, int fileline );
#define CG_CopyString( in ) _CG_CopyString( in, __FILE__, __LINE__ )

void CG_UseItem( const char *name );
void CG_RegisterCGameCommands( void );
void CG_UnregisterCGameCommands( void );
void CG_AddAward( const char *str );
void CG_OverrideWeapondef( int index, const char *cstring );

void CG_StartBackgroundTrack( void );

//
// cg_svcmds.c
//
void CG_ConfigString( int i, const char *s );
void CG_GameCommand( const char *command );
void CG_SC_AutoRecordAction( const char *action );

//
// cg_teams.c
//
void CG_RegisterForceModels();
void CG_PModelForCentity( centity_t *cent, PlayerModelMetadata **pmodelinfo );
RGB8 CG_TeamColor( int team );
Vec4 CG_TeamColorVec4( int team );
void CG_TeamColor( int team, vec4_t color );
void CG_TeamColorForEntity( int entNum, byte_vec4_t color );

//
// cg_view.c
//
enum {
	CAM_INEYES,
	CAM_THIRDPERSON,
	CAM_MODES
};

struct ChasecamState {
	int mode;
	bool key_pressed;
};

extern ChasecamState chaseCam;

extern cvar_t *cg_thirdPerson;
extern cvar_t *cg_thirdPersonAngle;
extern cvar_t *cg_thirdPersonRange;

void CG_ResetKickAngles( void );
void CG_ResetColorBlend( void );

void CG_AddEntityToScene( entity_t *ent );
void CG_StartKickAnglesEffect( vec3_t source, float knockback, float radius, int time );
void CG_StartFallKickEffect( int bounceTime );
void CG_ViewSmoothPredictedSteps( vec3_t vieworg );
float CG_ViewSmoothFallKick( void );
void CG_RenderView( int frameTime, int realFrameTime, int64_t monotonicTime, int64_t realTime, int64_t serverTime, unsigned extrapolationTime );
void CG_AddKickAngles( vec3_t viewangles );
bool CG_ChaseStep( int step );
bool CG_SwitchChaseCamMode( void );

//
// cg_lents.c
//

void CG_ClearLocalEntities( void );
void CG_AddLocalEntities( void );
void CG_FreeLocalEntities( void );

void CG_BulletExplosion( const vec3_t origin, const vec_t *dir, const trace_t *trace );
void CG_BubbleTrail( const vec3_t start, const vec3_t end, int dist );
void CG_ProjectileTrail( centity_t *cent );
void CG_NewBloodTrail( centity_t *cent );
void CG_BloodDamageEffect( const vec3_t origin, const vec3_t dir, int damage, int team );
void CG_SmallPileOfGibs( const vec3_t origin, int damage, const vec3_t initialVelocity, int team );
void CG_PlasmaExplosion( const vec3_t pos, const vec3_t dir, int team, float radius );
void CG_GrenadeExplosionMode( const vec3_t pos, const vec3_t dir, float radius, int team );
void CG_GenericExplosion( const vec3_t pos, const vec3_t dir, float radius );
void CG_RocketExplosionMode( const vec3_t pos, const vec3_t dir, float radius, int team );
void CG_EBBeam( Vec3 start, Vec3 end, int team );
void CG_EBImpact( const vec3_t pos, const vec3_t dir, int surfFlags, int team );
void CG_ImpactSmokePuff( const vec3_t origin, const vec3_t dir, float radius, float alpha, int time, int speed );
void CG_BladeImpact( const vec3_t pos, const vec3_t dir );
void CG_PModel_SpawnTeleportEffect( centity_t * cent, MatrixPalettes temp_pose );
void CG_SpawnSprite( const vec3_t origin, const vec3_t velocity, const vec3_t accel,
					 float radius, int time, int bounce, bool expandEffect, bool shrinkEffect,
					 float r, float g, float b, float a,
					 float light, float lr, float lg, float lb, const Material * material );
void CG_LaserGunImpact( const vec3_t pos, float radius, const vec3_t laser_dir, const vec4_t color );

void CG_Dash( const entity_state_t *state );
void CG_Explosion_Puff_2( const vec3_t pos, const vec3_t vel, int radius );
void CG_DustCircle( const vec3_t pos, const vec3_t dir, float radius, int count );
void CG_ExplosionsDust( const vec3_t pos, const vec3_t dir, float radius );

//
// cg_decals.c
//
extern cvar_t *cg_addDecals;

void CG_ClearDecals( void );
int CG_SpawnDecal( const vec3_t origin, const vec3_t dir, float orient, float radius,
				   float r, float g, float b, float a, float die, float fadetime, bool fadealpha, const Material * material );
void CG_AddDecals( void );

//
// cg_effects.c
//
void CG_ClearEffects( void );

void CG_AddLightToScene( vec3_t org, float radius, float r, float g, float b );
void CG_AddDlights( void );
void CG_AllocPlayerShadow( int entNum, const vec3_t origin, const vec3_t mins, const vec3_t maxs );
void CG_AddPlayerShadows( void );

void CG_ClearFragmentedDecals( void );
void CG_AddFragmentedDecal( vec3_t origin, vec3_t dir, float orient, float radius,
							float r, float g, float b, float a, const Material * material );

void CG_AddParticles( void );
void CG_ParticleEffect( const vec3_t org, const vec3_t dir, float r, float g, float b, int count );
void CG_ParticleEffect2( const vec3_t org, const vec3_t dir, float r, float g, float b, int count );
void CG_ParticleExplosionEffect( const vec3_t org, const vec3_t dir, float r, float g, float b, int count );
void CG_FlyEffect( centity_t *ent, const vec3_t origin );
void CG_EBIonsTrail( Vec3 start, Vec3 end, Vec4 color );
void CG_ImpactPuffParticles( const vec3_t org, const vec3_t dir, int count, float scale, float r, float g, float b, float a, const Material * material );
void CG_HighVelImpactPuffParticles( const vec3_t org, const vec3_t dir, int count, float scale, float r, float g, float b, float a, const Material * material );

void InitParticles();
void DrawParticles();

void DrawBeam( Vec3 start, Vec3 end, float width, Vec4 color, const Material * material );

void InitPersistentBeams();
void AddPersistentBeam( Vec3 start, Vec3 end, float width, Vec4 color, const Material * material, float duration, float fade_time );
void DrawPersistentBeams();

//
//	cg_vweap.c - client weapon
//
void CG_AddViewWeapon( cg_viewweapon_t *viewweapon );
void CG_CalcViewWeapon( cg_viewweapon_t *viewweapon );
void CG_ViewWeapon_StartAnimationEvent( int newAnim );
void CG_ViewWeapon_RefreshAnimation( cg_viewweapon_t *viewweapon );

//
// cg_events.c
//
extern cvar_t *cg_damage_indicator;
extern cvar_t *cg_damage_indicator_time;

void CG_FireEvents( bool early );
void CG_EntityEvent( entity_state_t *ent, int ev, int parm, bool predicted );
void CG_AddAnnouncerEvent( const SoundAsset *sound, bool queued );
void CG_ReleaseAnnouncerEvents( void );
void CG_ClearAnnouncerEvents( void );

// I don't know where to put these ones
void CG_WeaponBeamEffect( centity_t *cent );
void CG_LaserBeamEffect( centity_t *cent );


//
// cg_chat.cpp
//
void CG_InitChat();
void CG_ShutdownChat();
void CG_AddChat( const char * str );
void CG_DrawChat();

//
// cg_input.cpp
//

void CG_InitInput( void );
void CG_ShutdownInput( void );
void CG_InputFrame( int frameTime );
void CG_ClearInputState( void );
void CG_MouseMove( int mx, int my );
float CG_GetSensitivityScale( float sens, float zoomSens );
unsigned int CG_GetButtonBits( void );
void CG_AddViewAngles( vec3_t viewAngles );
void CG_AddMovement( vec3_t movement );

/*
* Returns angular movement vector (in euler angles) obtained from the input.
* Doesn't take flipping into account.
*/
void CG_GetAngularMovement( vec3_t movement );

/**
 * Gets up to two bound keys for a command.
 *
 * @param cmd      console command to get binds for
 * @param keys     output string
 * @param keysSize output string buffer size
 */
void CG_GetBoundKeysString( const char *cmd, char *keys, size_t keysSize );

/**
 * Checks a chat message for local player nick and flashes window on a match
 */
void CG_FlashChatHighlight( const unsigned int from, const char *text );
