#include "qcommon/qcommon.h"

enum LadderMovement : u8 {
	Ladder_Off,
	Ladder_On,
	Ladder_Fake,
};


struct pml_t {
	Vec3 origin;          // full float precision
	Vec3 velocity;        // full float precision

	Vec3 forward, right, up;
	// special handling for looking straight up or down
	float frametime;

	int groundsurfFlags;
	Plane groundplane;
	int groundcontents;

	Vec3 previous_origin;
	LadderMovement ladder;

	float forwardPush, sidePush, upPush;

	float maxPlayerSpeed;
	float maxCrouchedSpeed;

	float groundAccel;
	float airAccel;
	float waterAccel;
	float strafeBunnyAccel;

	float friction;

	void (*jumpCallback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState * );
	void (*specialCallback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState *, bool pressed );
};

#define PM_AIRCONTROL_BOUNCE_DELAY 200
#define PM_OVERBOUNCE       	   1.01f

#define SLIDEMOVE_PLANEINTERACT_EPSILON 0.05
#define SLIDEMOVEFLAG_BLOCKED       	( 1 << 1 )   // it was blocked at some point, doesn't mean it didn't slide along the blocking object
#define SLIDEMOVEFLAG_TRAPPED       	( 1 << 2 )
#define SLIDEMOVEFLAG_WALL_BLOCKED  	( 1 << 3 )


//shared
void PM_ClearDash( SyncPlayerState * ps );
float Normalize2D( Vec3 * v );
void PlayerTouchWall( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, int nbTestDir, float maxZnormal, Vec3 * normal );

void StaminaUse( SyncPlayerState * ps, s16 use );
void StaminaRecover( SyncPlayerState * ps, s16 recover );

float JumpVelocity( pmove_t * pm, float vel );

void PM_InitPerk( pmove_t * pm, pml_t * pml,
				float speed, float sidespeed, s16 stamina_max,
				void (*jumpCallback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState * ),
				void (*specialCallback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState *, bool pressed ) );

void PM_Jump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, float jumpspeed );
void PM_Dash( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, Vec3 dashdir, float dash_speed, float dash_upspeed );

//pmove_ files
void PM_NinjaInit( pmove_t * pm, pml_t * pml );
void PM_HooliganInit( pmove_t * pm, pml_t * pml );
void PM_MidgetInit( pmove_t * pm, pml_t * pml );
void PM_JetpackInit( pmove_t * pm, pml_t * pml );
void PM_BoomerInit( pmove_t * pm, pml_t * pml );