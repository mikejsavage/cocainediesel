/*
Copyright (C) 2006 Pekka Lampila ("Medar"), Damien Deville ("Pb")
and German Garcia Fernandez ("Jal") for Chasseur de bots association.


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

#include "qcommon/base.h"
#include "cgame/cg_local.h"

static int layout_cursor_x = 400;
static int layout_cursor_y = 300;
static int layout_cursor_width = 100;
static int layout_cursor_height = 100;
static Alignment layout_cursor_alignment = Alignment_LeftTop;
static Vec4 layout_cursor_color = vec4_white;

enum FontStyle {
	FontStyle_Normal,
	FontStyle_Bold,
	FontStyle_Italic,
	FontStyle_BoldItalic,
};

static float layout_cursor_font_size;
static FontStyle layout_cursor_font_style;
static bool layout_cursor_font_border;

static const Font * GetHUDFont() {
	switch( layout_cursor_font_style ) {
		case FontStyle_Normal: return cgs.fontMontserrat;
		case FontStyle_Bold: return cgs.fontMontserratBold;
		case FontStyle_Italic: return cgs.fontMontserratItalic;
		case FontStyle_BoldItalic: return cgs.fontMontserratBoldItalic;
	}
	return NULL;
}

//=============================================================================

typedef struct
{
	const char *name;
	int value;
} constant_numeric_t;

static const constant_numeric_t cg_numeric_constants[] = {
	{ "NOTSET", STAT_NOTSET },

	// teams
	{ "TEAM_SPECTATOR", TEAM_SPECTATOR },
	{ "TEAM_PLAYERS", TEAM_PLAYERS },
	{ "TEAM_ALPHA", TEAM_ALPHA },
	{ "TEAM_BETA", TEAM_BETA },
	{ "TEAM_ALLY", TEAM_ALLY },
	{ "TEAM_ENEMY", TEAM_ENEMY },

	{ "WIDTH", 800 },
	{ "HEIGHT", 600 },

	// match states
	{ "MATCH_STATE_NONE", MATCH_STATE_NONE },
	{ "MATCH_STATE_WARMUP", MATCH_STATE_WARMUP },
	{ "MATCH_STATE_COUNTDOWN", MATCH_STATE_COUNTDOWN },
	{ "MATCH_STATE_PLAYTIME", MATCH_STATE_PLAYTIME },
	{ "MATCH_STATE_POSTMATCH", MATCH_STATE_POSTMATCH },
	{ "MATCH_STATE_WAITEXIT", MATCH_STATE_WAITEXIT },

	// weapon
	{ "WEAP_GUNBLADE", WEAP_GUNBLADE },
	{ "WEAP_MACHINEGUN", WEAP_MACHINEGUN },
	{ "WEAP_RIOTGUN", WEAP_RIOTGUN },
	{ "WEAP_GRENADELAUNCHER", WEAP_GRENADELAUNCHER },
	{ "WEAP_ROCKETLAUNCHER", WEAP_ROCKETLAUNCHER },
	{ "WEAP_PLASMAGUN", WEAP_PLASMAGUN },
	{ "WEAP_LASERGUN", WEAP_LASERGUN },
	{ "WEAP_ELECTROBOLT", WEAP_ELECTROBOLT },

	{ "CS_CALLVOTE", CS_CALLVOTE },
	{ "CS_CALLVOTE_REQUIRED_VOTES", CS_CALLVOTE_REQUIRED_VOTES },
	{ "CS_CALLVOTE_YES_VOTES", CS_CALLVOTE_YES_VOTES },
	{ "CS_CALLVOTE_NO_VOTES", CS_CALLVOTE_NO_VOTES },

	{ "BombProgress_Nothing", BombProgress_Nothing },
	{ "BombProgress_Planting", BombProgress_Planting },
	{ "BombProgress_Defusing", BombProgress_Defusing },

	{ "RoundType_Normal", RoundType_Normal },
	{ "RoundType_MatchPoint", RoundType_MatchPoint },
	{ "RoundType_Overtime", RoundType_Overtime },
	{ "RoundType_OvertimeMatchPoint", RoundType_OvertimeMatchPoint },

	{ NULL, 0 }
};

//=============================================================================

static int CG_GetStatValue( const void *parameter ) {
	assert( (intptr_t)parameter >= 0 && (intptr_t)parameter < MAX_STATS );

	return cg.predictedPlayerState.stats[(intptr_t)parameter];
}

static int CG_GetLayoutStatFlag( const void *parameter ) {
	return ( cg.predictedPlayerState.stats[STAT_LAYOUTS] & (intptr_t)parameter ) ? 1 : 0;
}

static int CG_GetPOVnum( const void *parameter ) {
	return ( cg.predictedPlayerState.POVnum != cgs.playerNum + 1 ) ? cg.predictedPlayerState.POVnum : STAT_NOTSET;
}

static float _getspeed( void ) {
	vec3_t hvel;

	VectorSet( hvel, cg.predictedPlayerState.pmove.velocity[0], cg.predictedPlayerState.pmove.velocity[1], 0 );

	return VectorLength( hvel );
}

static int CG_GetSpeed( const void *parameter ) {
	return (int)_getspeed();
}

static int CG_GetSpeedVertical( const void *parameter ) {
	return cg.predictedPlayerState.pmove.velocity[2];
}

static int CG_GetFPS( const void *parameter ) {
#define FPSSAMPLESCOUNT 32
#define FPSSAMPLESMASK ( FPSSAMPLESCOUNT - 1 )
	static int frameTimes[FPSSAMPLESCOUNT];

	if( cg_showFPS->modified ) {
		memset( frameTimes, 0, sizeof( frameTimes ) );
		cg_showFPS->modified = false;
	}

	frameTimes[cg.frameCount & FPSSAMPLESMASK] = cls.realFrameTime;

	float average = 0.0f;
	for( size_t i = 0; i < ARRAY_COUNT( frameTimes ); i++ ) {
		average += frameTimes[( cg.frameCount - i ) & FPSSAMPLESMASK];
	}
	average /= FPSSAMPLESCOUNT;
	return int( 1000.0f / average + 0.5f );
;
}

static int CG_GetMatchState( const void *parameter ) {
	return GS_MatchState( &client_gs );
}

static int CG_GetMatchDuration( const void *parameter ) {
	return GS_MatchDuration( &client_gs );
}

static int CG_Paused( const void *parameter ) {
	return GS_MatchPaused( &client_gs );
}

static int CG_GetZoom( const void *parameter ) {
	return ( !cg.view.thirdperson && ( cg.predictedPlayerState.pmove.stats[PM_STAT_ZOOMTIME] != 0 ) );
}

static int CG_GetVidWidth( const void *parameter ) {
	return frame_static.viewport_width;
}

static int CG_GetVidHeight( const void *parameter ) {
	return frame_static.viewport_height;
}

static int CG_GetCvar( const void *parameter ) {
	return Cvar_Value( (const char *)parameter );
}

/**
 * Returns whether the weapon should be displayed in the weapon list on the HUD
 * (if the player either has the weapon ammo for it).
 *
 * @param weapon weapon item ID
 * @return whether to display the weapon
 */
static bool CG_IsWeaponInList( int weapon ) {
	bool hasWeapon = cg.predictedPlayerState.inventory[weapon] != 0;
	bool hasAmmo = cg.predictedPlayerState.inventory[weapon - WEAP_GUNBLADE + AMMO_GUNBLADE];

	if( weapon == WEAP_GUNBLADE ) { // gunblade always has 1 ammo when it's strong, but the player doesn't necessarily have it
		return hasWeapon;
	}

	return hasWeapon || hasAmmo;
}

static int CG_IsDemoPlaying( const void *parameter ) {
	return cgs.demoPlaying ? 1 : 0;
}

static int CG_IsActiveCallvote( const void * parameter ) {
	return strcmp( cgs.configStrings[ CS_CALLVOTE ], "" ) != 0;
}

static int CG_DownloadInProgress( const void *parameter ) {
	const char *str;

	str = Cvar_String( "cl_download_name" );
	if( str[0] ) {
		return 1;
	}
	return 0;
}

// ch : backport some of racesow hud elements
/*********************************************************************************
lm: edit for race mod,
    adds bunch of vars to the hud.

*********************************************************************************/

//lm: for readability
enum race_index {
	mouse_x,
	mouse_y,
	jumpspeed,
	move_an,
	diff_an,
	strafe_an,
	max_index
};

static int CG_GetScoreboardShown( const void *parameter ) {
	return CG_ScoreboardShown() ? 1 : 0;
}

typedef struct
{
	const char *name;
	int ( *func )( const void *parameter );
	const void *parameter;
} reference_numeric_t;

