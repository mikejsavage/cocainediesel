#include "qcommon/qcommon.h"


struct pml_t {
	Vec3 origin;          // full float precision
	Vec3 velocity;        // full float precision

	Vec3 forward, right, up;
	Vec3 flatforward;     // normalized forward without z component, saved here because it needs
	// special handling for looking straight up or down
	float frametime;

	int groundsurfFlags;
	Plane groundplane;
	int groundcontents;

	Vec3 previous_origin;
	bool ladder;

	float forwardPush, sidePush, upPush;

	float maxPlayerSpeed;
	float maxCrouchedSpeed;
	float jumpPlayerSpeed;
	float jumpPlayerSpeedWater;
	float dashPlayerSpeed;

	void (*jumpCallback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState * );
	void (*specialCallback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState * );
};

//to be changed
#define SPEEDKEY    500.0f

#define PM_DASHJUMP_TIMEDELAY 200 // delay in milliseconds
#define PM_WALLJUMP_TIMEDELAY   1300
#define PM_AIRCONTROL_BOUNCE_DELAY 200
#define PM_OVERBOUNCE       1.01f

#define SLIDEMOVE_PLANEINTERACT_EPSILON 0.05
#define SLIDEMOVEFLAG_WALL_BLOCKED  8
#define SLIDEMOVEFLAG_TRAPPED       4
#define SLIDEMOVEFLAG_BLOCKED       2   // it was blocked at some point, doesn't mean it didn't slide along the blocking object

#define DEFAULT_CROUCHEDSPEED 160.0f
#define DEFAULT_LADDERSPEED 300.0f



//shared
void PM_ClearDash( SyncPlayerState * ps );
void PM_ClearWallJump( SyncPlayerState * ps );
float Normalize2D( Vec3 * v );
void PlayerTouchWall( pmove_t *, pml_t * pml, const gs_state_t * pmove_gs, int nbTestDir, float maxZnormal, Vec3 * normal );



//default
void PM_DefaultJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps );
void PM_DefaultSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps );