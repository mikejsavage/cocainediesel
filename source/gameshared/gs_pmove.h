#include "qcommon/qcommon.h"
#include "gameshared/gs_weapons.h"

#define SPEEDKEY    500.0f

// pmove->pm_features
#define PMFEAT_CROUCH           ( 1 << 0 )
#define PMFEAT_WALK             ( 1 << 1 )
#define PMFEAT_JUMP             ( 1 << 2 )
#define PMFEAT_SPECIAL          ( 1 << 3 )
#define PMFEAT_SCOPE            ( 1 << 4 )
#define PMFEAT_GHOSTMOVE        ( 1 << 5 )
#define PMFEAT_WEAPONSWITCH     ( 1 << 6 )
#define PMFEAT_TEAMGHOST        ( 1 << 7 )

#define PMFEAT_ALL              ( 0xFFFF )
#define PMFEAT_DEFAULT          ( PMFEAT_ALL & ~PMFEAT_GHOSTMOVE & ~PMFEAT_TEAMGHOST )


#define PM_AIRCONTROL_BOUNCE_DELAY 200
#define PM_OVERBOUNCE       1.01f

// movement parameters

#define CROUCHTIME 100
#define DEFAULT_LADDERSPEED 300.0f

#define MIN_JUMPSOUND_COOLDOWN 200

enum JumpType {
	JumpType_Jump,
	JumpType_Dash,
	JumpType_Leap,
};

struct pml_t {
	Vec3 origin;          // full float precision
	Vec3 velocity;        // full float precision

	Vec3 forward, right, up;
	Vec3 flatforward;     // normalized forward without z component, saved here because it needs
	// special handling for looking straight up or down
	float frametime;

	int groundsurfFlags;
	cplane_t groundplane;
	int groundcontents;

	Vec3 previous_origin;
	bool ladder;

	float forwardPush, sidePush, upPush;

	float maxPlayerSpeed;
	float maxWalkSpeed;
	float maxCrouchedSpeed;
	
	float jumpUp;
	float jumpUpWater;
	float jumpForward;
	int jumpCooldown;
	int jumpCharge; //not implemented

	float walljumpUp;
	float walljumpBounce;
	int walljumpCooldown;

	JumpType jumpType;
	MovementType movement_type;
};


struct MovementDef {
	const char * short_name;
	const char * desc;
	JumpType jumpType;

	float airSpeed;
	float walkSpeed;

	float jumpUp;
	float jumpForward;
	int jumpCooldown;
	int jumpCharge;

	float walljumpUp;
	float walljumpBounce;
	int walljumpCooldown;
};