static const reference_numeric_t cg_numeric_references[] = {
	// stats
	{ "HEALTH", CG_GetStatValue, (void *)STAT_HEALTH },
	{ "WEAPON_ITEM", CG_GetStatValue, (void *)STAT_WEAPON },
	{ "PENDING_WEAPON", CG_GetStatValue, (void *)STAT_PENDING_WEAPON },

	{ "READY", CG_GetLayoutStatFlag, (void *)STAT_LAYOUT_READY },

	{ "SCORE", CG_GetStatValue, (void *)STAT_SCORE },
	{ "TEAM", CG_GetStatValue, (void *)STAT_TEAM },
	{ "RESPAWN_TIME", CG_GetStatValue, (void *)STAT_NEXT_RESPAWN },

	{ "POINTED_PLAYER", CG_GetStatValue, (void *)STAT_POINTED_PLAYER },
	{ "POINTED_TEAMPLAYER", CG_GetStatValue, (void *)STAT_POINTED_TEAMPLAYER },

	{ "TEAM_ALPHA_SCORE", CG_GetStatValue, (void *)STAT_TEAM_ALPHA_SCORE },
	{ "TEAM_BETA_SCORE", CG_GetStatValue, (void *)STAT_TEAM_BETA_SCORE },

	{ "PROGRESS", CG_GetStatValue, (void *)STAT_PROGRESS },
	{ "PROGRESS_TYPE", CG_GetStatValue, (void *)STAT_PROGRESS_TYPE },

	{ "ROUND_TYPE", CG_GetStatValue, (void *)STAT_ROUND_TYPE },

	{ "CARRYING_BOMB", CG_GetStatValue, (void *)STAT_CARRYING_BOMB },
	{ "CAN_PLANT_BOMB", CG_GetStatValue, (void *)STAT_CAN_PLANT_BOMB },
	{ "CAN_CHANGE_LOADOUT", CG_GetStatValue, (void *)STAT_CAN_CHANGE_LOADOUT },

	{ "ALPHA_PLAYERS_ALIVE", CG_GetStatValue, (void *)STAT_ALPHA_PLAYERS_ALIVE },
	{ "ALPHA_PLAYERS_TOTAL", CG_GetStatValue, (void *)STAT_ALPHA_PLAYERS_TOTAL },
	{ "BETA_PLAYERS_ALIVE", CG_GetStatValue, (void *)STAT_BETA_PLAYERS_ALIVE },
	{ "BETA_PLAYERS_TOTAL", CG_GetStatValue, (void *)STAT_BETA_PLAYERS_TOTAL },

	// other
	{ "CHASING", CG_GetPOVnum, NULL },
	{ "SPEED", CG_GetSpeed, NULL },
	{ "SPEED_VERTICAL", CG_GetSpeedVertical, NULL },
	{ "FPS", CG_GetFPS, NULL },
	{ "MATCH_STATE", CG_GetMatchState, NULL },
	{ "MATCH_DURATION", CG_GetMatchDuration, NULL },
	{ "PAUSED", CG_Paused, NULL },
	{ "ZOOM", CG_GetZoom, NULL },
	{ "VIDWIDTH", CG_GetVidWidth, NULL },
	{ "VIDHEIGHT", CG_GetVidHeight, NULL },
	{ "SCOREBOARD", CG_GetScoreboardShown, NULL },
	{ "DEMOPLAYING", CG_IsDemoPlaying, NULL },
	{ "CALLVOTE", CG_IsActiveCallvote, NULL },

	// cvars
	{ "SHOW_FPS", CG_GetCvar, "cg_showFPS" },
	{ "SHOW_OBITUARIES", CG_GetCvar, "cg_showObituaries" },
	{ "SHOW_POINTED_PLAYER", CG_GetCvar, "cg_showPointedPlayer" },
	{ "SHOW_PRESSED_KEYS", CG_GetCvar, "cg_showPressedKeys" },
	{ "SHOW_SPEED", CG_GetCvar, "cg_showSpeed" },
	{ "SHOW_AWARDS", CG_GetCvar, "cg_showAwards" },

	{ "DOWNLOAD_IN_PROGRESS", CG_DownloadInProgress, NULL },
	{ "DOWNLOAD_PERCENT", CG_GetCvar, "cl_download_percent" },

	{ NULL, NULL, NULL }
};

//=============================================================================

#define MAX_OBITUARIES 32

typedef enum { OBITUARY_NONE, OBITUARY_NORMAL, OBITUARY_SUICIDE, OBITUARY_ACCIDENT } obituary_type_t;

typedef struct obituary_s {
	obituary_type_t type;
	int64_t time;
	char victim[MAX_INFO_VALUE];
	int victim_team;
	char attacker[MAX_INFO_VALUE];
	int attacker_team;
	int mod;
} obituary_t;

static obituary_t cg_obituaries[MAX_OBITUARIES];
static int cg_obituaries_current = -1;

/*
* CG_SC_ResetObituaries
*/
void CG_SC_ResetObituaries( void ) {
	memset( cg_obituaries, 0, sizeof( cg_obituaries ) );
	cg_obituaries_current = -1;
}

static const char * obituaries[] = {
	"2.3'ED",
	"ABOLISHED",
	"ABUSED",
	"AIRED OUT",
	"ANALYZED",
	"ANNIHILATED",
	"ATOMIZED",
	"AXED",
	"BAKED",
	"BALLED",
	"BANGED",
	"BANKED",
	"BANNED",
	"BARFED ON",
	"BASHED",
	"BATTERED",
	"BBQED",
	"BEAMED",
	"BELITTLED",
	"BENT",
	"BENT OVER",
	"BIGGED ON",
	"BINNED",
	"BLASTED",
	"BLAZED",
	"BLEMISHED",
	"BLESSED",
	"BODYBAGGED",
	"BOMBED",
	"BONKED",
	"BOUGHT",
	"BROKE",
	"BROKE UP WITH",
	"BROWN BAGGED",
	"BURIED",
	"BUMMED",
	"BUMPED",
	"BURNED",
	"BUTTERED",
	"CALCULATED",
	"CAKED",
	"CANCELED",
	"CANNED",
	"CAPPED",
	"CARAMELIZED",
	"CARBONATED",
	"CASHED OUT",
	"CHARRED",
	"CHEATED ON",
	"CHECKED",
	"CHECKMATED",
	"CHEESED",
	"CLAPPED",
	"CLUBBED",
	"COMBED",
	"COMBO-ED",
	"COMPACTED",
	"CONSUMED",
	"COOKED",
	"CORRECTED",
	"CORRUPTED",
	"CRACKED",
	"CRANKED",
	"CRASHED",
	"CREAMED",
	"CRIPPLED",
	"CRUSHED",
	"CULLED",
	"CULTIVATED",
	"CURED",
	"CUT",
	"DARTED",
	"DEBUGGED",
	"DECAPITATED",
	"DECIMATED",
	"DECLINED",
	"DECONSTRUCTED",
	"DECREASED",
	"DECREMENTED",
	"DEEP FRIED",
	"DEEPWATER HORIZONED",
	"DEFACED",
	"DEFAMED",
	"DEFEATED",
	"DEFLATED",
	"DEFLOWERED",
	"DEFORMED",
	"DEGRADED",
	"DELETED",
	"DEMOLISHED",
	"DEMOTED",
	"DEPOSITED",
	"DESTROYED",
	"DEVASTATED",
	"DID",
	"DIGITALIZED",
	"DIMINISHED",
	"DIPPED",
	"DISASSEMBLED",
	"DISCIPLINED",
	"DISFIGURED",
	"DISJOINTED",
	"DISMANTLED",
	"DISMISSED",
	"DISLIKED",
	"DISPATCHED",
	"DISPERSED",
	"DISPLACED",
	"DISSOLVED",
	"DISTORTED",
	"DIVORCED",
	"DONATED",
	"DOWNGRADED",
	"DOWNVOTED",
	"DRENCHED",
	"DRILLED",
	"DRILLED INTO",
	"DROVE",
	"DUMPED ON",
	"DUMPED",
	"DUMPSTERED",
	"DUNKED",
	"EDUCATED",
	"EGGED",
	"EJECTED",
	"ELABORATED ON",
	"ELIMINATED",
	"EMANCIPATED",
	"EMBARRASSED",
	"EMBRACED",
	"ENDED",
	"ERASED",
	"EXCHANGED",
	"EXECUTED",
	"EXERCISED",
	"EXTERMINATED",
	"EXTRAPOLATED",
	"FACED",
	"FADED",
	"FARTED",
	"FILED",
	"FINANCED",
	"FIRED",
	"FISTED",
	"FIXED",
	"FLAT EARTHED",
	"FLEXED ON",
	"FLUSHED",
	"FOLDED",
	"FOLLOWED",
	"FORKED",
	"FORMATTED",
	"FRAGGED",
	"FREED",
	"FRIED",
	"FROSTED",
	"FUCKED",
	"GAGGED",
	"GARFUNKELED",
	"GERRYMANDERED",
	"GHOSTED",
	"GLAZED",
	"GRAVEDUG",
	"GRILLED",
	"GUTTED",
	"HACKED",
	"HARMED",
	"HURLED",
	"HURT",
	"HOSED",
	"HUMILIATED",
	"HUMPED",
	"ICED",
	"IGNITED",
	"IMPAIRED",
	"IMPREGNATED",
	"INJURED",
	"INSIDE JOBBED",
	"INSTRUCTED",
	"INVADED",
	"JACKED",
	"JAMMED",
	"JERKED",
	"KICKED",
	"KINDLED",
	"KISSED",
	"KNOCKED",
	"LAGGED",
	"LAID",
	"LANDED ON",
	"LANDFILLED",
	"LARDED",
	"LECTURED",
	"LEFT CLICKED ON",
	"LET GO",
	"LIKED",
	"LIQUIDATED",
	"LIT,"
	"LIVESTREAMED",
	"LOGGED",
	"LOOPED",
	"LUGGED",
	"LYNCHED",
	"MASHED",
	"MASSACRED",
	"MASSAGED",
	"MATED WITH",
	"MAULED",
	"MESSAGED",
	"MIXED",
	"MOBBED",
	"MOONWALKED ON",
	"MUNCHED",
	"MURDERED",
	"MUTILATED",
	"NAILED",
	"NEUTRALIZED",
	"NEXTED",
	"NOBODIED",
	"NUKED",
	"NULLIFIED",
	"NURTURED",
	"OBLITERATED",
	"OUTRAGED",
	"OWNED",
	"PACKED",
	"PAID RESPECTS TO",
	"PASTEURIZED",
	"PEELED",
	"PENETRATED",
	"PERFORMED ON",
	"PERISHED",
	"PIED",
	"PILED",
	"PIPED",
	"PISSED ON",
	"PLANTED",
	"PLASTERED",
	"PLOWED", //:v_trump:
	"POACHED",
	"POKED",
	"POOLED",
	"POUNDED",
	"PULVERIZED",
	"PUMPED",
	"PURGED",
	"PUT DOWN",
	"PYONGYANGED",
	"QUASHED",
	"RAISED",
	"RAMMED",
	"RAZED",
	"REAMED",
	"RECYCLED",
	"REDESIGNED",
	"REDUCED",
	"REFORMED",
	"REFRIGERATED",
	"REFUSED",
	"REGULATED",
	"REHABILITATED",
	"REJECTED",
	"RELEASED",
	"RELEGATED",
	"REMOVED",
	"RENDERED",
	"RESPECTED",
	"RETURNED",
	"REVERSED INTO",
	"REVISED",
	"RODE",
	"ROLLED",
	"ROOTED",
	"RUINED",
	"RULED",
	"SABOTAGED",
	"SACKED",
	"SAMPLED",
	"SANDED",
	"SAUCED",
	"SCORCHED",
	"SCREWED",
	"SCUTTLED",
	"SERVED",
	"SEVERED",
	"SHAFTED",
	"SHAGGED",
	"SHANKED",
	"SHAPED",
	"SHARPENED",
	"SHATTERED",
	"SHAVED",
	"SHELVED",
	"SHITTED ON",
	"SHOT",
	"SIMBA'D",
	"SKEWERED",
	"SKIMMED",
	"SLAMMED",
	"SLAPPED",
	"SLAUGHTERED",
	"SLICED",
	"SMACKED",
	"SMASHED",
	"SMEARED",
	"SMOKED",
	"SNAKED",
	"SNAPPED",
	"SNITCHED ON",
	"SOCKED",
	"SOLD",
	"SPANKED",
	"SPLASHED",
	"SPLIT",
	"SPOILED",
	"SPREAD",
	"SQUASHED",
	"SQUEEZED",
	"STABBED",
	"STAMPED OUT",
	"STEAMROLLED",
	"STOMPED",
	"STREAMED",
	"STUFFED",
	"STYLED ON",
	"SUBDUCTED",
	"SUBSCRIBED TO",
	"SUNK",
	"SUSPENDED",
	"SWAMPED",
	"SWIPED",
	"SWIPED LEFT ON",
	"SWIRLED",
	"SYNTHETIZED",
	"TAPPED",
	"TARNISHED",
	"TATTOOED",
	"TERMINATED",
	"TOILET STORED",
	"TOPPLED",
	"TORPEDOED",
	"TOSSED",
	"TRAINED",
	"TRASHED",
	"TROLLED",
	"TRUMPED",
	"TWEETED",
	"TWIRLED",
	"TWISTED",
	"UNFOLLOWED",
	"UNLOCKED",
	"UNTANGLED",
	"UPSET",
	"VANDALIZED",
	"VAPED",
	"VIOLATED",
	"VULKANIZED",
	"WASHED",
	"WASTED",
	"WAXED",
	"WEEDED OUT",
	"WHACKED",
	"WHIPPED",
	"WIPED OUT",
	"WITHDREW",
	"WITNESSED",
	"WRECKED",
};

