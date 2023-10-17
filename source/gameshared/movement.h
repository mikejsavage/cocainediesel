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

	Vec3 groundplane;

	Vec3 previous_origin;
	LadderMovement ladder;

	float forwardPush, sidePush;

	float maxSpeed;
	float maxAirSpeed;

	float groundAccel;
	float airAccel;
	float strafeBunnyAccel;

	float groundFriction;

	void (*ability1Callback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState *, bool );
	void (*ability2Callback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState *, bool );
};

constexpr float PM_OVERBOUNCE = 1.01f;

// shared
float Normalize2D( Vec3 * v );
Optional< Vec3 > PlayerTouchWall( const pmove_t * pm, const pml_t * pml, const gs_state_t * pmove_gs, bool z, SolidBits ignoreFlags );

bool StaminaAvailable( SyncPlayerState * ps, pml_t * pml, float need );
bool StaminaAvailableImmediate( SyncPlayerState * ps, float need );
void StaminaUse( SyncPlayerState * ps, pml_t * pml, float use );
void StaminaUseImmediate( SyncPlayerState * ps, float use );
void StaminaRecover( SyncPlayerState * ps, pml_t * pml, float recover );

void PM_InitPerk( pmove_t * pm, pml_t * pml, PerkType perk,
				void (*ability1Callback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState *, bool ),
				void (*ability2Callback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState *, bool ) );

void Jump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, float jumpspeed, bool addvel );
void Dash( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, Vec3 dashdir, float dash_speed, float dash_upspeed );
bool Walljump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, float jumpupspeed, float dashupspeed, float dashspeed, float wjupspeed, float wjbouncefactor );



// pmove_ files
void PM_NinjaInit( pmove_t * pm, pml_t * pml );
void PM_HooliganInit( pmove_t * pm, pml_t * pml );
void PM_MidgetInit( pmove_t * pm, pml_t * pml );
void PM_WheelInit( pmove_t * pm, pml_t * pml );
void PM_JetpackInit( pmove_t * pm, pml_t * pml );
void PM_BoomerInit( pmove_t * pm, pml_t * pml );
