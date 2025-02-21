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

#include "qcommon/types.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"
#include "gameshared/gs_weapons.h"
#include "cgame/ref.h"

#include "client/client.h"
#include "cgame/cg_public.h"
#include "cgame/cg_dynamics.h"
#include "cgame/cg_particles.h"
#include "cgame/cg_sprays.h"

#include "client/audio/types.h"
#include "client/renderer/types.h"

constexpr float FOV = 107.9f; // chosen to upset everyone equally

enum {
	LOCALEFFECT_VSAY_TIMEOUT,
	LOCALEFFECT_LASERBEAM,
	LOCALEFFECT_JETPACK,

	LOCALEFFECT_COUNT
};

struct centity_t {
	SyncEntityState current;
	SyncEntityState prev;        // will always be valid, but might just be a copy of current

	int serverFrame;            // if not current, this ent isn't in the frame
	int64_t fly_stoptime;

	int64_t respawnTime;

	InterpolatedEntity interpolated;
	unsigned int type;
	unsigned int effects;

	Vec3 velocity;

	bool canExtrapolate;
	bool canExtrapolatePrev;
	Vec3 prevVelocity;
	int microSmooth;
	Vec3 microSmoothOrigin;
	Vec3 microSmoothOrigin2;

	// effects
	PlayingSFXHandle sound;
	u64 last_noammo_sound;
	Vec3 trailOrigin;         // for particle trails

	// local effects from events timers
	int64_t localEffects[LOCALEFFECT_COUNT];

	// attached laser beam
	Vec3 laserOrigin;
	Vec3 laserPoint;
	Vec3 laserOriginOld;
	Vec3 laserPointOld;
	PlayingSFXHandle lg_hum_sound;
	PlayingSFXHandle lg_beam_sound;
	PlayingSFXHandle lg_tip_sound;

	bool jetpack_boost;
	PlayingSFXHandle jetpack_sound;

	PlayingSFXHandle playing_idle_sound;
	PlayingSFXHandle playing_body_sound;
	PlayingSFXHandle playing_vsay;
	PlayingSFXHandle playing_reload;

	bool linearProjectileCanDraw;
	Vec3 linearProjectileViewerSource;
	Vec3 linearProjectileViewerVelocity;

	// used for client side animation of player models
	int lastVelocitiesFrames[4];
	Vec4 lastVelocities[4];
	bool jumpedLeft;
	Vec3 animVelocity;
	float yawVelocity;
};

#include "cgame/cg_pmodels.h"

struct cgs_media_t {
	StringHash shaderWeaponIcon[ Weapon_Count ];
	StringHash shaderGadgetIcon[ Gadget_Count ];
	StringHash shaderPerkIcon[ Perk_Count ];
};

#define PREDICTED_STEP_TIME 150 // stairs smoothing time

struct cg_viewdef_t {
	int type;
	int POVent;
	bool thirdperson;
	bool playerPrediction;
	bool drawWeapon;
	bool draw2D;
	float fov_x, fov_y;
	float fracDistFOV;
	Vec3 origin;
	EulerDegrees3 angles;
	mat3_t axis;
	Vec3 velocity;
};

#include "cgame/cg_democams.h"

// this is not exactly "static" but still...
struct cg_static_t {
	const char *demoName;
	unsigned int playerNum;

	// fonts
	int fontSystemMediumSize;

	float textSizeTiny;
	float textSizeSmall;
	float textSizeMedium;
	float textSizeBig;

	const Font * fontNormal;
	const Font * fontNormalBold;
	const Font * fontItalic;
	const Font * fontBoldItalic;

	cgs_media_t media;

	bool demoPlaying;
	unsigned snapFrameTime;
	unsigned extrapolationTime;
};

struct cg_state_t {
	int frameCount;

	snapshot_t frame, oldFrame;
	bool fireEvents;

	Vec3 predictedOrigins[CMD_BACKUP];              // for debug comparing against server

	float predictedStep;                // for stair up smoothing
	int64_t predictedStepTime;

	int64_t predictingTimeStamp;
	int64_t predictedEventTimes[PREDICTABLE_EVENTS_MAX];
	Vec3 predictionError;
	SyncPlayerState predictedPlayerState;     // current in use, predicted or interpolated
	int predictedGroundEntity;

	// prediction optimization (don't run all ucmds in not needed)
	int64_t predictFrom;
	SyncEntityState predictFromEntityState;
	SyncPlayerState predictFromPlayerState;

	float lerpfrac;                     // between oldframe and frame
	float xerpTime;
	float oldXerpTime;
	float xerpSmoothFrac;

	bool showScoreboard;            // demos and multipov

	unsigned int multiviewPlayerNum;       // for multipov chasing, takes effect on next snap

	int pointedNum;
	int64_t pointRemoveTime;
	int pointedHealth;

	float xyspeed;

	bool recoiling;
	EulerDegrees2 recoil_velocity;
	EulerDegrees2 recoil_initial_angles;

	float damage_effect;

	float oldBobTime;
	int bobCycle;                   // odd cycles are right foot going forward
	float bobFracSin;               // sin(bobfrac*M_PI)

	int64_t fallEffectTime;
	int64_t fallEffectRebounceTime;

	cg_viewweapon_t weapon;
	cg_viewdef_t view;
};

extern cg_static_t cgs;
extern cg_state_t cg;

#define ISVIEWERENTITY( entNum )  ( cg.predictedPlayerState.POVnum > 0 && (int)cg.predictedPlayerState.POVnum == ( entNum ) && cg.view.type == ViewType_Player )

#define ISREALSPECTATOR()       ( cg.frame.playerState.real_team == Team_None )

extern centity_t cg_entities[MAX_EDICTS];

//
// cg_ents.c
//
bool CG_NewFrameSnap( snapshot_t *frame, snapshot_t *lerpframe );

void CG_SoundEntityNewState( centity_t *cent );
void DrawEntities();
void CG_LerpEntities();
void CG_LerpGenericEnt( centity_t *cent );

//
// cg_draw.c
//
Vec2 WorldToScreen( Vec3 v );
Vec2 WorldToScreenClamped( Vec3 v, Vec2 screen_border, bool * clamped );

//
// cg_media.c
//
void CG_RegisterMedia();

//
// cg_players.c
//
float CG_PlayerPitch( int entnum );
void CG_PlayerSound( int entnum, PlayerSound ps, bool stop_current );

//
// cg_predict.c
//
extern Cvar *cg_showMiss;

void CG_PredictedEvent( int entNum, int ev, u64 parm );
void CG_PredictedFireWeapon( int entNum, u64 parm );
void CG_PredictedAltFireWeapon( int entNum, u64 parm );
void CG_PredictedUseGadget( int entNum, GadgetType gadget, u64 parm, bool dead );
void CG_PredictMovement();
void CG_CheckPredictionError();
void CG_BuildSolidList( const snapshot_t * frame );
trace_t CG_Trace( Vec3 start, MinMax3 bounds, Vec3 end, int ignore, SolidBits solid_mask );
void CG_Predict_TouchTriggers( const pmove_t * pm, Vec3 previous_origin );

//
// cg_screen.c
//
extern Cvar *cg_showFPS;

void CG_ScreenInit();
void CG_DrawScope();
void CG_Draw2D();
void CG_CenterPrint( Span< const char > str );

void CG_EscapeKey();

void CG_DrawCrosshair( int x, int y );
void CG_ScreenCrosshairDamageUpdate();
void CG_ScreenCrosshairShootUpdate( u16 refire_time );

void CG_DrawKeyState( int x, int y, int w, int h, const char *key );

void CG_DrawPlayerNames( const Font * font, float font_size, Vec4 color );

void CG_InitDamageNumbers();
void CG_AddDamageNumber( SyncEntityState * ent, u64 parm );
void CG_DrawDamageNumbers( float obi_size, float dmg_size );

void CG_AddBombIndicator( const centity_t * cent );
void CG_AddBombSiteIndicator( const centity_t * cent );
void CG_DrawBombHUD( int name_size, int goal_size, int bomb_msg_size );
void CG_ResetBombHUD();

void AddDamageEffect( float x = 0.0f );