static const char * prefixes[] = {
	"AIR",
	"ALPHA",
	"AMERICA",
	"ANTI",
	"APE",
	"ASS",
	"ASTRO",
	"BACK",
	"BADASS",
	"BAG",
	"BALL",
	"BANANA",
	"BARBIE",
	"BASEMENT",
	"BASS",
	"BBQ",
	"BELLY",
	"BEYONCE",
	"BINGE",
	"BLOOD",
	"BLUNT",
	"BODY",
	"BOMB",
	"BONUS",
	"BOOST",
	"BOOTY",
	"BOTTOM",
	"BREXIT",
	"BRO",
	"BRUTALLY ",
	"BUFF",
	"BUM",
	"BUSINESS",
	"CAN",
	"CANDY",
	"CANNON",
	"CAPITAL",
	"CARE",
	"CAREFULLY ",
	"CARDINAL",
	"CASH",
	"CASINO",
	"CASSEROLE",
	"CHAOS",
	"CHEESE",
	"CHEERFULLY ",
	"CHICKEN",
	"CHIN",
	"CHINA",
	"CLEVERLY ",
	"CLOUD",
	"CLUTCH",
	"COASTER",
	"COCK",
	"CODEX",
	"COMBO",
	"COMMERCIAL",
	"COLOSSAL",
	"CORN",
	"COUNTER",
	"CRAB",
	"CREAM",
	"CHASSEUR DE ",
	"CRINGE",
	"CRUELLY ",
	"CRUNK",
	"CUM",
	"CUNT",
	"DEEP",
	"DEEPLY ",
	"DEFIANTLY ",
	"DEMON",
	"DEVIL",
	"DICK",
	"DIESEL",
	"DIRT",
	"DONG",
	"DONKEY",
	"DOUBLE",
	"DOWNSTAIRS",
	"DRONE",
	"DUMP",
	"DUNK",
	"DYNAMITE",
	"EARLY",
	"EASILY ",
	"ENERGY",
	"EYE",
	"EXPERTLY",
	"FACE",
	"FAMILY",
	"FART",
	"FAST",
	"FATALLY ",
	"FESTIVAL",
	"FINGER",
	"FISH",
	"FLAT",
	"FLEX",
	"FOOT",
	"FRENCH",
	"FRONT",
	"FULL",
	"FURY",
	"GARBAGE",
	"GEEK",
	"GENDER",
	"GHETTO",
	"GIANT",
	"GINGER",
	"GIRL",
	"GNU/",
	"GOB",
	"GOD",
	"GOOD",
	"GOOF",
	"GRAVE",
	"GRIME",
	"HACK",
	"HAM",
	"HAND",
	"HAPPY",
	"HARAMBE",
	"HEAD",
	"HOLE",
	"HOLY",
	"HOLLYWOOD",
	"HOOD",
	"HOOLIGAN",
	"HORROR",
	"HUSKY",
	"HYPE",
	"HYPER",
	"INDUSTRIAL",
	"INSIDE",
	"JAM",
	"JIMMY",
	"JOKE",
	"JOKER",
	"JOLLY",
	"JOYFULLY ",
	"JUDO",
	"JUICE",
	"JUMBO",
	"KANYE",
	"KARATE",
	"KARMA",
	"KEPT CALM AND ",
	"KIND",
	"KINDLY ",
	"KING",
	"KNEE",
	"KONY2012",
	"LAD",
	"LATE",
	"LEMON",
	"LICK",
	"LITERALLY ",
	"LONG",
	"LOTION",
	"LUBE",
	"MACHINE",
	"MADLY ",
	"MAMMOTH",
	"MASTER",
	"Mc",
	"MEAT",
	"MEGA",
	"MERCY",
	"MERRILY ",
	"META",
	"MILDLY ",
	"MILLENNIUM",
	"MINT",
	"MOLLY",
	"MONKEY",
	"MONSTER",
	"MONUMENTAL",
	"MOUTH",
	"MVP",
	"NASTY",
	"NEAT",
	"NEATLY ",
	"NERD",
	"NETFLIX & ",
	"NICELY ",
	"NUT",
	"OBAMA",
	"OMEGA",
	"OPEN-SOURCE ",
	"OUT",
	"PANTY",
	"PARTY",
	"PERFECTLY ",
	"PERMA",
	"PHENO",
	"PILE",
	"PIMP",
	"PIPE",
	"PLANTER",
	"POWER",
	"PRANK",
	"PROUD",
	"PUMPKIN",
	"PUSSY",
	"QFUSION",
	"QUICK",
	"RAGE",
	"RAM",
	"RAMBO",
	"RAPIDLY ",
	"RAZOR",
	"RESPECT",
	"Rn",
	"ROBOT",
	"ROFL",
	"ROLEX",
	"ROOSTER",
	"ROUGH",
	"ROUGHLY ",
	"ROYAL",
	"RUSSIA",
	"RUTHLESSLY ",
	"SACK",
	"SAD",
	"SALT",
	"SASSY",
	"SANDWICH",
	"SAUCE",
	"SAUSAGE",
	"SERIOUSLY ",
	"SHAME",
	"SHIT",
	"SHORT",
	"SIDE",
	"SILLY",
	"SKILL",
	"SKULL",
	"SMACK",
	"SMILE",
	"SLAM",
	"SLEEP",
	"SLICE",
	"SLOW",
	"SLUT",
	"SMACKDOWN",
	"SMELLY",
	"SMOOTH",
	"SMOOTHLY ",
	"SNAP",
	"SOFT",
	"SOFTLY ",
	"SON",
	"SOUR",
	"SPEED",
	"SPICE",
	"SPORTSMANSHIP",
	"STACK",
	"STAR",
	"STEAM",
	"SUGAR",
	"SUPER",
	"SWEETHEART",
	"SWINDLE",
	"SWOLE",
	"TAKEOUT",
	"TENDER",
	"TENDERLY ",
	"TERROR",
	"TINDER",
	"TITTY",
	"TOILET",
	"TOP",
	"TRAP",
	"TRASH",
	"TRIPLE",
	"TROLL",
	"TRUMP",
	"TRULY ",
	"TURBO",
	"TURKEY",
	"ULTRA",
	"UTTERLY ",
	"UNIVERSAL",
	"VASTLY",
	"VEGAN",
	"VIOLENTLY ",
	"WANG",
	"WASTE",
	"WEINER",
	"WILLY",
	"WHIP",
	"WIFE",
	"WILD",
	"WIDLY ",
	"WISHFULLY ",
	"WONDER",
};

static const char * RandomObituary() {
	return random_select( &cls.rng, obituaries );
}

static const char * RandomPrefix( float p ) {
	if( !random_p( &cls.rng, p ) )
		return "";
	return random_select( &cls.rng, prefixes );
}

/*
* CG_SC_Obituary
*/
void CG_SC_Obituary( void ) {
	char message[128];
	char message2[128];
	cg_clientInfo_t *victim, *attacker;
	int victimNum = atoi( Cmd_Argv( 1 ) );
	int attackerNum = atoi( Cmd_Argv( 2 ) );
	int mod = atoi( Cmd_Argv( 3 ) );
	obituary_t *current;

	victim = &cgs.clientInfo[victimNum - 1];

	if( attackerNum ) {
		attacker = &cgs.clientInfo[attackerNum - 1];
	} else {
		attacker = NULL;
	}

	cg_obituaries_current = ( cg_obituaries_current + 1 ) % MAX_OBITUARIES;
	current = &cg_obituaries[cg_obituaries_current];

	current->time = cls.monotonicTime;
	if( victim ) {
		Q_strncpyz( current->victim, victim->name, sizeof( current->victim ) );
		current->victim_team = cg_entities[victimNum].current.team;
	}
	if( attacker ) {
		Q_strncpyz( current->attacker, attacker->name, sizeof( current->attacker ) );
		current->attacker_team = cg_entities[attackerNum].current.team;
	}
	current->mod = mod;

	GS_Obituary( victim, attacker, mod, message, message2 );

	if( attackerNum ) {
		if( victimNum != attackerNum ) {
			current->type = OBITUARY_NORMAL;
			if( cg_showObituaries->integer & CG_OBITUARY_CONSOLE ) {
				CG_LocalPrint( "%s %s %s%s\n", victim->name, message, attacker->name, message2 );
			}

			if( ISVIEWERENTITY( attackerNum ) && ( cg_showObituaries->integer & CG_OBITUARY_CENTER ) ) {
				char name[MAX_NAME_CHARS + 1];
				Q_strncpyz( name, victim->name, sizeof( name ) );
				Q_strupr( name );
				CG_CenterPrint( va( "YOU %s%s%s %s", RandomPrefix( 0.05f ), RandomPrefix( 0.5f ), RandomObituary(), name ) );
			}
		} else {   // suicide
			current->type = OBITUARY_SUICIDE;
			if( cg_showObituaries->integer & CG_OBITUARY_CONSOLE ) {
				CG_LocalPrint( "%s %s\n", victim->name, message );
			}
		}
	} else {   // world accidents
		current->type = OBITUARY_ACCIDENT;
		if( cg_showObituaries->integer & CG_OBITUARY_CONSOLE ) {
			CG_LocalPrint( "%s %s\n", victim->name, message );
		}
	}
}

static const Material * CG_GetWeaponIcon( int weapon ) {
	return cgs.media.shaderWeaponIcon[ weapon - WEAP_GUNBLADE ];
}

