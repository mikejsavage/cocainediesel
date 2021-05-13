const MovementDef movements[] = {
	{ 	/* short name          */ NULL, //generic
		/* description         */ NULL,
		/* jump type           */ JumpType_Jump,

		/* air speed           */ 320.0f,
		/* walk speed          */ 160.0f,

		/* jump up speed       */ 270.0f,
		/* jump forward speed  */ 0.0f,
		/* jump cooldown       */ 0,
		/* jump charge time    */ 0,

		/* walljump up speed   */ 0.0f,
		/* walljump side speed */ 0.0f,
		/* walljump cooldown   */ 0, //Setting this to 0 will disable the walljump
	},

	{
		/* short name          */ "dash",
		/* description         */ "Fast dashes with low walljumps",
		/* jump type           */ JumpType_Dash,

		/* air speed           */ 320.0f,
		/* walk speed          */ 160.0f,

		/* jump up speed       */ 164.0f,
		/* jump forward speed  */ 550.0f,
		/* jump cooldown       */ 0,
		/* jump charge time    */ 0,

		/* walljump up speed   */ 300.0f,
		/* walljump side speed */ 0.5f,
		/* walljump cooldown   */ 1300,
	},

	{
		/* short name          */ "leap",
		/* description         */ "Big jumps with good walljumps",
		/* jump type           */ JumpType_Leap,

		/* air speed           */ 320.0f,
		/* walk speed          */ 160.0f,

		/* jump up speed       */ 275.0f,
		/* jump forward speed  */ 350.0f,
		/* jump cooldown       */ 0,
		/* jump charge time    */ 0,

		/* walljump up speed   */ 350.0f,
		/* walljump side speed */ 0.3f,
		/* walljump cooldown   */ 400,
	},
};