//
// cg_hud.c
//
void CG_InitHUD();
void CG_ShutdownHUD();
void CG_DrawScoreboard();
void CG_SC_ResetObituaries();
void CG_SC_Obituary( const Tokenized & args );
void CG_DrawHUD();

//
// cg_main.c
//
extern Cvar *cg_showClamp;
extern Cvar *cg_mask;

// wsw
extern Cvar *cg_projectileAntilagOffset;

extern Cvar *cg_showServerDebugPrints;

void CG_Init( unsigned int playerNum, int max_clients, bool demoplaying, const char *demoName, unsigned snapFrameTime );
void CG_Shutdown();

void CG_LocalPrint( Span< const char > str );

void CG_Reset();
void CG_Precache();

void CG_RegisterCGameCommands();
void CG_UnregisterCGameCommands();

const char * PlayerName( int i );

//
// cg_svcmds.c
//
void CG_GameCommand( const char *command );

//
// cg_teams.c
//
RGB8 CG_TeamColor( Team team );
RGB8 AllyColor();
RGB8 EnemyColor();
Vec4 CG_TeamColorVec4( Team team );
Vec4 AllyColorVec4();
Vec4 EnemyColorVec4();

//
// cg_view.c
//
struct ChasecamState {
	bool key_pressed;
};

extern ChasecamState chaseCam;

extern Cvar *cg_thirdPerson;
extern Cvar *cg_thirdPersonRange;

void CG_StartFallKickEffect( int bounceTime );
void CG_ViewSmoothPredictedSteps( Vec3 * vieworg );
float CG_ViewSmoothFallKick();
float CG_CalcViewFov();
void CG_RenderView( unsigned extrapolationTime );
bool CG_ChaseStep( int step );

float WidescreenFov( float fov );
float CalcHorizontalFov( const char * caller, float fov_y, float width, float height );

void MaybeResetShadertoyTime( bool respawned );

//
// cg_lents.c
//

void InitGibs();
void SpawnGibs( Vec3 origin, Vec3 velocity, int damage, Vec4 team_color );
void DrawGibs();

//
// cg_effects.c
//
void DrawBeam( Vec3 start, Vec3 end, float width, Vec4 color, StringHash material );

void InitPersistentBeams();
void AddPersistentBeam( Vec3 start, Vec3 end, float width, Vec4 color, StringHash material, Time duration, Time fade_time );
void DrawPersistentBeams();

//
// cg_trails.cpp
//
void DrawTrail( u64 unique_id, Vec3 point, float width, Vec4 color, StringHash material, Time duration );
void InitTrails();
void DrawTrails();

//
//	cg_vweap.c - client weapon
//
void CG_AddViewWeapon( cg_viewweapon_t *viewweapon );
void CG_CalcViewWeapon( cg_viewweapon_t *viewweapon );
void CG_ViewWeapon_AddAnimation( int ent_num, StringHash anim );

void CG_AddRecoil( WeaponType weapon );
void CG_Recoil( WeaponType weapon );

//
// cg_events.c
//
void CG_FireEvents( bool early );
void CG_EntityEvent( SyncEntityState *ent, int ev, u64 parm, bool predicted );
void CG_AddAnnouncerEvent( StringHash sound, bool queued );
void CG_ReleaseAnnouncerEvents();
void CG_ClearAnnouncerEvents();

void ResetAnnouncerSpeakers();
void AddAnnouncerSpeaker( const centity_t * cent );

// I don't know where to put these ones
void CG_JetpackEffect( centity_t * cent );
void CG_LaserBeamEffect( centity_t *cent );


//
// cg_chat.cpp
//
void CG_InitChat();
void CG_ShutdownChat();
void CG_AddChat( Span< const char > str );
void CG_DrawChat();

//
// cg_input.cpp
//

void CG_InitInput();
void CG_ShutdownInput();
void CG_ClearInputState();
void CG_MouseMove( Vec2 m );
UserCommandButton CG_GetButtonBits();
UserCommandButton CG_GetButtonDownEdges();
EulerDegrees2 CG_GetDeltaViewAngles();
Vec2 CG_GetMovement();