static void CG_DrawObituaries(
	int x, int y, Alignment alignment, int width, int height,
	int internal_align, unsigned int icon_size
) {
	const int icon_padding = 4;

	if( !( cg_showObituaries->integer & CG_OBITUARY_HUD ) ) {
		return;
	}

	unsigned line_height = Max3( 1u, unsigned( layout_cursor_font_size ), icon_size );
	int num_max = height / line_height;

	if( width < (int)icon_size || !num_max ) {
		return;
	}

	const Font * font = GetHUDFont();

	int next = cg_obituaries_current + 1;
	if( next >= MAX_OBITUARIES ) {
		next = 0;
	}

	int num = 0;
	int i = next;
	do {
		if( cg_obituaries[i].type != OBITUARY_NONE && cls.monotonicTime - cg_obituaries[i].time <= 5000 ) {
			num++;
		}
		if( ++i >= MAX_OBITUARIES ) {
			i = 0;
		}
	} while( i != next );

	int skip;
	if( num > num_max ) {
		skip = num - num_max;
		num = num_max;
	} else {
		skip = 0;
	}

	y = CG_VerticalAlignForHeight( y, alignment, height );
	x = CG_HorizontalAlignForWidth( x, alignment, width );

	int xoffset = 0;
	int yoffset = 0;

	i = next;
	do {
		const obituary_t * obr = &cg_obituaries[i];
		if( ++i >= MAX_OBITUARIES ) {
			i = 0;
		}

		if( obr->type == OBITUARY_NONE || cls.monotonicTime - obr->time > 5000 ) {
			continue;
		}

		if( skip > 0 ) {
			skip--;
			continue;
		}

		const Material *pic;
		switch( obr->mod ) {
			case MOD_GUNBLADE:
				pic = CG_GetWeaponIcon( WEAP_GUNBLADE );
				break;
			case MOD_MACHINEGUN:
				pic = CG_GetWeaponIcon( WEAP_MACHINEGUN );
				break;
			case MOD_RIOTGUN:
				pic = CG_GetWeaponIcon( WEAP_RIOTGUN );
				break;
			case MOD_GRENADE:
			case MOD_GRENADE_SPLASH:
				pic = CG_GetWeaponIcon( WEAP_GRENADELAUNCHER );
				break;
			case MOD_ROCKET:
			case MOD_ROCKET_SPLASH:
				pic = CG_GetWeaponIcon( WEAP_ROCKETLAUNCHER );
				break;
			case MOD_PLASMA:
			case MOD_PLASMA_SPLASH:
				pic = CG_GetWeaponIcon( WEAP_PLASMAGUN );
				break;
			case MOD_ELECTROBOLT:
				pic = CG_GetWeaponIcon( WEAP_ELECTROBOLT );
				break;
			case MOD_LASERGUN:
				pic = CG_GetWeaponIcon( WEAP_LASERGUN );
				break;
			default:
				pic = CG_GetWeaponIcon( WEAP_GUNBLADE ); // FIXME
				break;
		}

		float attacker_width = TextBounds( font, layout_cursor_font_size, obr->attacker ).maxs.x;
		float victim_width = TextBounds( font, layout_cursor_font_size, obr->victim ).maxs.x;

		int w = 0;
		if( obr->type != OBITUARY_ACCIDENT ) {
			w += attacker_width;
		}
		w += icon_padding;
		w += icon_size;
		w += icon_padding;
		w += victim_width;

		if( internal_align == 1 ) {
			// left
			xoffset = 0;
		} else if( internal_align == 2 ) {
			// center
			xoffset = ( width - w ) / 2;
		} else {
			// right
			xoffset = width - w;
		}

		int obituary_y = y + yoffset + ( line_height - layout_cursor_font_size ) / 2;
		if( obr->type != OBITUARY_ACCIDENT ) {
			Vec4 color = CG_TeamColorVec4( obr->attacker_team );
			DrawText( font, layout_cursor_font_size, obr->attacker, x + xoffset, obituary_y, color, layout_cursor_font_border );
			xoffset += attacker_width;
		}

		xoffset += icon_padding;

		Draw2DBox( x + xoffset, y + yoffset + ( line_height - icon_size ) / 2, icon_size, icon_size, pic, vec4_white );

		xoffset += icon_size + icon_padding;

		Vec4 color = CG_TeamColorVec4( obr->victim_team );
		DrawText( font, layout_cursor_font_size, obr->victim, x + xoffset, obituary_y, color, layout_cursor_font_border );

		yoffset += line_height;
	} while( i != next );
}

//=============================================================================

void CG_ClearAwards( void ) {
	// reset awards
	cg.award_head = 0;
	memset( cg.award_times, 0, sizeof( cg.award_times ) );
}

static void CG_DrawAwards( int x, int y, Alignment alignment, float font_size, Vec4 color, bool border ) {
	if( !cg_showAwards->integer ) {
		return;
	}

	if( !cg.award_head ) {
		return;
	}

	int count;
	for( count = 0; count < MAX_AWARD_LINES; count++ ) {
		int current = ( ( cg.award_head - 1 ) - count );
		if( current < 0 ) {
			break;
		}

		if( cg.award_times[current % MAX_AWARD_LINES] + MAX_AWARD_DISPLAYTIME < cl.serverTime ) {
			break;
		}

		if( !cg.award_lines[current % MAX_AWARD_LINES][0] ) {
			break;
		}
	}

	if( !count ) {
		return;
	}

	y = CG_VerticalAlignForHeight( y, alignment, font_size * MAX_AWARD_LINES );

	for( int i = count; i > 0; i-- ) {
		int current = ( cg.award_head - i ) % MAX_AWARD_LINES;
		const char *str = cg.award_lines[ current ];

		int yoffset = font_size * ( MAX_AWARD_LINES - i );

		DrawText( GetHUDFont(), font_size, str, alignment, x, y + yoffset, color, border );
	}
}

static bool CG_LFuncDrawCallvote( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char * vote = cgs.configStrings[ CS_CALLVOTE ];
	if( strlen( vote ) == 0 )
		return true;

	int left = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_alignment, layout_cursor_width );
	int top = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_alignment, layout_cursor_height );
	int right = left + layout_cursor_width;

	TempAllocator temp = cls.frame_arena.temp();

	const char * yeses = cgs.configStrings[ CS_CALLVOTE_YES_VOTES ];
	const char * required = cgs.configStrings[ CS_CALLVOTE_REQUIRED_VOTES ];

	bool voted = cg.predictedPlayerState.stats[ STAT_LAYOUTS ] & STAT_LAYOUT_VOTED;
	float padding = layout_cursor_font_size * 0.5f;

	if( !voted ) {
		float height = padding * 2 + layout_cursor_font_size * 2.2f;
		Draw2DBox( left, top, layout_cursor_width, height, cgs.white_material, Vec4( 0, 0, 0, 0.5f ) );
	}

	Vec4 color = voted ? vec4_white : AttentionGettingColor();

	DrawText( GetHUDFont(), layout_cursor_font_size, temp( "Vote: {}", vote ), left + padding, top + padding, color, true );
	DrawText( GetHUDFont(), layout_cursor_font_size, temp( "{}/{}", yeses, required ), Alignment_RightTop, right - padding, top + padding, color, true );

	if( !voted ) {
		char vote_yes_keys[ 128 ];
		CG_GetBoundKeysString( "vote yes", vote_yes_keys, sizeof( vote_yes_keys ) );
		char vote_no_keys[ 128 ];
		CG_GetBoundKeysString( "vote no", vote_no_keys, sizeof( vote_no_keys ) );

		const char * str = temp( "[{}] Vote yes [{}] Vote no", vote_yes_keys, vote_no_keys );
		float y = top + padding + layout_cursor_font_size * 1.2f;
		DrawText( GetHUDFont(), layout_cursor_font_size, str, left + padding, y, color, true );
	}

	return true;
}

//=============================================================================
//	STATUS BAR PROGRAMS
//=============================================================================

typedef float ( *opFunc_t )( const float a, float b );

// we will always operate with floats so we don't have to code 2 different numeric paths
// it's not like using float or ints would make a difference in this simple-scripting case.

static float CG_OpFuncAdd( const float a, const float b ) {
	return a + b;
}

static float CG_OpFuncSubtract( const float a, const float b ) {
	return a - b;
}

static float CG_OpFuncMultiply( const float a, const float b ) {
	return a * b;
}

static float CG_OpFuncDivide( const float a, const float b ) {
	return a / b;
}

static float CG_OpFuncAND( const float a, const float b ) {
	return (int)a & (int)b;
}

static float CG_OpFuncOR( const float a, const float b ) {
	return (int)a | (int)b;
}

static float CG_OpFuncXOR( const float a, const float b ) {
	return (int)a ^ (int)b;
}

static float CG_OpFuncCompareEqual( const float a, const float b ) {
	return ( a == b );
}

static float CG_OpFuncCompareNotEqual( const float a, const float b ) {
	return ( a != b );
}

static float CG_OpFuncCompareGreater( const float a, const float b ) {
	return ( a > b );
}

static float CG_OpFuncCompareGreaterOrEqual( const float a, const float b ) {
	return ( a >= b );
}

static float CG_OpFuncCompareSmaller( const float a, const float b ) {
	return ( a < b );
}

static float CG_OpFuncCompareSmallerOrEqual( const float a, const float b ) {
	return ( a <= b );
}

static float CG_OpFuncCompareAnd( const float a, const float b ) {
	return ( a && b );
}

static float CG_OpFuncCompareOr( const float a, const float b ) {
	return ( a || b );
}

typedef struct cg_layoutoperators_s
{
	const char *name;
	opFunc_t opFunc;
} cg_layoutoperators_t;

static cg_layoutoperators_t cg_LayoutOperators[] =
{
	{
		"+",
		CG_OpFuncAdd
	},

	{
		"-",
		CG_OpFuncSubtract
	},

	{
		"*",
		CG_OpFuncMultiply
	},

	{
		"/",
		CG_OpFuncDivide
	},

	{
		"&",
		CG_OpFuncAND
	},

	{
		"|",
		CG_OpFuncOR
	},

	{
		"^",
		CG_OpFuncXOR
	},

	{
		"==",
		CG_OpFuncCompareEqual
	},

	{
		"!=",
		CG_OpFuncCompareNotEqual
	},

	{
		">",
		CG_OpFuncCompareGreater
	},

	{
		">=",
		CG_OpFuncCompareGreaterOrEqual
	},

	{
		"<",
		CG_OpFuncCompareSmaller
	},

	{
		"<=",
		CG_OpFuncCompareSmallerOrEqual
	},

	{
		"&&",
		CG_OpFuncCompareAnd
	},

	{
		"||",
		CG_OpFuncCompareOr
	},

	{
		NULL,
		NULL
	},
};

/*
* CG_OperatorFuncForArgument
*/
static opFunc_t CG_OperatorFuncForArgument( const char *token ) {
	cg_layoutoperators_t *op;

	while( *token == ' ' )
		token++;

	for( op = cg_LayoutOperators; op->name; op++ ) {
		if( !Q_stricmp( token, op->name ) ) {
			return op->opFunc;
		}
	}

	return NULL;
}

//=============================================================================

static const char *CG_GetStringArg( struct cg_layoutnode_s **argumentsnode );
static float CG_GetNumericArg( struct cg_layoutnode_s **argumentsnode );

//=============================================================================

enum {
	LNODE_NUMERIC,
	LNODE_STRING,
	LNODE_REFERENCE_NUMERIC,
	LNODE_COMMAND
};

//=============================================================================
// Commands' Functions
//=============================================================================

static bool CG_IsWeaponSelected( int weapon ) {
	if( cg.view.playerPrediction && cg.predictedWeaponSwitch && cg.predictedWeaponSwitch != cg.predictedPlayerState.stats[STAT_PENDING_WEAPON] ) {
		return weapon == cg.predictedWeaponSwitch;
	}

	return weapon == cg.predictedPlayerState.stats[STAT_PENDING_WEAPON];
}

constexpr float SEL_WEAP_X_OFFSET = 0.25f;

static void CG_DrawWeaponIcons( int x, int y, int offx, int offy, int iw, int ih, Alignment alignment, float font_size ) {
	int num_weapons = 0;
	for( int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
		if( CG_IsWeaponInList( i ) ) {
			num_weapons++;
		}
	}

	int padx = offx - iw;
	int pady = offy - ih;
	int total_width = max( 0, num_weapons * offx - padx );
	int total_height = max( 0, num_weapons * offy - pady );

	int drawn_weapons = 0;
	bool selected_found = false;
	for( int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
		if( !CG_IsWeaponInList( i ) )
			continue;

		int curx = CG_HorizontalAlignForWidth( x + offx * drawn_weapons, alignment, total_width );
		int cury = CG_VerticalAlignForHeight( y + offy * drawn_weapons, alignment, total_height );

		int curiw = iw;
		int curih = ih;

		if( CG_IsWeaponSelected( i ) ) {
			selected_found = true;
			cury -= ih * SEL_WEAP_X_OFFSET;
		}

		Vec4 color = Vec4( 1.0f );
		Vec4 color_bg = Vec4( 0.5f );
		// if ( i != WEAP_GUNBLADE ) {
		int ammo = cg.predictedPlayerState.inventory[ AMMO_GUNBLADE + i - WEAP_GUNBLADE ];

		int ammo_in_clip = 0;


		if ( i != WEAP_GUNBLADE ) {
			int capacity = GS_FindItemByTag( i )->capacity;
			int clips = GS_FindItemByTag( i )->clips;
			int ammo_max =  clips * capacity;
			ammo_in_clip = capacity - ((ammo_max - ammo) % capacity);

			color = Vec4( 0.0f, 1.0f, 0.0f, 1.0f );
			color_bg = Vec4( 0.0f, 0.5f, 0.0f, 1.0f );

			int ammo_in_clip_pct = ( 100  * ammo_in_clip + capacity / 2) /capacity;
			if ( ammo_in_clip_pct <= 67) {
				color = Vec4( 1.0f, 0.5f, 0.0f, 1.0f );
				color_bg = Vec4( 0.5f, 0.25f, 0.0f, 1.0f );
			}
			if ( ammo_in_clip_pct <= 34) {
				color = Vec4( 1.0f, 0.0f, 0.0f, 1.0f );
				color_bg = Vec4( 0.5f, 0.0f, 0.0f, 1.0f );
			}
		}

		Draw2DBox( curx, cury, curiw, curih, cgs.white_material, color );
		Draw2DBox( curx + roundf( curiw * 0.03f ), cury + roundf ( curih * 0.03f ), roundf( curiw * 0.95f ), roundf( curih * 0.95f ), cgs.white_material, color_bg );
		Draw2DBox( curx + roundf( curiw * 0.16f ), cury + roundf ( curih * 0.16f ), roundf( curiw * 0.69f ), roundf( curiw * 0.69f ), CG_GetWeaponIcon( i ), color );

		if( i != WEAP_GUNBLADE ) {
			DrawText( GetHUDFont(), font_size + (curiw - iw)/4, va( "%i", ammo_in_clip ), Alignment_LeftBottom, curx + curiw*0.15f, cury + curih*0.85f, layout_cursor_color, layout_cursor_font_border );
		}

		drawn_weapons++;
	}
}

static bool CG_LFuncDrawPicByName( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int x = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_alignment, layout_cursor_width );
	int y = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_alignment, layout_cursor_height );
	Draw2DBox( x, y, layout_cursor_width, layout_cursor_height, FindMaterial( CG_GetStringArg( &argumentnode ) ), layout_cursor_color );
	return true;
}

static float ScaleX( float x ) {
	return x * frame_static.viewport_width / 800.0f;
}

static float ScaleY( float y ) {
	return y * frame_static.viewport_height / 600.0f;
}

static bool CG_LFuncCursor( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float x = ScaleX( CG_GetNumericArg( &argumentnode ) );
	float y = ScaleY( CG_GetNumericArg( &argumentnode ) );

	layout_cursor_x = Q_rint( x );
	layout_cursor_y = Q_rint( y );
	return true;
}

static bool CG_LFuncMoveCursor( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float x = ScaleX( CG_GetNumericArg( &argumentnode ) );
	float y = ScaleY( CG_GetNumericArg( &argumentnode ) );

	layout_cursor_x += Q_rint( x );
	layout_cursor_y += Q_rint( y );
	return true;
}

static bool CG_LFuncSize( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	float x = ScaleX( CG_GetNumericArg( &argumentnode ) );
	float y = ScaleY( CG_GetNumericArg( &argumentnode ) );

	layout_cursor_width = Q_rint( x );
	layout_cursor_height = Q_rint( y );
	return true;
}

static bool CG_LFuncColor( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	for( int i = 0; i < 4; i++ ) {
		layout_cursor_color.ptr()[ i ] = Clamp01( CG_GetNumericArg( &argumentnode ) );
	}
	return true;
}

static bool CG_LFuncColorToTeamColor( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	layout_cursor_color = CG_TeamColorVec4( CG_GetNumericArg( &argumentnode ) );
	return true;
}

static bool CG_LFuncAttentionGettingColor( struct cg_layoutnode_s * argumentnode, int numArguments ) {
	layout_cursor_color = AttentionGettingColor();
	return true;
}

static bool CG_LFuncColorAlpha( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	layout_cursor_color.w = CG_GetNumericArg( &argumentnode );
	return true;
}

static bool CG_LFuncAlignment( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char * x = CG_GetStringArg( &argumentnode );
	const char * y = CG_GetStringArg( &argumentnode );

	if( !Q_stricmp( x, "left" ) ) {
		layout_cursor_alignment.x = XAlignment_Left;
	}
	else if( !Q_stricmp( x, "center" ) ) {
		layout_cursor_alignment.x = XAlignment_Center;
	}
	else if( !Q_stricmp( x, "right" ) ) {
		layout_cursor_alignment.x = XAlignment_Right;
	}
	else {
		CG_Printf( "WARNING 'CG_LFuncAlignment' Unknown alignment '%s'", x );
		return false;
	}

	if( !Q_stricmp( y, "top" ) ) {
		layout_cursor_alignment.y = YAlignment_Top;
	}
	else if( !Q_stricmp( y, "middle" ) ) {
		layout_cursor_alignment.y = YAlignment_Middle;
	}
	else if( !Q_stricmp( y, "bottom" ) ) {
		layout_cursor_alignment.y = YAlignment_Bottom;
	}
	else {
		CG_Printf( "WARNING 'CG_LFuncAlignment' Unknown alignment '%s'", y );
		return false;
	}

	return true;
}

static bool CG_LFuncFontSize( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	struct cg_layoutnode_s *charnode = argumentnode;
	const char * fontsize = CG_GetStringArg( &charnode );

	if( !Q_stricmp( fontsize, "tiny" ) ) {
		layout_cursor_font_size = cgs.textSizeTiny;
	}
	else if( !Q_stricmp( fontsize, "small" ) ) {
		layout_cursor_font_size = cgs.textSizeSmall;
	}
	else if( !Q_stricmp( fontsize, "medium" ) ) {
		layout_cursor_font_size = cgs.textSizeMedium;
	}
	else if( !Q_stricmp( fontsize, "big" ) ) {
		layout_cursor_font_size = cgs.textSizeBig;
	}
	else {
		layout_cursor_font_size = CG_GetNumericArg( &argumentnode );
	}

	return true;
}

static bool CG_LFuncFontStyle( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char * fontstyle = CG_GetStringArg( &argumentnode );

	if( !Q_stricmp( fontstyle, "normal" ) ) {
		layout_cursor_font_style = FontStyle_Normal;
	}
	else if( !Q_stricmp( fontstyle, "bold" ) ) {
		layout_cursor_font_style = FontStyle_Bold;
	}
	else if( !Q_stricmp( fontstyle, "italic" ) ) {
		layout_cursor_font_style = FontStyle_Italic;
	}
	else if( !Q_stricmp( fontstyle, "bold-italic" ) ) {
		layout_cursor_font_style = FontStyle_BoldItalic;
	}
	else {
		CG_Printf( "WARNING 'CG_LFuncFontStyle' Unknown font style '%s'", fontstyle );
		return false;
	}

	return true;
}

static bool CG_LFuncFontBorder( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char * border = CG_GetStringArg( &argumentnode );
	layout_cursor_font_border = Q_stricmp( border, "on" ) == 0;
	return true;
}

static bool CG_LFuncDrawObituaries( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int internal_align = (int)CG_GetNumericArg( &argumentnode );
	int icon_size = (int)CG_GetNumericArg( &argumentnode );

	CG_DrawObituaries( layout_cursor_x, layout_cursor_y, layout_cursor_alignment,
		layout_cursor_width, layout_cursor_height, internal_align, icon_size * frame_static.viewport_height / 600 );
	return true;
}

static bool CG_LFuncDrawAwards( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawAwards( layout_cursor_x, layout_cursor_y, layout_cursor_alignment, layout_cursor_font_size, layout_cursor_color, layout_cursor_font_border );
	return true;
}

static bool CG_LFuncDrawClock( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawClock( layout_cursor_x, layout_cursor_y, layout_cursor_alignment, GetHUDFont(), layout_cursor_font_size, layout_cursor_color, layout_cursor_font_border );
	return true;
}

static bool CG_LFuncDrawDamageNumbers( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawDamageNumbers();
	return true;
}

static bool CG_LFuncDrawBombIndicators( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawBombHUD();
	return true;
}

static bool CG_LFuncDrawPlayerIcons( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int team = int( CG_GetNumericArg( &argumentnode ) );
	int alive_index = int( CG_GetNumericArg( &argumentnode ) );
	int total_index = int( CG_GetNumericArg( &argumentnode ) );

	if( total_index < 0 || alive_index < 0 || total_index >= MAX_CONFIGSTRINGS || alive_index >= MAX_CONFIGSTRINGS ) {
		CG_Printf( "WARNING 'CG_LFuncDrawPlayerIcons' configstring out of range" );
		return false;
	}

	int alive = atoi( cgs.configStrings[ alive_index ] );
	int total = atoi( cgs.configStrings[ total_index ] );
	Vec4 team_color = CG_TeamColorVec4( team );

	const Material * icon = FindMaterial( "gfx/hud/guy" );

	float height = layout_cursor_font_size;
	float width = float( icon->texture->width ) / float( icon->texture->height ) * height;
	float padding = width * 0.25f;

	float x = layout_cursor_x;
	float y = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_alignment, height );
	float dx = width + padding;
	if( layout_cursor_alignment.x == XAlignment_Right ) {
		x -= width;
		dx = -dx;
	}

	for( int i = 0; i < total; i++ ) {
		Vec4 color = i < alive ? team_color : layout_cursor_color;
		Draw2DBox( x + dx * i, y, width, height, icon, color );
	}

	return true;
}

static bool CG_LFuncDrawPointed( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawPlayerNames( GetHUDFont(), layout_cursor_font_size, layout_cursor_color, layout_cursor_font_border );
	return true;
}

static bool CG_LFuncDrawString( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char *string = CG_GetStringArg( &argumentnode );

	if( !string || !string[0] ) {
		return false;
	}

	DrawText( GetHUDFont(), layout_cursor_font_size, string, layout_cursor_alignment, layout_cursor_x, layout_cursor_y, layout_cursor_color, layout_cursor_font_border );

	return true;
}

static bool CG_LFuncDrawBindString( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char * fmt = CG_GetStringArg( &argumentnode );
	const char * command = CG_GetStringArg( &argumentnode );

	char keys[ 128 ];
	if( !CG_GetBoundKeysString( command, keys, sizeof( keys ) ) ) {
		snprintf( keys, sizeof( keys ), "[%s]", command );
	}

	char buf[ 1024 ];
	snprintf( buf, sizeof( buf ), fmt, keys );

	DrawText( GetHUDFont(), layout_cursor_font_size, buf, layout_cursor_alignment, layout_cursor_x, layout_cursor_y, layout_cursor_color, layout_cursor_font_border );

	return true;
}

static bool CG_LFuncDrawPlayerName( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int index = (int)CG_GetNumericArg( &argumentnode ) - 1;

	if( index >= 0 && index < client_gs.maxclients && cgs.clientInfo[index].name[0] ) {
		DrawText( GetHUDFont(), layout_cursor_font_size, cgs.clientInfo[ index ].name, layout_cursor_alignment, layout_cursor_x, layout_cursor_y, layout_cursor_color, layout_cursor_font_border );
		return true;
	}

	return false;
}

static bool CG_LFuncDrawNumeric( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	int value = (int)CG_GetNumericArg( &argumentnode );
	DrawText( GetHUDFont(), layout_cursor_font_size, va( "%i", value ), layout_cursor_alignment, layout_cursor_x, layout_cursor_y, layout_cursor_color, layout_cursor_font_border );
	return true;
}

static void CG_LFuncsWeaponIcons( struct cg_layoutnode_s *argumentnode ) {
	int offx, offy, w, h;

	offx = (int)( CG_GetNumericArg( &argumentnode ) * frame_static.viewport_width / 800 );
	offy = (int)( CG_GetNumericArg( &argumentnode ) * frame_static.viewport_height / 600 );
	w = (int)( CG_GetNumericArg( &argumentnode ) * frame_static.viewport_width / 800 );
	h = (int)( CG_GetNumericArg( &argumentnode ) * frame_static.viewport_height / 600 );
	float font_size = CG_GetNumericArg( &argumentnode );

	CG_DrawWeaponIcons( layout_cursor_x, layout_cursor_y, offx, offy, w, h, layout_cursor_alignment, font_size );
}

static bool CG_LFuncDrawWeaponIcons( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_LFuncsWeaponIcons( argumentnode );
	return true;
}


static bool CG_LFuncDrawCrossHair( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawCrosshair();
	return true;
}

static bool CG_LFuncDrawKeyState( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	const char *key = CG_GetStringArg( &argumentnode );

	CG_DrawKeyState( layout_cursor_x, layout_cursor_y, layout_cursor_width, layout_cursor_height, key );
	return true;
}

static bool CG_LFuncDrawNet( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	CG_DrawNet( layout_cursor_x, layout_cursor_y, layout_cursor_width, layout_cursor_height, layout_cursor_alignment, layout_cursor_color );
	return true;
}

static bool CG_LFuncIf( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	return (int)CG_GetNumericArg( &argumentnode ) != 0;
}

static bool CG_LFuncIfNot( struct cg_layoutnode_s *argumentnode, int numArguments ) {
	return (int)CG_GetNumericArg( &argumentnode ) == 0;
}


typedef struct cg_layoutcommand_s
{
	const char *name;
	bool ( *func )( struct cg_layoutnode_s *argumentnode, int numArguments );
	int numparms;
	const char *help;
} cg_layoutcommand_t;

static const cg_layoutcommand_t cg_LayoutCommands[] =
{
	{
		"setCursor",
		CG_LFuncCursor,
		2,
		"Sets the cursor position to x and y coordinates.",
	},

	{
		"moveCursor",
		CG_LFuncMoveCursor,
		2,
		"Moves the cursor position by dx and dy.",
	},

	{
		"setAlignment",
		CG_LFuncAlignment,
		2,
		"Changes alignment setting",
	},

	{
		"setSize",
		CG_LFuncSize,
		2,
		"Sets width and height. Used for pictures and models.",
	},

	{
		"setFontSize",
		CG_LFuncFontSize,
		1,
		"Sets font by font name. Accepts tiny/small/medium/big as shortcuts to default game fonts sizes.",
	},

	{
		"setFontStyle",
		CG_LFuncFontStyle,
		1,
		"Sets font style. Possible values are: 'normal', 'italic', 'bold' and 'bold-italic'.",
	},

	{
		"setFontBorder",
		CG_LFuncFontBorder,
		1,
		"Enable or disable font border. Values are 'on' and 'off'",
	},

	{
		"setColor",
		CG_LFuncColor,
		4,
		"Sets color setting in RGBA mode. Used for text and pictures",
	},

	{
		"setColorToTeamColor",
		CG_LFuncColorToTeamColor,
		1,
		"Sets cursor color to the color of the team provided in the argument",
	},

	{
		"setAttentionGettingColor",
		CG_LFuncAttentionGettingColor,
		0,
		"Sets cursor color to something that flashes and is distracting",
	},

	{
		"setColorAlpha",
		CG_LFuncColorAlpha,
		1,
		"Changes the alpha value of the current color",
	},

	{
		"drawObituaries",
		CG_LFuncDrawObituaries,
		2,
		"Draws graphical death messages",
	},

	{
		"drawAwards",
		CG_LFuncDrawAwards,
		0,
		"Draws award messages",
	},

	{
		"drawCallvote",
		CG_LFuncDrawCallvote,
		0,
		"Draw callvote",
	},

	{
		"drawClock",
		CG_LFuncDrawClock,
		0,
		"Draws clock",
	},

	{
		"drawPlayerName",
		CG_LFuncDrawPlayerName,
		1,
		"Draws the name of the player with id provided by the argument, colored with color tokens, white by default",
	},

	{
		"drawPointing",
		CG_LFuncDrawPointed,
		0,
		"Draws the name of the player in the crosshair",
	},

	{
		"drawDamageNumbers",
		CG_LFuncDrawDamageNumbers,
		0,
		"Draws damage numbers",
	},

	{
		"drawBombIndicators",
		CG_LFuncDrawBombIndicators,
		0,
		"Draws bomb HUD",
	},

	{
		"drawPlayerIcons",
		CG_LFuncDrawPlayerIcons,
		3,
		"Draw dead/alive player icons",
	},

	{
		"drawString",
		CG_LFuncDrawString,
		1,
		"Draws the string in the argument",
	},

	{
		"drawStringNum",
		CG_LFuncDrawNumeric,
		1,
		"Draws numbers as text",
	},

	{
		"drawBindString",
		CG_LFuncDrawBindString,
		2,
		"Draws a string with %s replaced by a key name",
	},

	{
		"drawCrosshair",
		CG_LFuncDrawCrossHair,
		0,
		"Draws the game crosshair",
	},

	{
		"drawKeyState",
		CG_LFuncDrawKeyState,
		1,
		"Draws icons showing if the argument key is pressed. Possible arg: forward, backward, left, right, fire, jump, crouch, special",
	},

	{
		"drawNetIcon",
		CG_LFuncDrawNet,
		0,
		"Draws the disconnection icon",
	},

	{
		"drawPicByName",
		CG_LFuncDrawPicByName,
		1,
		"Draws a pic with argument being the file path",
	},

	{
		"drawWeaponIcons",
		CG_LFuncDrawWeaponIcons,
		5,
		"Draws the icons of weapon/ammo owned by the player, arguments are offset x, offset y, size x, size y",
	},

	{
		"if",
		CG_LFuncIf,
		1,
		"Conditional expression. Argument accepts operations >, <, ==, >=, <=, etc",
	},

	{
		"ifnot",
		CG_LFuncIfNot,
		1,
		"Negative conditional expression. Argument accepts operations >, <, ==, >=, <=, etc",
	},

	{
		"endif",
		NULL,
		0,
		"End of conditional expression block",
	},

	{ }
};


//=============================================================================


typedef struct cg_layoutnode_s
{
	bool ( *func )( struct cg_layoutnode_s *argumentnode, int numArguments );
	int type;
	char *string;
	int integer;
	float value;
	opFunc_t opFunc;
	struct cg_layoutnode_s *parent;
	struct cg_layoutnode_s *next;
	struct cg_layoutnode_s *ifthread;
} cg_layoutnode_t;

/*
* CG_GetStringArg
*/
static const char *CG_GetStringArg( struct cg_layoutnode_s **argumentsnode ) {
	struct cg_layoutnode_s *anode = *argumentsnode;

	if( !anode || anode->type == LNODE_COMMAND ) {
		CG_Error( "'CG_LayoutGetStringArg': bad arg count" );
	}

	// we can return anything as string
	*argumentsnode = anode->next;
	return anode->string;
}

/*
* CG_GetNumericArg
* can use recursion for mathematical operations
*/
static float CG_GetNumericArg( struct cg_layoutnode_s **argumentsnode ) {
	struct cg_layoutnode_s *anode = *argumentsnode;
	float value;

	if( !anode || anode->type == LNODE_COMMAND ) {
		CG_Error( "'CG_LayoutGetNumericArg': bad arg count" );
	}

	if( anode->type != LNODE_NUMERIC && anode->type != LNODE_REFERENCE_NUMERIC ) {
		CG_Printf( "WARNING: 'CG_LayoutGetNumericArg': arg %s is not numeric", anode->string );
	}

	*argumentsnode = anode->next;
	if( anode->type == LNODE_REFERENCE_NUMERIC ) {
		value = cg_numeric_references[anode->integer].func( cg_numeric_references[anode->integer].parameter );
	} else {
		value = anode->value;
	}

	// recurse if there are operators
	if( anode->opFunc != NULL ) {
		value = anode->opFunc( value, CG_GetNumericArg( argumentsnode ) );
	}

	return value;
}

/*
* CG_LayoutParseCommandNode
* alloc a new node for a command
*/
static cg_layoutnode_t *CG_LayoutParseCommandNode( const char *token ) {
	int i = 0;
	const cg_layoutcommand_t *command = NULL;
	cg_layoutnode_t *node;

	for( i = 0; cg_LayoutCommands[i].name; i++ ) {
		if( !Q_stricmp( token, cg_LayoutCommands[i].name ) ) {
			command = &cg_LayoutCommands[i];
			break;
		}
	}

	if( command == NULL ) {
		return NULL;
	}

	node = ( cg_layoutnode_t * )CG_Malloc( sizeof( cg_layoutnode_t ) );
	node->type = LNODE_COMMAND;
	node->integer = command->numparms;
	node->value = 0.0f;
	node->string = CG_CopyString( command->name );
	node->func = command->func;
	node->ifthread = NULL;

	return node;
}

/*
* CG_LayoutParseArgumentNode
* alloc a new node for an argument
*/
static cg_layoutnode_t *CG_LayoutParseArgumentNode( const char *token ) {
	cg_layoutnode_t *node;
	int type = LNODE_NUMERIC;
	const char *valuetok;
	static char tmpstring[8];

	// find what's it
	if( !token ) {
		return NULL;
	}

	valuetok = token;

	if( token[0] == '%' ) { // it's a stat parm
		int i;
		type = LNODE_REFERENCE_NUMERIC;
		valuetok++; // skip %

		// replace stat names by values
		for( i = 0; cg_numeric_references[i].name != NULL; i++ ) {
			if( !Q_stricmp( valuetok, cg_numeric_references[i].name ) ) {
				snprintf( tmpstring, sizeof( tmpstring ), "%i", i );
				valuetok = tmpstring;
				break;
			}
		}
		if( cg_numeric_references[i].name == NULL ) {
			CG_Printf( "Warning: HUD: %s is not valid numeric reference\n", valuetok );
			valuetok--;
			valuetok = "0";
		}
	} else if( token[0] == '#' ) {   // it's a integer constant
		int i;
		type = LNODE_NUMERIC;
		valuetok++; // skip #

		for( i = 0; cg_numeric_constants[i].name != NULL; i++ ) {
			if( !Q_stricmp( valuetok, cg_numeric_constants[i].name ) ) {
				snprintf( tmpstring, sizeof( tmpstring ), "%i", cg_numeric_constants[i].value );
				valuetok = tmpstring;
				break;
			}
		}
		if( cg_numeric_constants[i].name == NULL ) {
			CG_Printf( "Warning: HUD: %s is not valid numeric constant\n", valuetok );
			valuetok = "0";
		}

	} else if( token[0] == '\\' ) {
		valuetok = ++token;
		type = LNODE_STRING;
	} else if( token[0] < '0' && token[0] > '9' && token[0] != '.' ) {
		type = LNODE_STRING;
	}

	// alloc
	node = ( cg_layoutnode_t * )CG_Malloc( sizeof( cg_layoutnode_t ) );
	node->type = type;
	node->integer = atoi( valuetok );
	node->value = atof( valuetok );
	node->string = CG_CopyString( token );
	node->func = NULL;
	node->ifthread = NULL;

	// return it
	return node;
}

/*
* CG_LayoutCathegorizeToken
*/
static int CG_LayoutCathegorizeToken( char *token ) {
	int i = 0;

	for( i = 0; cg_LayoutCommands[i].name; i++ ) {
		if( !Q_stricmp( token, cg_LayoutCommands[i].name ) ) {
			return LNODE_COMMAND;
		}
	}

	if( token[0] == '%' ) { // it's a numerical reference
		return LNODE_REFERENCE_NUMERIC;
	} else if( token[0] == '#' ) {   // it's a numerical constant
		return LNODE_NUMERIC;
	} else if( token[0] < '0' && token[0] > '9' && token[0] != '.' ) {
		return LNODE_STRING;
	}

	return LNODE_NUMERIC;
}

/*
* CG_RecurseFreeLayoutThread
* recursive for freeing "if" subtrees
*/
static void CG_RecurseFreeLayoutThread( cg_layoutnode_t *rootnode ) {
	cg_layoutnode_t *node;

	if( !rootnode ) {
		return;
	}

	while( rootnode ) {
		node = rootnode;
		rootnode = rootnode->parent;

		if( node->ifthread ) {
			CG_RecurseFreeLayoutThread( node->ifthread );
		}

		if( node->string ) {
			CG_Free( node->string );
		}

		CG_Free( node );
	}
}

/*
* CG_LayoutFixCommasInToken
* commas are accepted in the scripts. They actually do nothing, but are good for readability
*/
static bool CG_LayoutFixCommasInToken( char **ptr, char **backptr ) {
	char *token;
	char *back;
	int offset, count;
	bool stepback = false;

	token = *ptr;
	back = *backptr;

	if( !token || !strlen( token ) ) {
		return false;
	}

	// check that sizes match (quotes are removed from tokens)
	offset = count = strlen( token );
	back = *backptr;
	while( count-- ) {
		if( *back == '"' ) {
			count++;
			offset++;
		}
		back--;
	}

	back = *backptr - offset;
	while( offset ) {
		if( *back == '"' ) {
			offset--;
			back++;
			continue;
		}

		if( *token != *back ) {
			CG_Printf( "Token and Back mismatch %c - %c\n", *token, *back );
		}

		if( *back == ',' ) {
			*back = ' ';
			stepback = true;
		}

		offset--;
		token++;
		back++;
	}

	return stepback;
}

/*
* CG_RecurseParseLayoutScript
* recursive for generating "if" subtrees
*/
static cg_layoutnode_t *CG_RecurseParseLayoutScript( char **ptr, int level ) {
	cg_layoutnode_t *command = NULL;
	cg_layoutnode_t *argumentnode = NULL;
	cg_layoutnode_t *node = NULL;
	cg_layoutnode_t *rootnode = NULL;
	int expecArgs = 0, numArgs = 0;
	int token_type;
	bool add;
	char *token, *s_tokenback;

	if( !ptr ) {
		return NULL;
	}

	if( !*ptr || !*ptr[0] ) {
		return NULL;
	}

	while( *ptr ) {
		s_tokenback = *ptr;

		token = COM_Parse( ptr );
		while( *token == ' ' ) token++; // eat up whitespaces
		if( !Q_stricmp( ",", token ) ) {
			continue;                            // was just a comma
		}
		if( CG_LayoutFixCommasInToken( &token, ptr ) ) {
			*ptr = s_tokenback; // step back
			continue;
		}

		if( !*token ) {
			continue;
		}
		if( !strlen( token ) ) {
			continue;
		}

		add = false;
		token_type = CG_LayoutCathegorizeToken( token );

		// if it's an operator, we don't create a node, but add the operation to the last one
		if( CG_OperatorFuncForArgument( token ) != NULL ) {
			if( !node ) {
				CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" Operator hasn't any prior argument\n", level, token );
				continue;
			}
			if( node->type == LNODE_COMMAND || node->type == LNODE_STRING ) {
				CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" Operator was assigned to a command node\n", level, token );
			} else {
				expecArgs++; // we now expect one extra argument (not counting the operator one)

			}
			node->opFunc = CG_OperatorFuncForArgument( token );
			continue; // skip and continue
		}

		if( expecArgs > numArgs ) {
			// we are expecting an argument
			switch( token_type ) {
				case LNODE_NUMERIC:
				case LNODE_STRING:
				case LNODE_REFERENCE_NUMERIC:
					break;
				case LNODE_COMMAND:
				{
					CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" is not a valid argument for \"%s\"\n", level, token, command ? command->string : "" );
					continue;
				}
				break;
				default:
				{
					CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i) skip and continue: Unrecognized token \"%s\"\n", level, token );
					continue;
				}
				break;
			}
		} else {
			if( token_type != LNODE_COMMAND ) {
				// we are expecting a command
				CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): unrecognized command \"%s\"\n", level, token );
				continue;
			}

			// special case: endif commands interrupt the thread and are not saved
			if( !Q_stricmp( token, "endif" ) ) {
				//finish the last command properly
				if( command ) {
					command->integer = expecArgs;
				}
				return rootnode;
			}

			// special case: last command was "if", we create a new sub-thread and ignore the new command
			if( command && ( !Q_stricmp( command->string, "if" ) || !Q_stricmp( command->string, "ifnot" ) ) ) {
				*ptr = s_tokenback; // step back one token
				command->ifthread = CG_RecurseParseLayoutScript( ptr, level + 1 );
			}
		}

		// things look fine, proceed creating the node
		switch( token_type ) {
			case LNODE_NUMERIC:
			case LNODE_STRING:
			case LNODE_REFERENCE_NUMERIC:
			{
				node = CG_LayoutParseArgumentNode( token );
				if( !node ) {
					CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" is not a valid argument for \"%s\"\n", level, token, command ? command->string : "" );
					break;
				}
				numArgs++;
				add = true;
			}
			break;
			case LNODE_COMMAND:
			{
				node = CG_LayoutParseCommandNode( token );
				if( !node ) {
					CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): \"%s\" is not a valid command\n", level, token );
					break; // skip and continue
				}

				// expected arguments could have been extended by the operators
				if( command ) {
					command->integer = expecArgs;
				}

				// move on into the new command
				command = node;
				argumentnode = NULL;
				numArgs = 0;
				expecArgs = command->integer;
				add = true;
			}
			break;
			default:
				break;
		}

		if( add == true ) {
			if( command && command == rootnode ) {
				if( !argumentnode ) {
					argumentnode = node;
				}
			}

			if( rootnode ) {
				rootnode->next = node;
			}
			node->parent = rootnode;
			rootnode = node;
		}
	}

	if( level > 0 ) {
		CG_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): If without endif\n", level );
	}

	return rootnode;
}

/*
* CG_ParseLayoutScript
*/
static void CG_ParseLayoutScript( char *string, cg_layoutnode_t *rootnode ) {

	CG_RecurseFreeLayoutThread( cg.statusBar );
	cg.statusBar = CG_RecurseParseLayoutScript( &string, 0 );
}

//=============================================================================

//=============================================================================

/*
* CG_RecurseExecuteLayoutThread
* Execution works like this: First node (on backwards) is expected to be the command, followed by arguments nodes.
* we keep a pointer to the command and run the tree counting arguments until we reach the next command,
* then we call the command function sending the pointer to first argument and the pointer to the command.
* At return we advance one node (we stopped at last argument node) so it starts again from the next command (if any).
*
* When finding an "if" command with a subtree, we execute the "if" command. In the case it
* returns any value, we recurse execute the subtree
*/
static void CG_RecurseExecuteLayoutThread( cg_layoutnode_t *rootnode ) {
	cg_layoutnode_t *argumentnode = NULL;
	cg_layoutnode_t *commandnode = NULL;
	int numArguments;

	if( !rootnode ) {
		return;
	}

	// run until the real root
	commandnode = rootnode;
	while( commandnode->parent ) {
		commandnode = commandnode->parent;
	}

	// now run backwards up to the next command node
	while( commandnode ) {
		argumentnode = commandnode->next;

		// we could trust the parser, but I prefer counting the arguments here
		numArguments = 0;
		while( argumentnode ) {
			if( argumentnode->type == LNODE_COMMAND ) {
				break;
			}

			argumentnode = argumentnode->next;
			numArguments++;
		}

		// reset
		argumentnode = commandnode->next;

		// Execute the command node
		if( commandnode->integer != numArguments ) {
			CG_Printf( "ERROR: Layout command %s: invalid argument count (expecting %i, found %i)\n", commandnode->string, commandnode->integer, numArguments );
			return;
		}
		if( commandnode->func ) {
			//special case for if commands
			if( commandnode->func( argumentnode, numArguments ) ) {
				// execute the "if" thread when command returns a value
				if( commandnode->ifthread ) {
					CG_RecurseExecuteLayoutThread( commandnode->ifthread );
				}
			}
		}

		//move up to next command node
		commandnode = argumentnode;
		if( commandnode == rootnode ) {
			return;
		}

		while( commandnode && commandnode->type != LNODE_COMMAND ) {
			commandnode = commandnode->next;
		}
	}
}

/*
* CG_ExecuteLayoutProgram
*/
void CG_ExecuteLayoutProgram( struct cg_layoutnode_s *rootnode ) {
	ZoneScoped;
	CG_RecurseExecuteLayoutThread( rootnode );
}

//=============================================================================

//=============================================================================



/*
* CG_LoadHUDFile
*/

// Loads the HUD-file recursively. Recursive includes now supported
// Also processes "preload" statements for graphics pre-loading
#define HUD_MAX_LVL 16 // maximum levels of recursive file loading
static char *CG_LoadHUDFile( const char *path ) {
	char *rec_fn[HUD_MAX_LVL]; // Recursive filenames...
	char *rec_buf[HUD_MAX_LVL]; // Recursive file contents buffers
	char *rec_ptr[HUD_MAX_LVL]; // Recursive file position buffers
	char *token = NULL, *tmpbuf = NULL, *retbuf = NULL;
	int rec_lvl = 0, rec_plvl = -1;
	int retuse = 0, retlen = 0;
	int f, i, len;

	// Check if path is correct
	if( path == NULL ) {
		return NULL;
	}
	memset( rec_ptr, 0, sizeof( rec_ptr ) );
	memset( rec_buf, 0, sizeof( rec_buf ) );
	memset( rec_ptr, 0, sizeof( rec_ptr ) );

	// Copy the path of the file to the first recursive level filename :)
	rec_fn[rec_lvl] = ( char * )CG_Malloc( strlen( path ) + 1 );
	Q_strncpyz( rec_fn[rec_lvl], path, strlen( path ) + 1 );
	while( 1 ) {
		if( rec_lvl > rec_plvl ) {
			// We went a recursive level higher, our filename should have been filled in :)
			if( !rec_fn[rec_lvl] ) {
				rec_lvl--;
			} else if( rec_fn[rec_lvl][0] == '\0' ) {
				CG_Free( rec_fn[rec_lvl] );
				rec_fn[rec_lvl] = NULL;
				rec_lvl--;
			} else {
				// First check if this file hadn't been included already by one of the previous files
				// in the current file-stack to prevent problems :)
				for( i = 0; i < rec_lvl; i++ ) {
					if( !Q_stricmp( rec_fn[rec_lvl], rec_fn[i] ) ) {
						// Recursive file loading detected!!
						CG_Printf( "HUD: WARNING: Detected recursive file inclusion: %s\n", rec_fn[rec_lvl] );
						CG_Free( rec_fn[rec_lvl] );
						rec_fn[rec_lvl] = NULL;
					}
				}
			}

			// File was OK :)
			if( rec_fn[rec_lvl] != NULL ) {
				len = FS_FOpenFile( rec_fn[rec_lvl], &f, FS_READ );
				if( len > 0 ) {
					rec_plvl = rec_lvl;
					rec_buf[rec_lvl] = ( char * )CG_Malloc( len + 1 );
					rec_buf[rec_lvl][len] = '\0';
					rec_ptr[rec_lvl] = rec_buf[rec_lvl];

					// Now read the file
					if( FS_Read( rec_buf[rec_lvl], len, f ) <= 0 ) {
						if( rec_lvl > 0 ) {
							CG_Printf( "HUD: WARNING: Read error while loading file: %s\n", rec_fn[rec_lvl] );
						}
						CG_Free( rec_fn[rec_lvl] );
						CG_Free( rec_buf[rec_lvl] );
						rec_fn[rec_lvl] = NULL;
						rec_buf[rec_lvl] = NULL;
						rec_lvl--;
					}
					FS_FCloseFile( f );
				} else {
					if( !len ) {
						// File was empty - still have to close
						FS_FCloseFile( f );
					} else if( rec_lvl > 0 ) {
						CG_Printf( "HUD: WARNING: Could not include file: %s\n", rec_fn[rec_lvl] );
					}
					CG_Free( rec_fn[rec_lvl] );
					rec_fn[rec_lvl] = NULL;
					rec_lvl--;
				}
			} else {
				// Skip this file, go down one level
				rec_lvl--;
			}
			rec_plvl = rec_lvl;
		} else if( rec_lvl < rec_plvl ) {
			// Free previous level buffer
			if( rec_fn[rec_plvl] ) {
				CG_Free( rec_fn[rec_plvl] );
			}
			if( rec_buf[rec_plvl] ) {
				CG_Free( rec_buf[rec_plvl] );
			}
			rec_buf[rec_plvl] = NULL;
			rec_ptr[rec_plvl] = NULL;
			rec_fn[rec_plvl] = NULL;
			rec_plvl = rec_lvl;
			if( rec_lvl < 0 ) {
				// Break - end of recursive looping
				if( retbuf == NULL ) {
					CG_Printf( "HUD: ERROR: Could not load empty HUD-script: %s\n", path );
				}
				break;
			}
		}
		if( rec_lvl < 0 ) {
			break;
		}
		token = COM_ParseExt2( ( const char ** )&rec_ptr[rec_lvl], true, false );
		if( !Q_stricmp( "include", token ) ) {
			// Handle include
			token = COM_ParseExt2( ( const char ** )&rec_ptr[rec_lvl], false, false );
			if( ( ( rec_lvl + 1 ) < HUD_MAX_LVL ) && ( rec_ptr[rec_lvl] ) && ( token ) && ( token[0] != '\0' ) ) {
				// Go to next recursive level and prepare it's filename :)
				rec_lvl++;
				i = strlen( "huds/" ) + strlen( token ) + strlen( ".hud" ) + 1;
				rec_fn[rec_lvl] = ( char * )CG_Malloc( i );
				snprintf( rec_fn[rec_lvl], i, "huds/%s", token );
				COM_DefaultExtension( rec_fn[rec_lvl], ".hud", i );
				if( FS_FOpenFile( rec_fn[rec_lvl], NULL, FS_READ ) < 0 ) {
					// File doesn't exist!
					CG_Free( rec_fn[rec_lvl] );
					i = strlen( "huds/inc/" ) + strlen( token ) + strlen( ".hud" ) + 1;
					rec_fn[rec_lvl] = ( char * )CG_Malloc( i );
					snprintf( rec_fn[rec_lvl], i, "huds/inc/%s", token );
					COM_DefaultExtension( rec_fn[rec_lvl], ".hud", i );
					if( FS_FOpenFile( rec_fn[rec_lvl], NULL, FS_READ ) < 0 ) {
						CG_Free( rec_fn[rec_lvl] );
						rec_fn[rec_lvl] = NULL;
						rec_lvl--;
					}
				}
			}
		} else if( ( len = strlen( token ) ) > 0 ) {
			// Normal token, add to token-pool.
			if( ( retuse + len + 1 ) >= retlen ) {
				// Enlarge token buffer by 1kb
				retlen += 1024;
				tmpbuf = ( char * )CG_Malloc( retlen );
				if( retbuf ) {
					memcpy( tmpbuf, retbuf, retuse );
					CG_Free( retbuf );
				}
				retbuf = tmpbuf;
				retbuf[retuse] = '\0';
			}
			strcat( &retbuf[retuse], token );
			retuse += len;
			strcat( &retbuf[retuse], " " );
			retuse++;
			retbuf[retuse] = '\0';
		}

		// Detect "end-of-file" of included files and go down 1 level.
		if( ( rec_lvl <= rec_plvl ) && ( !rec_ptr[rec_lvl] ) ) {
			rec_plvl = rec_lvl;
			rec_lvl--;
		}
	}
	if( retbuf == NULL ) {
		CG_Printf( "HUD: ERROR: Could not load file: %s\n", path );
	}
	return retbuf;
}

static void CG_LoadHUD() {
	const char * path = "huds/default.hud";
	char * opt = CG_LoadHUDFile( path );
	if( opt == NULL ) {
		CG_Printf( "HUD: failed to load %s file\n", path );
		return;
	}

	// load the new status bar program
	CG_ParseLayoutScript( opt, cg.statusBar );

	// Free the opt buffer!
	CG_Free( opt );

	// set up layout font as default system font
	layout_cursor_font_style = FontStyle_Normal;
	layout_cursor_font_size = cgs.textSizeSmall;
}

void CG_InitHUD() {
	Cmd_AddCommand( "reloadhud", CG_LoadHUD );
	CG_LoadHUD();
}

void CG_ShutdownHUD() {
	CG_RecurseFreeLayoutThread( cg.statusBar );
	Cmd_RemoveCommand( "reloadhud" );
}
