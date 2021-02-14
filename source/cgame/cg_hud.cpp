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
#include "qcommon/string.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/text.h"
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
		case FontStyle_Normal: return cgs.fontNormal;
		case FontStyle_Bold: return cgs.fontNormalBold;
		case FontStyle_Italic: return cgs.fontNormalItalic;
		case FontStyle_BoldItalic: return cgs.fontNormalBoldItalic;
	}
	return NULL;
}

//=============================================================================

using opFunc_t = float( * )( const float a, float b );

struct cg_layoutnode_t {
	bool ( *func )( cg_layoutnode_t *argumentnode );
	int type;
	char *string;
	int num_args;
	size_t idx;
	float value;
	opFunc_t opFunc;
	cg_layoutnode_t *args;
	cg_layoutnode_t *next;
	cg_layoutnode_t *ifthread;
};

static cg_layoutnode_t * hud_root;

struct constant_numeric_t {
	const char *name;
	int value;
};

static const constant_numeric_t cg_numeric_constants[] = {
	{ "NOTSET", 9999 },

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
};

//=============================================================================

static int CG_Int( const void * p ) {
	return *( const int * ) p;
}

static int CG_U8( const void * p ) {
	return *( const u8 * ) p;
}

static int CG_S16( const void * p ) {
	return *( const s16 * ) p;
}

static int CG_Bool( const void * p ) {
	return *( const bool * ) p ? 1 : 0;
}

static int CG_GetPOVnum( const void *parameter ) {
	return cg.predictedPlayerState.POVnum != cgs.playerNum + 1 ? cg.predictedPlayerState.POVnum : 9999;
}

static int CG_IsTeamBased( const void *parameter ) {
	return GS_TeamBasedGametype( &client_gs ) ? 1 : 0;
}

static int CG_GetSpeed( const void *parameter ) {
	return Length( cg.predictedPlayerState.pmove.velocity.xy() );
}

static int CG_GetSpeedVertical( const void *parameter ) {
	return cg.predictedPlayerState.pmove.velocity.z;
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

static int CG_GetVidWidth( const void *parameter ) {
	return frame_static.viewport_width;
}

static int CG_GetVidHeight( const void *parameter ) {
	return frame_static.viewport_height;
}

static int CG_GetCvar( const void *parameter ) {
	return Cvar_Value( (const char *)parameter );
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

static int CG_GetScoreboardShown( const void *parameter ) {
	return CG_ScoreboardShown() ? 1 : 0;
}

struct reference_numeric_t {
	const char *name;
	int ( *func )( const void *parameter );
	const void *parameter;
};

static const reference_numeric_t cg_numeric_references[] = {
	// stats
	{ "HEALTH", CG_S16, &cg.predictedPlayerState.health },
	{ "WEAPON_ITEM", CG_U8, &cg.predictedPlayerState.weapon },

	{ "READY", CG_Bool, &cg.predictedPlayerState.ready },

	{ "TEAM", CG_Int, &cg.predictedPlayerState.team },

	{ "TEAMBASED", CG_IsTeamBased, NULL },
	{ "ALPHA_SCORE", CG_U8, &client_gs.gameState.bomb.alpha_score },
	{ "ALPHA_PLAYERS_ALIVE", CG_U8, &client_gs.gameState.bomb.alpha_players_alive },
	{ "ALPHA_PLAYERS_TOTAL", CG_U8, &client_gs.gameState.bomb.alpha_players_total },
	{ "BETA_SCORE", CG_U8, &client_gs.gameState.bomb.beta_score },
	{ "BETA_PLAYERS_ALIVE", CG_U8, &client_gs.gameState.bomb.beta_players_alive },
	{ "BETA_PLAYERS_TOTAL", CG_U8, &client_gs.gameState.bomb.beta_players_total },

	{ "PROGRESS", CG_U8, &cg.predictedPlayerState.progress },
	{ "PROGRESS_TYPE", CG_U8, &cg.predictedPlayerState.progress_type },

	{ "ROUND_TYPE", CG_U8, &client_gs.gameState.round_type },

	{ "CARRYING_BOMB", CG_Bool, &cg.predictedPlayerState.carrying_bomb },
	{ "CAN_PLANT_BOMB", CG_Bool, &cg.predictedPlayerState.can_plant },
	{ "CAN_CHANGE_LOADOUT", CG_Bool, &cg.predictedPlayerState.can_change_loadout },

	// other
	{ "CHASING", CG_GetPOVnum, NULL },
	{ "SPEED", CG_GetSpeed, NULL },
	{ "SPEED_VERTICAL", CG_GetSpeedVertical, NULL },
	{ "FPS", CG_GetFPS, NULL },
	{ "MATCH_STATE", CG_GetMatchState, NULL },
	{ "MATCH_DURATION", CG_GetMatchDuration, NULL },
	{ "PAUSED", CG_Paused, NULL },
	{ "VIDWIDTH", CG_GetVidWidth, NULL },
	{ "VIDHEIGHT", CG_GetVidHeight, NULL },
	{ "SCOREBOARD", CG_GetScoreboardShown, NULL },
	{ "DEMOPLAYING", CG_IsDemoPlaying, NULL },
	{ "CALLVOTE", CG_IsActiveCallvote, NULL },

	// cvars
	{ "SHOW_FPS", CG_GetCvar, "cg_showFPS" },
	{ "SHOW_POINTED_PLAYER", CG_GetCvar, "cg_showPointedPlayer" },
	{ "SHOW_SPEED", CG_GetCvar, "cg_showSpeed" },
	{ "SHOW_HOTKEYS", CG_GetCvar, "cg_showHotkeys" },

	{ "DOWNLOAD_IN_PROGRESS", CG_DownloadInProgress, NULL },
	{ "DOWNLOAD_PERCENT", CG_GetCvar, "cl_download_percent" },
};

//=============================================================================

#define MAX_OBITUARIES 32

enum obituary_type_t {
	OBITUARY_NONE,
	OBITUARY_NORMAL,
	OBITUARY_SUICIDE,
	OBITUARY_ACCIDENT,
};

struct obituary_t {
	obituary_type_t type;
	int64_t time;
	char victim[MAX_INFO_VALUE];
	int victim_team;
	char attacker[MAX_INFO_VALUE];
	int attacker_team;
	int mod;
};

static obituary_t cg_obituaries[MAX_OBITUARIES];
static int cg_obituaries_current = -1;

struct {
	s64 time;
	u64 entropy;
} self_obituary;

void CG_SC_ResetObituaries() {
	memset( cg_obituaries, 0, sizeof( cg_obituaries ) );
	cg_obituaries_current = -1;
	self_obituary = { };
}

static const char * obituaries[] = {
	"-19 INFECTED",
	"2.3'ED",
	"2077'ED",
	"ABOLISHED",
	"ABORTED",
	"ABUSED",
	"ACCELERATED",
	"ACCEPTED",
	"ADJUSTED",
	"ADMINISTERED",
	"ADMIRED",
	"ADOPTED",
	"ADVISED",
	"AFFECTED",
	"AIDED",
	"AIMED AT",
	"AIRED OUT",
	"ALIGNED",
	"ALLOCATED",
	"AMPLIFIED",
	"AMPUTATED",
	"ANALYZED",
	"ANNIHILATED",
	"ANNOTATED",
	"ANSWERED",
	"ANTICIPATED",
	"APPLIED",
	"APPOINTED",
	"APPRAISED",
	"APPROVED",
	"ARBITRATED",
	"ARRANGED",
	"ASSEMBLED",
	"ASSESSED",
	"ASSUMED",
	"ASSURED",
	"ATE",
	"ATOMIZED",
	"AUDITED",
	"AUTHORED",
	"AUTHORIZED",
	"AWARDED",
	"AXED",
	"BAKED",
	"BALANCED",
	"BALLED",
	"BANGED",
	"BANISHED",
	"BANKED",
	"BANNED",
	"BARFED ON",
	"BASHED",
	"BATHED",
	"BATTERED",
	"BBQED",
	"BEAMED",
	"BEAT",
	"BEAUTIFIED",
	"BELITTLED",
	"BENT OVER",
	"BENT",
	"BIGGED ON",
	"BINNED",
	"BIT",
	"BLASTED",
	"BLAZED",
	"BLED",
	"BLEMISHED",
	"BLENDED",
	"BLESSED",
	"BLEW UP",
	"BLEW",
	"BLINDSIDED",
	"BLOCKED",
	"BODYBAGGED",
	"BOILED",
	"BOMBED",
	"BONKED",
	"BOOSTED",
	"BOOTSTRAP",
	"BOUGHT",
	"BOUNCED",
	"BOUND",
	"BRED",
	"BRIEFED",
	"BROKE UP WITH",
	"BROKE",
	"BROWN BAGGED",
	"BRUTALIZED",
	"BUDGETED",
	"BUMMED",
	"BUMPED OFF",
	"BUMPED",
	"BURIED",
	"BURNED",
	"BUTTERED",
	"CAKED",
	"CALCULATED",
	"CANCELED",
	"CANNED",
	"CAPPED",
	"CARAMELIZED",
	"CARBONATED",
	"CARRIED",
	"CARVED",
	"CASHED OUT",
	"CELEBRATED",
	"CENTRALIZED",
	"CHALLENGED",
	"CHANGED",
	"CHARRED",
	"CHARTED",
	"CHEATED ON",
	"CHECKED OUT",
	"CHECKED",
	"CHECKMATED",
	"CHEESED",
	"CHEST",
	"CHEWED ON",
	"CHEWED",
	"CHILLED OFF",
	"CHOPPED",
	"CHOSE",
	"CLAIMED",
	"CLAPPED",
	"CLARIFIED",
	"CLEANED",
	"CLEANSED",
	"CLEARED",
	"CLIMBED",
	"CLOSED",
	"CLUBBED",
	"COACHED",
	"CODEXED",
	"COLLECTED",
	"COMBED",
	"COMBO-ED",
	"COMPACTED",
	"COMPILED",
	"COMPLETED",
	"COMPOSED",
	"COMPRESSED",
	"CONFIRMED",
	"CONNED",
	"CONSOLIDATED",
	"CONSULTED",
	"CONSUMED",
	"CONVERTED",
	"CONVINCED",
	"COOKED",
	"COORDINATED",
	"CORRECTED",
	"CORRUPTED",
	"COUNSELED",
	"CRACKED",
	"CRANKED",
	"CRASHED",
	"CREAMED",
	"CRIPPLED",
	"CRITICIZED",
	"CRITIQUED",
	"CROPPED",
	"CRUCIFIED",
	"CRUSHED",
	"CU IN 2 WEEKS'D",
	"CULLED",
	"CULTIVATED",
	"CURBED",
	"CURED",
	"CURTAILED",
	"CUSTOMIZED",
	"CUT OUT",
	"CUT",
	"DARED",
	"DARTED",
	"DEBUGGED",
	"DECAPITATED",
	"DECIMATED",
	"DECLINED",
	"DECONSTRUCTED",
	"DECREASED",
	"DECREMENTED",
	"DEDUCTED",
	"DEEP FRIED",
	"DEEPWATER HORIZONED",
	"DEFACED",
	"DEFAMED",
	"DEFEATED",
	"DEFINED",
	"DEFLATED",
	"DEFLOWERED",
	"DEFORMED",
	"DEGRADED",
	"DELEGATED",
	"DELETED",
	"DELIVERED",
	"DEMOLISHED",
	"DEMONSTRATED",
	"DEMOTED",
	"DENIED",
	"DEPOSITED",
	"DESTROYED",
	"DETACHED",
	"DEVASTATED",
	"DEVOURED",
	"DIAGNOSED",
	"DID",
	"DIGITALIZED",
	"DIMINISHED",
	"DIPPED",
	"DIRECTED",
	"DISABLED",
	"DISASSEMBLED",
	"DISCARDED",
	"DISCIPLINED",
	"DISCOVERED",
	"DISCREDITED",
	"DISFIGURED",
	"DISINTEGRATED",
	"DISJOINTED",
	"DISLIKED",
	"DISLODGED",
	"DISMANTLED",
	"DISMISSED",
	"DISPATCHED",
	"DISPERSED",
	"DISPLACED",
	"DISSOLVED",
	"DISTORTED",
	"DISTRIBUTED",
	"DISTURBED",
	"DIVORCED",
	"DOCUMENTED",
	"DONATED TO",
	"DONATED",
	"DOWNGRADED",
	"DOWNVOTED",
	"DRANK",
	"DRENCHED",
	"DRESSED",
	"DRIED",
	"DRILLED INTO",
	"DRILLED",
	"DROPPED",
	"DROVE",
	"DUMPED ON",
	"DUMPED",
	"DUMPSTERED",
	"DUNKED",
	"EARNED",
	"EARTHED",
	"EDGED",
	"EDITED",
	"EDUCATED",
	"EGGED",
	"EJECTED",
	"ELABORATED ON",
	"ELEVATED",
	"ELIMINATED",
	"EMANCIPATED",
	"EMBARRASSED",
	"EMBRACED",
	"ENABLED",
	"ENCODED",
	"ENDED",
	"ENFORCED",
	"ENHANCED",
	"ENLARGED",
	"ENSLAVED",
	"ENTERED",
	"EQUALIZED",
	"EQUINOX",
	"ERADICATED",
	"ERASED",
	"ESTABLISHED",
	"EVALUATED",
	"EVENED OUT",
	"EXAMINED",
	"EXCEEDED",
	"EXCHANGED",
	"EXECUTED",
	"EXERCISED",
	"EXONERATED",
	"EXPANDED",
	"EXPEDITED",
	"EXPELLED",
	"EXPLORED",
	"EXPORTED",
	"EXTERMINATED",
	"EXTINCTED",
	"EXTRACTED",
	"EXTRAPOLATED",
	"FACED",
	"FADED",
	"FARTED ON",
	"FED",
	"FELT UP",
	"FIELDED",
	"FILED",
	"FILLED",
	"FINALIZED",
	"FINANCED",
	"FINISHED",
	"FIRED",
	"FISTED",
	"FIXED",
	"FLAT EARTHED",
	"FLATTENED",
	"FLEXED ON",
	"FLIPPED",
	"FLUSHED",
	"FOLDED",
	"FOLLOWED",
	"FORCED",
	"FORCEPUSHED",
	"FORECASTED",
	"FORGED",
	"FORGOT",
	"FORKED",
	"FORMATTED",
	"FOUGHT",
	"FRAGGED",
	"FREED",
	"FRIED",
	"FROSTED",
	"FROZE",
	"FUCKED",
	"FURTHERED",
	"GAGGED",
	"GARFUNKELED",
	"GARGLED WITH",
	"GARGLED",
	"GEFICKT",
	"GERRYMANDERED",
	"GHOSTED",
	"GIBBED",
	"GLAZED",
	"GOT RID OF",
	"GOT",
	"GRATED",
	"GRAVEDUG",
	"GRILLED",
	"GUIDED",
	"GUTTED",
	"HACKED",
	"HANDLED",
	"HARASSED",
	"HARMED",
	"HARVESTED",
	"HATED ON",
	"HIRED",
	"HIT",
	"HOLED",
	"HOSED",
	"HUGGED",
	"HUMILIATED",
	"HUMPED",
	"HURLED",
	"HURT",
	"ICED",
	"IDENTIFIED",
	"IGNITED",
	"IGNORED",
	"ILLUSTRATED",
	"IMPAIRED",
	"IMPEACHED",
	"IMPLEMENTED",
	"IMPREGNATED",
	"IMPROVED",
	"INFLUENCED",
	"INFORMED",
	"INGESTED",
	"INJURED",
	"INSIDE JOBBED",
	"INSPECTED",
	"INSPIRED",
	"INSTALLED",
	"INSTRUCTED",
	"INTERPRETED",
	"INTRODUCED",
	"INVADED",
	"INVESTIGATED",
	"ITEMIZED",
	"JACKED",
	"JAMMED",
	"JERKED",
	"JUMPED",
	"KICKED",
	"KILLED",
	"KINDLED",
	"KISSED",
	"KNIT",
	"KNOCKED THE STUFFING OUT OF",
	"KNOCKED",
	"LAGGED",
	"LAID",
	"LAMBASTED",
	"LAMINATED",
	"LANDED ON",
	"LANDFILLED",
	"LARDED",
	"LAUNCHED",
	"LECTURED",
	"LEFT CLICKED ON",
	"LET GO",
	"LEVELLED",
	"LIFTED",
	"LIKED",
	"LIMITED",
	"LIQUIDATED",
	"LIT",
	"LIVESTREAMED",
	"LOGGED",
	"LOOPED",
	"LUGGED",
	"LYNCHED",
	"MANAGED",
	"MAPPED",
	"MARKETED",
	"MASHED",
	"MASSACRED",
	"MASSAGED",
	"MATED WITH",
	"MAULED",
	"MAXIMIZED",
	"MEASURED",
	"MELTED",
	"MENTORED",
	"MERGED",
	"MESSAGED",
	"MIXED",
	"MOBBED",
	"MODIFIED",
	"MONITORED",
	"MOONED",
	"MOONWALKED ON",
	"MOPPED UP",
	"MOTIVATED",
	"MOWED",
	"MUNCHED",
	"MURDERED",
	"MURKED",
	"MUTILATED",
	"NAILED",
	"NAVIGATED",
	"NEGLECTED",
	"NEUTRALIZED",
	"NEVER AGAINED",
	"NEXTED",
	"NOBODIED",
	"NUKED",
	"NULLIFIED",
	"NURTURED",
	"OBLITERATED",
	"OFFED",
	"ORGANIZED",
	"ORNAMENTED",
	"OUTPACED",
	"OUTPERFORMED",
	"OUTRAGED",
	"OVERHAULED",
	"OWNED",
	"PACKED",
	"PAID RESPECTS TO",
	"PAID",
	"PAINTED",
	"PASTEURIZED",
	"PATRONIZED",
	"PEELED",
	"PEGGED",
	"PENETRATED",
	"PERFORMED ON",
	"PERISHED",
	"PERSUADED",
	"PIED",
	"PILED",
	"PINCHED",
	"PIPED",
	"PISSED ON",
	"PLANTED",
	"PLASTERED",
	"PLAYED",
	"PLOWED", //:v_trump:
	"PLUCKED",
	"POACHED",
	"POKED",
	"POLARIZED",
	"POOLED",
	"POUNDED",
	"PREDICTED",
	"PROMOTED",
	"PUBLICIZED",
	"PUFFED",
	"PULLED",
	"PULVERIZED",
	"PUMMELLED",
	"PUMPED",
	"PUNCHED",
	"PURGED",
	"PUSHED",
	"PUT DOWN",
	"PYONGYANGED",
	"QUALIFIED",
	"QUASHED",
	"QUENCHED",
	"QUIT",
	"RAISED",
	"RAMMED",
	"RAZED",
	"READ",
	"REAMED",
	"REBASED",
	"REBUKED",
	"RECONCILED",
	"RECRUITED",
	"RECYCLED",
	"REDESIGNED",
	"REDUCED",
	"REFINED",
	"REFOCUSED",
	"REFORMED",
	"REFRIGERATED",
	"REFUSED",
	"REGULATED",
	"REHABILITATED",
	"REJECTED",
	"REKT",
	"RELEASED",
	"RELEGATED",
	"REMINDED",
	"REMODELED",
	"REMOVED",
	"RENDERED",
	"REORGANIZED",
	"REPAIRED",
	"REPLACED",
	"REPRIMANDED",
	"RESET",
	"RESOLVED",
	"RESPECTED",
	"RESTRICTED",
	"RESTRUCTURED",
	"RETURNED",
	"REVAMPED",
	"REVERSED INTO",
	"REVERSED",
	"REVIEWED",
	"REVISED",
	"RINSED",
	"ROASTED",
	"ROBBED",
	"RODE",
	"ROLLED",
	"ROOTED",
	"RUINED",
	"RULED",
	"RUNG",
	"SABOTAGED",
	"SACKED",
	"SAMPLED",
	"SANCTIONED",
	"SANDED",
	"SANK",
	"SATISFIED",
	"SAUCED",
	"SAVED",
	"SCARIFIED",
	"SCARRED",
	"SCATTERED",
	"SCISSORED",
	"SCORCHED",
	"SCREENED",
	"SCREWED",
	"SCRUBBED",
	"SCRUTINIZED",
	"SCUTTLED",
	"SECURED",
	"SELECTED",
	"SEPARATED",
	"SERVED",
	"SET",
	"SEVERED",
	"SHAFTED",
	"SHAGGED",
	"SHANKED",
	"SHAPED",
	"SHARED",
	"SHARPENED",
	"SHATTERED",
	"SHAVED",
	"SHED",
	"SHELVED",
	"SHIPPED",
	"SHITTED ON",
	"SHOOK",
	"SHOT",
	"SHOWCASED",
	"SHRUNK",
	"SHUFFLED",
	"SHUT",
	"SIMBA'D",
	"SIMPLIFIED",
	"SKEWERED",
	"SKIMMED",
	"SLABBED",
	"SLAMMED",
	"SLAPPED",
	"SLATED",
	"SLAUGHTERED",
	"SLICED",
	"SMACKED",
	"SMASHED",
	"SMEARED",
	"SMOKED",
	"SNAKED",
	"SNAPPED",
	"SNITCHED ON",
	"SNOOPED",
	"SNUBBED",
	"SOCKED",
	"SOILED",
	"SOLD",
	"SPANKED",
	"SPARKLED",
	"SPAYED",
	"SPLASHED",
	"SPLATTERED",
	"SPLIT",
	"SPOILED",
	"SPRAYED",
	"SPREAD",
	"SPRINKLED",
	"SQUASHED",
	"SQUEEZED",
	"STABBED",
	"STACKED",
	"STAMPED OUT",
	"STANDARDIZED",
	"STASHED",
	"STEAMED",
	"STEAMROLLED",
	"STEPPED ON",
	"STEPPED",
	"STERILIZED",
	"STIFFED",
	"STIFLED",
	"STIMULATED",
	"STIR-FRIED",
	"STIRRED",
	"STOMPED",
	"STONED",
	"STOPPED",
	"STREAMED",
	"STREAMLINED",
	"STRETCHED",
	"STRUCK OUT",
	"STRUCK",
	"STUCK IT TO",
	"STUFFED",
	"STUNG",
	"STYLED ON",
	"SUBDUCTED",
	"SUBMITTED",
	"SUBSCRIBED TO",
	"SUNK",
	"SUPERVISED",
	"SUPPLIED",
	"SURPASSED",
	"SURVEYED",
	"SUSPENDED",
	"SWALLOWED",
	"SWAMPED",
	"SWEPT",
	"SWIPED LEFT ON",
	"SWIPED",
	"SWIRLED",
	"SYNTHETIZED",
	"TAPED",
	"TAPPED",
	"TARGETED",
	"TARNISHED",
	"TASTED",
	"TATTOOED",
	"TAUGHT",
	"TERMINATED",
	"TESTED",
	"THREW",
	"TIMED OUT",
	"TOILET STORED",
	"TOOK DOWN",
	"TOOK OUT",
	"TOPPLED",
	"TORE DOWN",
	"TORE",
	"TORPEDOED",
	"TOSSED",
	"TRACKED",
	"TRAINED",
	"TRANSFORMED",
	"TRANSLATED",
	"TRAPPED",
	"TRASHED",
	"TRIED",
	"TRIMMED",
	"TRIPPED",
	"TROLLED",
	"TRUMPED",
	"TRUNCATED",
	"TWEETED",
	"TWIRLED",
	"TWISTED",
	"UNFOLLOWED",
	"UNIFIED",
	"UNINSTALLED",
	"UNLOCKED",
	"UNSUBSCRIBED FROM",
	"UNTANGLED",
	"UPDATED",
	"UPPERCASED",
	"UPSET",
	"VACCINATED",
	"VACUUMED",
	"VANDALIZED",
	"VAPED",
	"VAPORIZED",
	"VERIFIED",
	"VIOLATED",
	"VULKANIZED",
	"WASHED",
	"WASTED",
	"WAXED",
	"WEEDED OUT",
	"WHACKED",
	"WHIPPED",
	"WHISKED",
	"WIPED OUT",
	"WITHDREW",
	"WITNESSED",
	"WOKE UP",
	"WORE DOWN",
	"WORE OUT",
	"WRECKED",
	"YEETED",
	"YIELDED",
};

static const char * prefixes[] = {
	"#",
	"1080",
	"2 FAST 2 ",
	"360",
	"420",
	"666",
	"69",
	"A$AP ",
	"ABSOLUTE",
	"ABSTRACT",
	"ABU BAKR AL-",
	"AESTHETIC",
	"AFRICA",
	"AIR",
	"ALIEN",
	"ALL-STAR ",
	"ALPHA",
	"AMATEUR",
	"AMERICA",
	"ANGER",
	"ANTI",
	"ANUS",
	"APE",
	"ARBITRAGE",
	"ARBORETUM",
	"AREA51",
	"ART & ",
	"ART",
	"ASIA",
	"ASS",
	"ASTRO",
	"AUSTRALIA",
	"AUTO",
	"AVID",
	"BACK",
	"BACKWARDS",
	"BACTERIA",
	"BADASS",
	"BAG",
	"BAHAMAS",
	"BAILOUT",
	"BALL",
	"BALLER",
	"BAM",
	"BANANA",
	"BANK",
	"BARBIE",
	"BARSTOOL",
	"BASEMENT",
	"BASS",
	"BASTARD",
	"BATTY BOY ",
	"BAY-RUM ",
	"BBQ",
	"BEAST",
	"BEAUTY",
	"BEAVIS & BUTT-",
	"BEEFCAKE",
	"BELIEF",
	"BELLY",
	"BENICIO DEL ",
	"BETA",
	"BEYONCE",
	"BILLY RAY ",
	"BINARY",
	"BINGE",
	"BISCUIT",
	"BIT",
	"BITMAP",
	"BLACKLIST",
	"BLACKOUT",
	"BLADDER",
	"BLAST",
	"BLASTER",
	"BLOOD",
	"BLUE",
	"BLUNT",
	"BODY",
	"BOLLYWOOD",
	"BOMB",
	"BONE",
	"BONUS",
	"BOOB",
	"BOOM",
	"BOOMER",
	"BOOST",
	"BOOT",
	"BOOTY",
	"BOOYA",
	"BOREDOM",
	"BORIS",
	"BOTTOM",
	"BOX OFFICE ",
	"BOY",
	"BRAIN",
	"BRASS",
	"BREXIT",
	"BRICK",
	"BRIDGE",
	"BRO",
	"BROWNOUT",
	"BRUTAL",
	"BRUTALLY ",
	"BRUTE",
	"BUFF",
	"BUKKAKE",
	"BULG",
	"BUM",
	"BUSINESS",
	"BUSTA",
	"BUTTER",
	"CABIN",
	"CAMP",
	"CAN",
	"CANCER",
	"CANDY",
	"CANISTER",
	"CANNON",
	"CAP",
	"CAPITAL",
	"CAPITALISM",
	"CARBONARA",
	"CARDINAL",
	"CARE",
	"CAREFULLY ",
	"CASH",
	"CASINO",
	"CASSEROLE",
	"CAT",
	"CATEGORY",
	"CEO ",
	"CHAIN",
	"CHAINMAILFACE",
	"CHAKRA",
	"CHAMPION",
	"CHAOS",
	"CHARA-"
	"CHASSEUR DE ",
	"CHATUR",
	"CHEAP",
	"CHECK",
	"CHEDDAR",
	"CHEER",
	"CHEERFULLY ",
	"CHEERS",
	"CHEESE",
	"CHERRY",
	"CHICKEN",
	"CHILL",
	"CHIN",
	"CHINA",
	"CHOICE",
	"CHOPPER",
	"CIPHER",
	"CIRCLE",
	"CLASH",
	"CLASS",
	"CLEAN",
	"CLEVERLY ",
	"CLIMATE",
	"CLIT",
	"CLITORIS",
	"CLOUD",
	"CLOWN",
	"CLUTCH",
	"COASTER",
	"COCAINE",
	"COCK",
	"CODE",
	"CODEX",
	"COKE",
	"COLD",
	"COLLUSION",
	"COLOSSAL",
	"COMBO",
	"COMIC SANS ",
	"COMMERCIAL",
	"CON",
	"CONCEPT",
	"CONCRETE",
	"CONDOM",
	"CONJECTURE",
	"CONSCIENCE",
	"CONSENT",
	"CONSTITUTION",
	"CONTACTLESS",
	"COOKIE",
	"CORN",
	"CORONA",
	"COUNTER",
	"COVENANT",
	"COVID-",
	"COW",
	"COWABUNGA",
	"CRAB",
	"CRACK",
	"CRATER",
	"CRAZY",
	"CREAM",
	"CREW",
	"CRINGE",
	"CRUELLY ",
	"CRUNK",
	"CRYPTO",
	"CTRL + ALT + ",
	"CU@",
	"CUCKOLD",
	"CULTURE",
	"CUM",
	"CUMIN",
	"CUNT",
	"CURVE",
	"CUTE",
	"CYBER",
	"DAMP",
	"DANK",
	"DARK",
	"DARKWEB",
	"DART",
	"DAY",
	"DEBUG",
	"DEEP",
	"DEEPFAKE",
	"DEEPLY ",
	"DEFIANTLY ",
	"DEFICIT",
	"DEMOCRACY",
	"DEMON",
	"DENIAL",
	"DENTAL",
	"DEPRESSION",
	"DEUCE",
	"DEVIL",
	"DHARMA",
	"DIABOTICAL",
	"DICK",
	"DIESEL",
	"DILLY",
	"DIME",
	"DIP",
	"DIRECT",
	"DIRT",
	"DISCO",
	"DISGUST",
	"DISS",
	"DISTANCING",
	"DISTRUST",
	"DOG",
	"DOGMA",
	"DOME",
	"DONG",
	"DONKEY",
	"DOUBLE D ",
	"DOUBLE DIP ",
	"DOUBLE",
	"DOWNSTAIRS",
	"DRAFT",
	"DRAGON",
	"DRAGOSTEA DIN ",
	"DREAM",
	"DRILL",
	"DRONE",
	"DRUG",
	"DUKE",
	"DULCE DE ",
	"DUMB",
	"DUMP",
	"DUMPLING",
	"DUNK",
	"DUPE",
	"DUTY",
	"DWARF",
	"DWELL",
	"DYNAMITE",
	"EARLY",
	"EASILY ",
	"EASY",
	"EBOLA",
	"ECONOMY",
	"ECSTASY",
	"EDGAR ALLAN ",
	"EDIT",
	"EFFORT",
	"EJACULATE",
	"ELECTRO",
	"ELEGANCE",
	"ELITE",
	"ELON",
	"EMBRYO",
	"ENERGY",
	"ENVIRONMENT",
	"EPIPHANY",
	"EPSTEIN",
	"ETERNITY",
	"ETHERNET",
	"ETHIC",
	"EXA",
	"EXCELLENCE",
	"EXISTENCE",
	"EXPELIARMUS",
	"EXPERIENCE",
	"EXPERT",
	"EXPERTLY",
	"EXPLOSION",
	"EXTRA",
	"EYE",
	"FACE",
	"FACT CHECK ",
	"FACT",
	"FADE",
	"FAKE NEWS ",
	"FAKE",
	"FAKIE",
	"FAME",
	"FAMILY",
	"FANTASY",
	"FART",
	"FAST",
	"FAT",
	"FATALLY ",
	"FAXX",
	"FESTIVAL",
	"FETUS",
	"FIDELITY",
	"FIELD",
	"FIEND",
	"FILIBUSTER",
	"FINGER",
	"FIRE",
	"FIRST BAPTIST CHURCH OF ",
	"FISH",
	"FIST",
	"FLAKE",
	"FLAT",
	"FLAVA",
	"FLEX",
	"FLUID",
	"FLY",
	"FO' SHO' ",
	"FOOL",
	"FOOT",
	"FORCE",
	"FRAME",
	"FRANKLIN D. ",
	"FRAUD",
	"FREE WILL ",
	"FREEDOM",
	"FREEMIUM",
	"FREESTYLE",
	"FRENCH",
	"FRENULUM",
	"FRESH",
	"FRIENDSHIP",
	"FROG",
	"FRONT",
	"FROST",
	"FRUITY",
	"FUCK",
	"FUDGE",
	"FULL METAL ",
	"FULL",
	"FUNK",
	"FURLOUGH",
	"FURY",
	"FUTURE OF ",
	"G-",
	"GALAXY",
	"GALILEO GALI",
	"GAME",
	"GANGAM",
	"GANGSTA",
	"GARBAGE",
	"GARGOYLE",
	"GARLIC",
	"GAS",
	"GEEK",
	"GENDER",
	"GEORGE R. R. ",
	"GEORGE W. ",
	"GERRYMANDER",
	"GHETTO",
	"GHOST",
	"GIANT",
	"GIFT",
	"GIGA",
	"GIGI D'",
	"GIMP",
	"GINGER",
	"GIRL",
	"GIT ",
	"GLOW UP",
	"GNU/",
	"GOAFER",
	"GOB",
	"GOBLIN",
	"GOD",
	"GOLD",
	"GOOCHIE",
	"GOOD",
	"GOOF",
	"GOOSE",
	"GORE",
	"GORILLA",
	"GRASS",
	"GRAVE",
	"GRAVITY",
	"GREMLIN",
	"GRETA",
	"GRID",
	"GRILL",
	"GRIME",
	"GRIND",
	"GRITTY",
	"GRUNT",
	"GULLY",
	"GUT",
	"GYM",
	"GYMBRO",
	"H1N1",
	"HACK",
	"HALL OF FAME ",
	"HAM",
	"HAMMER",
	"HAND",
	"HAPPINESS",
	"HAPPY",
	"HARAMBE",
	"HARD",
	"HARDCORE",
	"HARMONY",
	"HASHTAG",
	"HAWK",
	"HEAD",
	"HEADWIND",
	"HEART",
	"HELL",
	"HELLO. BEEP. BOOP. ",
	"HERD",
	"HERO",
	"HERPES",
	"HOLE",
	"HOLLA",
	"HOLLYWOOD",
	"HOLY",
	"HOME",
	"HOMER J. ",
	"HOMOEROTICALLY ",
	"HOOD",
	"HOOLIGAN",
	"HORMONE",
	"HORROR",
	"HTTPS://",
	"HURENSOHN",
	"HURRICANE",
	"HUSKY",
	"HUSTLE",
	"HUSTLER",
	"HYBRID",
	"HYPE",
	"HYPER",
	"ICE",
	"IDEA",
	"IDENTITY",
	"IDEOLOGY",
	"IGNORANCE",
	"ILL",
	"IMMUNE",
	"IMP",
	"IMPACT",
	"INCEL",
	"INDUSTRIAL",
	"INFINITY",
	"INFORMATION",
	"INFRARED",
	"INSECT",
	"INSIDE",
	"INSTA",
	"INTUITION",
	"IRON",
	"IRONY",
	"J. R. R. ",
	"JAIL",
	"JAM",
	"JAW",
	"JEAN-CLAUDE VAN ",
	"JELLY",
	"JET",
	"JEYUK",
	"JIMMY",
	"JOHANN WOLFGANG VON ",
	"JOHN F. ",
	"JOKE",
	"JOKER",
	"JOLLY",
	"JOYFULLY ",
	"JUDGEMENT",
	"JUDO",
	"JUICE",
	"JUMBO",
	"JUSTICE",
	"KAMASUTRA",
	"KANYE",
	"KARATE",
	"KAREEM ABDUL-JA",
	"KAREN",
	"KARMA",
	"KEPT CALM AND ",
	"KICK",
	"KICKFLIP",
	"KILO",
	"KIM IL-",
	"KIM JONG-",
	"KIND",
	"KINDLY ",
	"KING",
	"KITCHEN",
	"KNEE",
	"KNOB",
	"KNOWLEDGE",
	"KONY2012",
	"KRACH",
	"KRUNK",
	"LAD",
	"LARGE HADRON ",
	"LASER",
	"LATE",
	"LATIN AMERICA",
	"LEMMING",
	"LEMON",
	"LEONARDO DA ",
	"LEVERAGE",
	"LIBROCKET",
	"LICK",
	"LIFE",
	"LIL'",
	"LITE",
	"LITERALLY ",
	"LOCKDOWN",
	"LOGIC",
	"LONG",
	"LORD",
	"LOTION",
	"LOVE",
	"LOWKEY",
	"LOYALTY",
	"LUBE",
	"LUDWIG II OF ",
	"LUDWIG VAN",
	"LaTeX",
	"MAC",
	"MACGYVER",
	"MACH 2",
	"MACH 3",
	"MACHINE",
	"MAD SOBBING CHILD ",
	"MAD",
	"MADLY ",
	"MAGIC",
	"MAIL-IN ",
	"MALARKEY",
	"MALLET",
	"MALWARE",
	"MAMMOTH",
	"MAN",
	"MANNEQUIN",
	"MANTRA",
	"MAO TSE-",
	"MAR-A-",
	"MARTIN LUTHER ",
	"MARX",
	"MASCULINITY",
	"MASK",
	"MASTER",
	"MATH",
	"MATTER",
	"MAY HAVE ",
	"MEAT",
	"MEGA",
	"MEMBER",
	"MEME",
	"MERCY",
	"MERGE",
	"MERRILY ",
	"META",
	"METAL",
	"MICROAGGRESSION",
	"MIDGET",
	"MIKE",
	"MILDLY ",
	"MILLENNIUM",
	"MIND",
	"MINORITY",
	"MINT",
	"MISSILE",
	"MOBILE",
	"MOIST",
	"MOLD",
	"MOLLY",
	"MONEY",
	"MONGOL",
	"MONKEY",
	"MONSTER",
	"MONUMENTAL",
	"MOOD",
	"MOSCOW",
	"MOST FROLICKINGLY ",
	"MOST PROBABLY ",
	"MOTION",
	"MOUTH",
	"MOVIE",
	"MULE",
	"MUMMY",
	"MURICA",
	"MVP ",
	"MWAGA",
	"Mc",
	"NAME",
	"NAS NE ",
	"NASTY",
	"NAT KING ",
	"NATURE",
	"NAY",
	"NEAT",
	"NEATLY ",
	"NECKBEARD",
	"NEKO",
	"NERD",
	"NETFLIX & ",
	"NEVERLAND",
	"NICELY ",
	"NIGHT",
	"NIGHTMARE",
	"NINJA",
	"NON-BINARY ",
	"NORMIE",
	"NOTION",
	"NOVEL CORONA",
	"NOVICE",
	"NUKE",
	"NUT",
	"O-",
	"OBAMA",
	"OBANI",
	"OBSTRUCTION",
	"OFF",
	"OLDSCHOOL",
	"OMEGA",
	"ON-BRAND ",
	"ONE-NIGHT-",
	"OPEN-SOURCE ",
	"OPIOID",
	"ORGASM",
	"OSAMA BIN-",
	"OTTO VON ",
	"OUT",
	"OUTSIDE",
	"OVER",
	"OVERDRIVE",
	"P2",
	"PAD",
	"PANDEMIC",
	"PANTY",
	"PAPRIKASH",
	"PARADOX",
	"PARTICLE",
	"PARTY",
	"PASSION",
	"PEACE",
	"PEAK",
	"PECKER",
	"PENIS",
	"PEOPLE'S ",
	"PERFECTLY ",
	"PERFORMANCE",
	"PERIOD",
	"PERMA",
	"PETA",
	"PHANTOM",
	"PHENO",
	"PHOTO",
	"PIECE",
	"PIG",
	"PILE",
	"PIMP",
	"PINKY",
	"PIPE",
	"PISS",
	"PISTACCHIO",
	"PIZZA",
	"PLANTER",
	"PLATINUM",
	"PLAYBOY",
	"POLONIUM",
	"PONY",
	"POODLE",
	"POOP",
	"POOR",
	"POPE",
	"PORTFOLIO",
	"POWER",
	"PRANK",
	"PREDATOR",
	"PREMIUM",
	"PRESIDENT",
	"PRICE",
	"PRICK",
	"PRIME",
	"PRINCE",
	"PRINCIPLE",
	"PRISON",
	"PRIZE",
	"PROBABLY",
	"PRONG",
	"PRONOUN",
	"PROPERLY",
	"PROSTATE",
	"PROUD",
	"PUFF",
	"PUMPKIN",
	"PUNANI",
	"PUSSY",
	"QFUSION",
	"QUAKE",
	"QUALITY",
	"QUANTITY",
	"QUARANTINE",
	"QUEEF",
	"QUICK",
	"RAGE",
	"RAID",
	"RAKE",
	"RAM",
	"RAMBO",
	"RAPIDLY ",
	"RAPTOR",
	"RATIONALE",
	"RAVE",
	"RAZOR",
	"RE: ",
	"REASON",
	"REFORM",
	"REGEX",
	"RELATIONSHIP",
	"RESPECT",
	"RETRO",
	"RIGHT",
	"RING",
	"ROAST",
	"ROBERT E. ",
	"ROBOT",
	"ROCK 'n' ROLL ",
	"ROCK",
	"ROFL",
	"ROLEX",
	"ROOSTER",
	"ROUGH",
	"ROUGHLY ",
	"ROYAL",
	"RUDOLPH THE RED-",
	"RUSH",
	"RUSSIA",
	"RUTHLESSLY ",
	"Rn",
	"SACHA BARON ",
	"SACK",
	"SAD",
	"SAFE SPACE",
	"SALAD",
	"SALMON",
	"SALT",
	"SAMUEL L. ",
	"SAND",
	"SANDWICH",
	"SARS-CoV-",
	"SASSY",
	"SAUCE",
	"SAUD BIN ",
	"SAUNA",
	"SAUSAGE",
	"SCAM",
	"SCHADENFREUDE",
	"SCISSOR",
	"SCREW",
	"SCROOGE Mc",
	"SCRUB",
	"SEAGULL",
	"SECT",
	"SEMEN",
	"SERIOUSLY ",
	"SEX",
	"SHADE",
	"SHAKE",
	"SHAME",
	"SHANK",
	"SHAREHOLDER",
	"SHART",
	"SHEEP",
	"SHIBA",
	"SHIT",
	"SHOOK",
	"SHORT",
	"SHOWER",
	"SIBAL",
	"SIDE",
	"SILLY",
	"SIMP",
	"SIN",
	"SIR ISAAC ",
	"SKELETON",
	"SKELETOR",
	"SKILL",
	"SKIN",
	"SKULL",
	"SLAM",
	"SLAVE",
	"SLAY",
	"SLEEP",
	"SLICE",
	"SLOW",
	"SLUT",
	"SMACK",
	"SMACKDOWN",
	"SMELL",
	"SMELLY",
	"SMILE",
	"SMOKE",
	"SMOOTH",
	"SMOOTHLY ",
	"SNAP",
	"SNICKER",
	"SNIFFLE",
	"SNOWFLAKE",
	"SNUFF",
	"SOAK",
	"SOCIALISM",
	"SOCIETY",
	"SOCK",
	"SOFT",
	"SOFTCORE",
	"SOFTLY ",
	"SOLDIER",
	"SOLOMONK",
	"SON",
	"SOUP",
	"SOUR",
	"SPACE",
	"SPEED",
	"SPICE",
	"SPIRIT",
	"SPORTSMANSHIP",
	"SPRAY",
	"SQUIRT",
	"STACK",
	"STALIN",
	"STAN",
	"STAR",
	"STEAM",
	"STEEL",
	"STEEZ",
	"STEROID",
	"STIMULUS",
	"STOMP",
	"STONE",
	"STOVEPIPE",
	"STREAM",
	"STREET",
	"STYLE",
	"SUBLIME",
	"SUBSTANCE",
	"SUDO ",
	"SUGAR",
	"SUGAWARA NO ",
	"SULTAN",
	"SUPER",
	"SUPER-SPREADER ",
	"SUPERSTAR",
	"SURELY ",
	"SURGERY",
	"SWAMP",
	"SWEETHEART",
	"SWINDLE",
	"SWOLE",
	"SYMBOL",
	"SYNDROME",
	"T-BONE ",
	"TABLE",
	"TAG",
	"TAKEOUT",
	"TANTRA",
	"TASTE",
	"TEARGAS",
	"TEMPLE",
	"TENDER",
	"TENDERLY ",
	"TENSION",
	"TERA",
	"TERATOMA",
	"TERRY A. ",
	"TERROR",
	"TESLA",
	"THIRST",
	"THOROUGHLY ",
	"TIGHT",
	"TIME AND SPACE",
	"TIME",
	"TINDER",
	"TIT",
	"TITAN",
	"TITTY",
	"TOILET",
	"TOKYO",
	"TOMBSTONE ",
	"TOOTH",
	"TOP",
	"TOTAL",
	"TOY",
	"TRADE",
	"TRAP",
	"TRASH",
	"TREASON",
	"TRICKSTER",
	"TRICKY",
	"TRIPLE",
	"TROLL",
	"TRULY ",
	"TRUMP",
	"TRUST",
	"TRUTH",
	"TURBO",
	"TURKEY",
	"TUTANKHAMUN",
	"TWIST",
	"TYPE",
	"UBER",
	"UGLY",
	"ULTIMATE",
	"ULTRA",
	"ULTRASOUND",
	"UNDER",
	"UNICODE",
	"UNITED STATES OF ",
	"UNIVERSAL",
	"UNIVERSE",
	"UNIVERSITY",
	"UNIX",
	"UNREAL",
	"USS ",
	"UTF-",
	"UTTERLY ",
	"VAGINA",
	"VALUE",
	"VASECTOMY",
	"VASTLY",
	"VEGAN",
	"VETERAN",
	"VIDEO",
	"VINCENT VAN ",
	"VIOLENTLY ",
	"VIRGIN",
	"VIRTUE",
	"VIRUS",
	"VITAMIN",
	"VOLATILITY",
	"VULTURE",
	"VULVA",
	"WA GWAAN ",
	"WACK",
	"WAFFLE",
	"WANG",
	"WAREZ",
	"WASTE",
	"WAX",
	"WEENIE",
	"WEINER",
	"WEINSTEIN",
	"WET",
	"WHIP",
	"WHISTLEBLOWER",
	"WHITE",
	"WHORE",
	"WICKED",
	"WICKEDLY ",
	"WIDLY ",
	"WIFE",
	"WILD",
	"WILDFIRE",
	"WILLY",
	"WINDMILL",
	"WINNIE THE ",
	"WISDOM",
	"WISHFULLY ",
	"WOKE",
	"WOLFGANG AMADEUS ",
	"WONDER",
	"WORLD CUP ",
	"WRECK",
	"WRONG",
	"X AE A-12 ",
	"X-RAY ",
	"XI JIN",
	"XXX",
	"YAY",
	"YEET",
	"YIELD",
	"YOGA",
	"YOTTA",
	"ZEALOT",
	"ZEITGEIST",
	"ZETTA",
	"ZIP",
	"ZOMBIE",
	"ZOOM",
	"ZOOMER",
};

static const char * conjunctions[] = {
	"+",
	"&",
	"&&",
	"ALONG WITH",
	"AND",
	"AS WELL AS",
	"ASSISTED BY",
	"FEAT.",
	"N'",
	"PLUS",
	"UND",
	"W/",
	"WITH",
	"X",
};

static const char * RandomObituary( RNG * rng ) {
	return random_select( rng, obituaries );
}

static const char * RandomPrefix( RNG * rng, float p ) {
	if( !random_p( rng, p ) )
		return "";
	return random_select( rng, prefixes );
}

static const char * RandomAssistConjunction( RNG * rng ) {
	return random_select( rng, conjunctions );
}

void CG_SC_Obituary() {
	int victimNum = atoi( Cmd_Argv( 1 ) );
	int attackerNum = atoi( Cmd_Argv( 2 ) );
	int topAssistorNum = atoi( Cmd_Argv( 3 ) );
	int mod = atoi( Cmd_Argv( 4 ) );
	u64 entropy = strtonum( Cmd_Argv( 5 ), S64_MIN, S64_MAX, NULL );

	const cg_clientInfo_t * victim = &cgs.clientInfo[ victimNum - 1 ];
	const cg_clientInfo_t * attacker = attackerNum == 0 ? NULL : &cgs.clientInfo[ attackerNum - 1 ];
	const cg_clientInfo_t * assistor = topAssistorNum == -1 ? NULL : &cgs.clientInfo[ topAssistorNum - 1 ];
	cg_obituaries_current = ( cg_obituaries_current + 1 ) % MAX_OBITUARIES;
	obituary_t * current = &cg_obituaries[cg_obituaries_current];

	RNG rng = new_rng( entropy, 0 );

	current->time = cls.monotonicTime;
	if( victim != NULL ) {
		Q_strncpyz( current->victim, victim->name, sizeof( current->victim ) );
		current->victim_team = cg_entities[victimNum].current.team;
	}
	if( attacker != NULL ) {
		Q_strncpyz( current->attacker, attacker->name, sizeof( current->attacker ) );
		current->attacker_team = cg_entities[attackerNum].current.team;
	}
	current->mod = mod;

	if( cg.view.playerPrediction && ISVIEWERENTITY( victimNum ) ) {
		self_obituary.entropy = 0;
	}

	if( attackerNum ) {
		if( victimNum != attackerNum ) {
			current->type = OBITUARY_NORMAL;

			TempAllocator temp = cls.frame_arena.temp();
			const char * obituary = temp( "{}{}{}", RandomPrefix( &rng, 0.05f ), RandomPrefix( &rng, 0.5f ), RandomObituary( &rng ) );

			char attacker_name[ MAX_NAME_CHARS + 1 ];
			Q_strncpyz( attacker_name, attacker->name, sizeof( attacker_name ) );
			Q_strupr( attacker_name );

			char victim_name[ MAX_NAME_CHARS + 1 ];
			Q_strncpyz( victim_name, victim->name, sizeof( victim_name ) );
			Q_strupr( victim_name );

			RGB8 attacker_color = CG_TeamColor( current->attacker_team );
			RGB8 victim_color = CG_TeamColor( current->victim_team );

			if( ISVIEWERENTITY( attackerNum ) ) {
				CG_CenterPrint( temp( "{} {}", obituary, victim_name ) );
			}

			if( assistor == NULL ) {
				CG_AddChat( temp( "{}{} {}{} {}{}",
					ImGuiColorToken( attacker_color ), attacker_name,
					ImGuiColorToken( rgba8_diesel_yellow ), obituary,
					ImGuiColorToken( victim_color ), victim_name
				) );
			}
			else {
				const char * assist_conj = RandomAssistConjunction( &rng );
				char assistor_name[ MAX_NAME_CHARS + 1 ];
				Q_strncpyz( assistor_name, assistor->name, sizeof( assistor_name ) );
				Q_strupr( assistor_name );

				CG_AddChat( temp( "{}{} {}{} {}{} {}{} {}{}",
					ImGuiColorToken( attacker_color ), attacker_name,
					ImGuiColorToken( 255, 255, 255, 255 ), assist_conj,
					ImGuiColorToken( attacker_color ), assistor_name,
					ImGuiColorToken( rgba8_diesel_yellow ), obituary,
					ImGuiColorToken( victim_color ), victim_name
				) );
			}

			if( cg.view.playerPrediction && ISVIEWERENTITY( victimNum ) ) {
				self_obituary.time = cls.monotonicTime;
				self_obituary.entropy = entropy;
			}
		}
		else {   // suicide
			current->type = OBITUARY_SUICIDE;
		}
	}
	else {   // world accidents
		current->type = OBITUARY_ACCIDENT;
	}
}

static const Material * MODToIcon( int mod ) {
	int weapon = MODToWeapon( mod );
	if( weapon != Weapon_None ) {
		return cgs.media.shaderWeaponIcon[ weapon ];
	}

	switch( mod ) {
		case MeanOfDeath_Slime:
			return FindMaterial( "gfx/slime" );
		case MeanOfDeath_Lava:
			return FindMaterial( "gfx/lava" );
		case MeanOfDeath_Crush:
			return FindMaterial( "gfx/crush" );
		case MeanOfDeath_Telefrag:
			return FindMaterial( "gfx/telefrag" );
		case MeanOfDeath_Suicide:
			return FindMaterial( "gfx/suicide" );
		case MeanOfDeath_Explosion:
			return FindMaterial( "gfx/explosion" );
		case MeanOfDeath_Trigger:
			return FindMaterial( "gfx/trigger" );
		case MeanOfDeath_Laser:
			return FindMaterial( "gfx/laser" );
		case MeanOfDeath_Spike:
			return FindMaterial( "gfx/spike" );
		case MeanOfDeath_Void:
			return FindMaterial( "gfx/void" );
	}

	return FindMaterial( "" );
}

static void CG_DrawObituaries(
	int x, int y, Alignment alignment, int width, int height,
	int internal_align, unsigned int icon_size
) {
	const int icon_padding = 4;

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

		const Material * pic = MODToIcon( obr->mod );

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

		Draw2DBox( x + xoffset, y + yoffset + ( line_height - icon_size ) / 2, icon_size, icon_size, pic, AttentionGettingColor() );

		xoffset += icon_size + icon_padding;

		Vec4 color = CG_TeamColorVec4( obr->victim_team );
		DrawText( font, layout_cursor_font_size, obr->victim, x + xoffset, obituary_y, color, layout_cursor_font_border );

		yoffset += line_height;
	} while( i != next );

	if( cg.predictedPlayerState.health <= 0 && cg.predictedPlayerState.team != TEAM_SPECTATOR ) {
		if( self_obituary.entropy != 0 ) {
			float h = 128.0f;
			float yy = frame_static.viewport.y * 0.5f - h * 0.5f;

			float t = float( cls.monotonicTime - self_obituary.time ) / 1000.0f;

			Draw2DBox( 0, yy, frame_static.viewport.x, h, cls.white_material, Vec4( 0, 0, 0, Min2( 0.5f, t * 0.5f ) ) );

			if( t >= 1.0f ) {
				RNG rng = new_rng( self_obituary.entropy, 0 );

				TempAllocator temp = cls.frame_arena.temp();
				const char * obituary = temp( "{}{}{}", RandomPrefix( &rng, 0.05f ), RandomPrefix( &rng, 0.5f ), RandomObituary( &rng ) );

				float size = Lerp( h * 0.5f, Unlerp01( 1.0f, t, 3.0f ), h * 0.75f );
				Vec4 color = CG_TeamColorVec4( TEAM_ENEMY );
				color.w = Unlerp01( 1.0f, t, 2.0f );
				DrawText( cgs.fontNormal, size, obituary, Alignment_CenterMiddle, frame_static.viewport.x * 0.5f, frame_static.viewport.y * 0.5f, color );
			}
		}
	}
}

//=============================================================================

void CG_ClearAwards() {
	// reset awards
	cg.award_head = 0;
	memset( cg.award_times, 0, sizeof( cg.award_times ) );
}

static void CG_DrawAwards( int x, int y, Alignment alignment, float font_size, Vec4 color, bool border ) {
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

static bool CG_LFuncDrawCallvote( cg_layoutnode_t *argumentnode ) {
	const char * vote = cgs.configStrings[ CS_CALLVOTE ];
	if( strlen( vote ) == 0 )
		return true;

	int left = CG_HorizontalAlignForWidth( layout_cursor_x, layout_cursor_alignment, layout_cursor_width );
	int top = CG_VerticalAlignForHeight( layout_cursor_y, layout_cursor_alignment, layout_cursor_height );
	int right = left + layout_cursor_width;

	TempAllocator temp = cls.frame_arena.temp();

	const char * yeses = cgs.configStrings[ CS_CALLVOTE_YES_VOTES ];
	const char * required = cgs.configStrings[ CS_CALLVOTE_REQUIRED_VOTES ];

	bool voted = cg.predictedPlayerState.voted;
	float padding = layout_cursor_font_size * 0.5f;

	if( !voted ) {
		float height = padding * 2 + layout_cursor_font_size * 2.2f;
		Draw2DBox( left, top, layout_cursor_width, height, cls.white_material, Vec4( 0, 0, 0, 0.5f ) );
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

struct cg_layoutoperators_t {
	const char *name;
	opFunc_t opFunc;
};

static cg_layoutoperators_t cg_LayoutOperators[] = {
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
};

static opFunc_t CG_OperatorFuncForArgument( Span< const char > token ) {
	for( cg_layoutoperators_t op : cg_LayoutOperators ) {
		if( StrCaseEqual( token, op.name ) ) {
			return op.opFunc;
		}
	}

	return NULL;
}

//=============================================================================

static const char *CG_GetStringArg( cg_layoutnode_t **argumentsnode );
static float CG_GetNumericArg( cg_layoutnode_t **argumentsnode );

//=============================================================================

enum {
	LNODE_COMMAND,
	LNODE_NUMERIC,
	LNODE_STRING,
	LNODE_REFERENCE_NUMERIC,
	LNODE_DUMMY,
};

//=============================================================================
// Commands' Functions
//=============================================================================

static void CG_DrawWeaponIcons( int x, int y, int offx, int offy, int iw, int ih, Alignment alignment, float font_size ) {
	const SyncPlayerState * ps = &cg.predictedPlayerState;
	Vec4 light_gray = sRGBToLinear( RGBA8( 96, 96, 96, 255 ) );
	Vec4 dark_gray = sRGBToLinear( RGBA8( 51, 51, 51, 255 ) );
	Vec4 color_ammo_max = sRGBToLinear( rgba8_diesel_yellow );
	Vec4 color_ammo_min = sRGBToLinear( RGBA8( 255, 56, 97, 255 ) );

	const SyncEntityState * es = &cg_entities[ ps->POVnum ].current;

	int num_weapons = 0;
	for( size_t i = 0; i < ARRAY_COUNT( ps->weapons ); i++ ) {
		if( ps->weapons[ i ].weapon != Weapon_None ) {
			num_weapons++;
		}
	}

	int bomb = ( es->effects & EF_CARRIER ) != 0 ? 1 : 0;

	int padx = offx - iw;
	int pady = offy - ih;
	int total_width = Max2( 0, ( num_weapons + bomb ) * offx - padx );
	int total_height = Max2( 0, ( num_weapons + bomb ) * offy - pady );

	int border = iw * 0.04f;
	int border_sel = border * 0.25f;
	int padding = iw * 0.1f;
	int pad_sel = border * 2;
	int innerw = iw - border * 2;
	int innerh = ih - border * 2;
	int iconw = iw - border * 2 - padding * 2;
	int iconh = ih - border * 2 - padding * 2;

	if( bomb != 0 ) {
		int curx = CG_HorizontalAlignForWidth( x, alignment, total_width );
		int cury = CG_VerticalAlignForHeight( y, alignment, total_height );
		Vec4 color = ps->can_plant ? AttentionGettingColor() : light_gray;

		Draw2DBox( curx, cury, iw, ih, cls.white_material, color );
		Draw2DBox( curx + border, cury + border, innerw, innerh, cls.white_material, dark_gray );

		Draw2DBox( curx + border + padding, cury + border + padding, iconw, iconh, cgs.media.shaderBombIcon, color );
	}

	for( int i = 0; i < num_weapons; i++ ) {
		int curx = CG_HorizontalAlignForWidth( x + offx * ( i + bomb ), alignment, total_width );
		int cury = CG_VerticalAlignForHeight( y + offy * ( i + bomb ), alignment, total_height );

		WeaponType weap = ps->weapons[ i ].weapon;
		int ammo = ps->weapons[ i ].ammo;
		const WeaponDef * def = GS_GetWeaponDef( weap );

		Vec4 color = Vec4( 1.0f );
		float ammo_frac = 1.0f;

		if( def->clip_size != 0 ) {
			if( weap == ps->weapon && ps->weapon_state == WeaponState_Reloading ) {
				ammo_frac = 1.0f - float( ps->weapon_time ) / float( def->reload_time );
			}
			else {
				color = Vec4( 0.0f, 1.0f, 0.0f, 1.0f );
				ammo_frac = float( ammo ) / float( def->clip_size );
			}
		}

		color = Lerp( color_ammo_min, Unlerp( 0.0f, ammo_frac, 1.0f ), color_ammo_max );

		Vec4 color_bg = Vec4( color.xyz() * 0.33f, 1.0f );

		const Material * icon = cgs.media.shaderWeaponIcon[ weap ];

		bool selected = weap == ps->pending_weapon;
		int offset = ( selected ? border_sel : 0 );
		int pady_sel = ( selected ? pad_sel : 0 );

		if( ammo_frac < 1.0f ) {
			Draw2DBox( curx - offset, cury - offset - pady_sel, iw + offset * 2, ih + offset * 2, cls.white_material, light_gray );
			Draw2DBox( curx + border, cury + border - pady_sel, innerw, innerh, cls.white_material, dark_gray );
			Draw2DBox( curx + border + padding, cury + border + padding - pady_sel, iconw, iconh, icon, light_gray );
		}

		Vec2 half_pixel = HalfPixelSize( icon );

		if( def->clip_size == 0 || ammo_frac != 0 ) {
			Draw2DBox( curx - offset, cury + ih * ( 1.0f - ammo_frac ) - offset - pady_sel, iw + offset * 2, ih * ammo_frac + offset * 2, cls.white_material, color );
			Draw2DBox( curx + border, cury + ih * ( 1.0f - ammo_frac ) + border - pady_sel, innerw, ih * ammo_frac - border * 2, cls.white_material, color_bg );
		}

		float asdf = Max2( ih * ( 1.0f - ammo_frac ), float( padding ) ) - padding;
		Draw2DBoxUV( curx + border + padding, cury + border + padding + asdf - pady_sel,
			iconw, iconh - asdf,
			Vec2( half_pixel.x, Lerp( half_pixel.y, asdf / iconh, 1.0f - half_pixel.y ) ), 1.0f - half_pixel,
			cgs.media.shaderWeaponIcon[ weap ], color );

		if( def->clip_size != 0 ) {
			DrawText( GetHUDFont(), font_size, va( "%i", ammo ), Alignment_CenterMiddle, curx + iw*0.5f, cury - ih * 0.25f - pady_sel, color, layout_cursor_font_border );
		}

		// weapon slot binds start from index 1, use drawn_weapons for actual loadout index
		char bind[ 32 ];

		// UNBOUND can look real stupid so bump size down a bit in case someone is scrolling. this still doesnt fit
		const float bind_font_size = font_size * 0.55f;

		// first try the weapon specific bind
		if( !CG_GetBoundKeysString( va( "use %s", def->short_name ), bind, sizeof( bind ) ) ) {
			CG_GetBoundKeysString( va( "weapon %i", i + 1 ), bind, sizeof( bind ) );
		}

		DrawText( GetHUDFont(), bind_font_size, va( "[ %s ]", bind) , Alignment_CenterMiddle, curx + iw*0.5f, cury + ih * 1.2f - pady_sel, layout_cursor_color, layout_cursor_font_border );
	}
}

static bool CG_LFuncDrawPicByName( cg_layoutnode_t *argumentnode ) {
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

static bool CG_LFuncCursor( cg_layoutnode_t *argumentnode ) {
	float x = ScaleX( CG_GetNumericArg( &argumentnode ) );
	float y = ScaleY( CG_GetNumericArg( &argumentnode ) );

	layout_cursor_x = Q_rint( x );
	layout_cursor_y = Q_rint( y );
	return true;
}

static bool CG_LFuncMoveCursor( cg_layoutnode_t *argumentnode ) {
	float x = ScaleX( CG_GetNumericArg( &argumentnode ) );
	float y = ScaleY( CG_GetNumericArg( &argumentnode ) );

	layout_cursor_x += Q_rint( x );
	layout_cursor_y += Q_rint( y );
	return true;
}

static bool CG_LFuncSize( cg_layoutnode_t *argumentnode ) {
	float x = ScaleX( CG_GetNumericArg( &argumentnode ) );
	float y = ScaleY( CG_GetNumericArg( &argumentnode ) );

	layout_cursor_width = Q_rint( x );
	layout_cursor_height = Q_rint( y );
	return true;
}

static bool CG_LFuncColor( cg_layoutnode_t *argumentnode ) {
	for( int i = 0; i < 4; i++ ) {
		layout_cursor_color[ i ] = Clamp01( CG_GetNumericArg( &argumentnode ) );
	}
	return true;
}

static bool CG_LFuncColorsRGB( cg_layoutnode_t *argumentnode ) {
	for( int i = 0; i < 4; i++ ) {
		layout_cursor_color[ i ] = sRGBToLinear( Clamp01( CG_GetNumericArg( &argumentnode ) ) );
	}
	return true;
}

static bool CG_LFuncColorToTeamColor( cg_layoutnode_t *argumentnode ) {
	layout_cursor_color = CG_TeamColorVec4( CG_GetNumericArg( &argumentnode ) );
	return true;
}

static bool CG_LFuncAttentionGettingColor( cg_layoutnode_t * argumentnode ) {
	layout_cursor_color = AttentionGettingColor();
	return true;
}

static bool CG_LFuncColorAlpha( cg_layoutnode_t *argumentnode ) {
	layout_cursor_color.w = CG_GetNumericArg( &argumentnode );
	return true;
}

static bool CG_LFuncAlignment( cg_layoutnode_t *argumentnode ) {
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
		Com_Printf( "WARNING 'CG_LFuncAlignment' Unknown alignment '%s'\n", x );
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
		Com_Printf( "WARNING 'CG_LFuncAlignment' Unknown alignment '%s'\n", y );
		return false;
	}

	return true;
}

static bool CG_LFuncFontSize( cg_layoutnode_t *argumentnode ) {
	cg_layoutnode_t *charnode = argumentnode;
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

static bool CG_LFuncFontStyle( cg_layoutnode_t *argumentnode ) {
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
		Com_Printf( "WARNING 'CG_LFuncFontStyle' Unknown font style '%s'\n", fontstyle );
		return false;
	}

	return true;
}

static bool CG_LFuncFontBorder( cg_layoutnode_t *argumentnode ) {
	const char * border = CG_GetStringArg( &argumentnode );
	layout_cursor_font_border = Q_stricmp( border, "on" ) == 0;
	return true;
}

static bool CG_LFuncDrawObituaries( cg_layoutnode_t *argumentnode ) {
	int internal_align = (int)CG_GetNumericArg( &argumentnode );
	int icon_size = (int)CG_GetNumericArg( &argumentnode );

	CG_DrawObituaries( layout_cursor_x, layout_cursor_y, layout_cursor_alignment,
		layout_cursor_width, layout_cursor_height, internal_align, icon_size * frame_static.viewport_height / 600 );
	return true;
}

static bool CG_LFuncDrawAwards( cg_layoutnode_t *argumentnode ) {
	CG_DrawAwards( layout_cursor_x, layout_cursor_y, layout_cursor_alignment, layout_cursor_font_size, layout_cursor_color, layout_cursor_font_border );
	return true;
}

static bool CG_LFuncDrawClock( cg_layoutnode_t *argumentnode ) {
	CG_DrawClock( layout_cursor_x, layout_cursor_y, layout_cursor_alignment, GetHUDFont(), layout_cursor_font_size, layout_cursor_color, layout_cursor_font_border );
	return true;
}

static bool CG_LFuncDrawDamageNumbers( cg_layoutnode_t *argumentnode ) {
	CG_DrawDamageNumbers();
	return true;
}

static bool CG_LFuncDrawBombIndicators( cg_layoutnode_t *argumentnode ) {
	CG_DrawBombHUD();
	return true;
}

static bool CG_LFuncDrawPlayerIcons( cg_layoutnode_t *argumentnode ) {
	int team = int( CG_GetNumericArg( &argumentnode ) );
	int alive = int( CG_GetNumericArg( &argumentnode ) );
	int total = int( CG_GetNumericArg( &argumentnode ) );

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

static bool CG_LFuncDrawPointed( cg_layoutnode_t *argumentnode ) {
	CG_DrawPlayerNames( GetHUDFont(), layout_cursor_font_size, layout_cursor_color, layout_cursor_font_border );
	return true;
}

static bool CG_LFuncDrawString( cg_layoutnode_t *argumentnode ) {
	const char *string = CG_GetStringArg( &argumentnode );

	if( !string || !string[0] ) {
		return false;
	}

	DrawText( GetHUDFont(), layout_cursor_font_size, string, layout_cursor_alignment, layout_cursor_x, layout_cursor_y, layout_cursor_color, layout_cursor_font_border );

	return true;
}

static bool CG_LFuncDrawBindString( cg_layoutnode_t *argumentnode ) {
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

static bool CG_LFuncDrawPlayerName( cg_layoutnode_t *argumentnode ) {
	int index = (int)CG_GetNumericArg( &argumentnode ) - 1;

	if( index >= 0 && index < client_gs.maxclients && cgs.clientInfo[index].name[0] ) {
		DrawText( GetHUDFont(), layout_cursor_font_size, cgs.clientInfo[ index ].name, layout_cursor_alignment, layout_cursor_x, layout_cursor_y, layout_cursor_color, layout_cursor_font_border );
		return true;
	}

	return false;
}

static bool CG_LFuncDrawNumeric( cg_layoutnode_t *argumentnode ) {
	int value = CG_GetNumericArg( &argumentnode );
	DrawText( GetHUDFont(), layout_cursor_font_size, va( "%i", value ), layout_cursor_alignment, layout_cursor_x, layout_cursor_y, layout_cursor_color, layout_cursor_font_border );
	return true;
}

static bool CG_LFuncDrawWeaponIcons( cg_layoutnode_t *argumentnode ) {
	int offx = CG_GetNumericArg( &argumentnode ) * frame_static.viewport_width / 800;
	int offy = CG_GetNumericArg( &argumentnode ) * frame_static.viewport_height / 600;
	int w = CG_GetNumericArg( &argumentnode ) * frame_static.viewport_width / 800;
	int h = CG_GetNumericArg( &argumentnode ) * frame_static.viewport_height / 600;
	float font_size = CG_GetNumericArg( &argumentnode );

	CG_DrawWeaponIcons( layout_cursor_x, layout_cursor_y, offx, offy, w, h, layout_cursor_alignment, font_size );

	return true;
}

static bool CG_LFuncDrawCrossHair( cg_layoutnode_t *argumentnode ) {
	CG_DrawCrosshair();
	return true;
}

static bool CG_LFuncDrawNet( cg_layoutnode_t *argumentnode ) {
	CG_DrawNet( layout_cursor_x, layout_cursor_y, layout_cursor_width, layout_cursor_height, layout_cursor_alignment, layout_cursor_color );
	return true;
}

static bool CG_LFuncIf( cg_layoutnode_t *argumentnode ) {
	return (int)CG_GetNumericArg( &argumentnode ) != 0;
}

static bool CG_LFuncIfNot( cg_layoutnode_t *argumentnode ) {
	return (int)CG_GetNumericArg( &argumentnode ) == 0;
}

static bool CG_LFuncEndIf( cg_layoutnode_t *argumentnode ) {
	return true;
}

struct cg_layoutcommand_t {
	const char *name;
	bool ( *func )( cg_layoutnode_t *argumentnode );
	int numparms;
	const char *help;
};

static const cg_layoutcommand_t cg_LayoutCommands[] = {
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
		"setColorsRGB",
		CG_LFuncColorsRGB,
		4,
		"setColor but in sRGB",
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
		CG_LFuncEndIf,
		0,
		"End of conditional expression block",
	},
};


//=============================================================================

static const char *CG_GetStringArg( cg_layoutnode_t **argumentsnode ) {
	cg_layoutnode_t *anode = *argumentsnode;

	if( anode == NULL ) {
		return "";
	}

	*argumentsnode = anode->next;
	return anode->string;
}

/*
* CG_GetNumericArg
* can use recursion for mathematical operations
*/
static float CG_GetNumericArg( cg_layoutnode_t **argumentsnode ) {
	cg_layoutnode_t *anode = *argumentsnode;

	if( anode == NULL ) {
		return 0.0f;
	}

	if( anode->type != LNODE_NUMERIC && anode->type != LNODE_REFERENCE_NUMERIC ) {
		Com_Printf( "WARNING: 'CG_LayoutGetNumericArg': arg %s is not numeric\n", anode->string );
	}

	*argumentsnode = anode->next;
	float value;
	if( anode->type == LNODE_REFERENCE_NUMERIC ) {
		value = cg_numeric_references[anode->idx].func( cg_numeric_references[anode->idx].parameter );
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
static cg_layoutnode_t *CG_LayoutParseCommandNode( Span< const char > token ) {
	for( cg_layoutcommand_t command : cg_LayoutCommands ) {
		if( StrCaseEqual( token, command.name ) ) {
			cg_layoutnode_t * node = ALLOC( sys_allocator, cg_layoutnode_t );
			*node = { };

			node->type = LNODE_COMMAND;
			node->num_args = command.numparms;
			node->string = CopyString( sys_allocator, command.name );
			node->func = command.func;

			return node;
		}
	}

	return NULL;
}

/*
* CG_LayoutParseArgumentNode
* alloc a new node for an argument
*/
static cg_layoutnode_t *CG_LayoutParseArgumentNode( Span< const char > token ) {
	cg_layoutnode_t * node = ALLOC( sys_allocator, cg_layoutnode_t );
	*node = { };

	if( token[ 0 ] == '%' ) {
		node->type = LNODE_REFERENCE_NUMERIC;

		bool ok = false;
		for( size_t i = 0; i < ARRAY_COUNT( cg_numeric_references ); i++ ) {
			if( StrCaseEqual( token + 1, cg_numeric_references[ i ].name ) ) {
				ok = true;
				node->idx = i;
				break;
			}
		}

		if( !ok ) {
			Com_GGPrint( "Warning: HUD: {} is not valid numeric reference", token );
		}
	}
	else if( token[ 0 ] == '#' ) {
		node->type = LNODE_NUMERIC;

		bool ok = false;
		for( size_t i = 0; i < ARRAY_COUNT( cg_numeric_constants ); i++ ) {
			if( StrCaseEqual( token + 1, cg_numeric_constants[ i ].name ) ) {
				ok = true;
				node->value = cg_numeric_constants[ i ].value;
				break;
			}
		}

		if( !ok ) {
			Com_GGPrint( "Warning: HUD: %s is not valid numeric constant", token );
		}

	}
	else if( token[ 0 ] == '\\' ) {
		node->type = LNODE_STRING;
		token++;
	}
	else {
		node->value = 0.0f;
		node->type = TrySpanToFloat( token, &node->value ) ? LNODE_NUMERIC : LNODE_STRING;
	}

	node->string = ( *sys_allocator )( "{}", token );

	return node;
}

static void CG_RecurseFreeLayoutThread( cg_layoutnode_t * node ) {
	if( node == NULL )
		return;

	CG_RecurseFreeLayoutThread( node->args );
	CG_RecurseFreeLayoutThread( node->ifthread );
	CG_RecurseFreeLayoutThread( node->next );
	FREE( sys_allocator, node->string );
	FREE( sys_allocator, node );
}

static Span< const char > ParseHUDToken( Span< const char > * cursor ) {
	Span< const char > token;

	// skip comments
	while( true ) {
		token = ParseToken( cursor, Parse_DontStopOnNewLine );
		if( !StartsWith( token, "//" ) )
			break;

		while( true ) {
			if( ParseToken( cursor, Parse_StopOnNewLine ) == "" ) {
				break;
			}
		}
	}

	// strip trailing comma
	if( token.n > 0 && token[ token.n - 1 ] == ',' ) {
		token.n--;
	}

	return token;
}

struct LayoutNodeList {
	cg_layoutnode_t * head;
	cg_layoutnode_t * tail;

	LayoutNodeList() {
		head = ALLOC( sys_allocator, cg_layoutnode_t );
		tail = head;

		*head = { };
		head->type = LNODE_DUMMY;
	}

	void add( cg_layoutnode_t * node ) {
		tail->next = node;
		tail = tail->next;
	}
};

static cg_layoutnode_t * CG_ParseArgs( Span< const char > * cursor, const char * command_name, int expected_args, int * parsed_args ) {
	LayoutNodeList nodes;

	*parsed_args = 0;

	while( true ) {
		Span< const char > last_cursor = *cursor;
		Span< const char > token = ParseHUDToken( cursor );
		if( token == "" )
			break;

		opFunc_t op = CG_OperatorFuncForArgument( token );
		if( op != NULL ) {
			if( nodes.tail == NULL ) {
				Com_GGPrint( "WARNING 'CG_RecurseParseLayoutScript'({}): \"{}\" Operator hasn't any prior argument", command_name, token );
				continue;
			}
			if( nodes.tail->opFunc != NULL ) {
				Com_GGPrint( "WARNING 'CG_RecurseParseLayoutScript'({}): \"{}\" Found two operators in a row", command_name, token );
			}
			if( nodes.tail->type == LNODE_STRING ) {
				Com_GGPrint( "WARNING 'CG_RecurseParseLayoutScript'({}): \"{}\" Operator was assigned to a string node", command_name, token );
			}

			nodes.tail->opFunc = op;
			continue;
		}

		if( *parsed_args == expected_args && nodes.tail->opFunc == NULL ) {
			*cursor = last_cursor;
			break;
		}

		cg_layoutnode_t * command = CG_LayoutParseCommandNode( token );
		if( command != NULL ) {
			Com_GGPrint( "WARNING 'CG_RecurseParseLayoutScript': \"{}\" is not a valid argument for \"{}\"", token, command_name );
			break;
		}

		cg_layoutnode_t * node = CG_LayoutParseArgumentNode( token );

		if( nodes.tail == NULL || nodes.tail->opFunc == NULL ) {
			*parsed_args += 1;
		}

		nodes.add( node );
	}

	return nodes.head;
}

static cg_layoutnode_t *CG_RecurseParseLayoutScript( Span< const char > * cursor, int level ) {
	LayoutNodeList nodes;

	while( true ) {
		Span< const char > token = ParseHUDToken( cursor );
		if( token == "" ) {
			if( level > 0 ) {
				Com_Printf( "WARNING 'CG_RecurseParseLayoutScript'(level %i): If without endif\n", level );
			}
			break;
		}

		cg_layoutnode_t * command = CG_LayoutParseCommandNode( token );
		if( command == NULL ) {
			Com_GGPrint( "WARNING 'CG_RecurseParseLayoutScript'(level {}): unrecognized command \"{}\"", level, token );
			break;
		}

		int parsed_args;
		command->args = CG_ParseArgs( cursor, command->string, command->num_args, &parsed_args );

		if( parsed_args != command->num_args ) {
			Com_Printf( "ERROR: Layout command %s: invalid argument count (expecting %i, found %i)\n", command->string, command->num_args, parsed_args );
		}

		nodes.add( command );

		if( command->func == CG_LFuncIf || command->func == CG_LFuncIfNot ) {
			command->ifthread = CG_RecurseParseLayoutScript( cursor, level + 1 );
		}
		else if( command->func == CG_LFuncEndIf ) {
			break;
		}
	}

	return nodes.head;
}

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
static void CG_RecurseExecuteLayoutThread( cg_layoutnode_t * node ) {
	if( node == NULL )
		return;

	// args->next to skip the dummy node
	if( node->type != LNODE_DUMMY && node->func( node->args->next ) ) {
		CG_RecurseExecuteLayoutThread( node->ifthread );
	}

	CG_RecurseExecuteLayoutThread( node->next );
}

static bool LoadHUDFile( const char * path, DynamicString & script ) {
	Span< const char > contents = AssetString( StringHash( path ) );
	const char * cursor = contents.ptr;

	while( true ) {
		const char * before_include = strstr( cursor, "#include" );
		if( before_include == NULL )
			break;

		script.append_raw( cursor, before_include - cursor );
		cursor = before_include + strlen( "#include" );

		Span< const char > include = ParseToken( &cursor, Parse_StopOnNewLine );
		if( include == "" ) {
			Com_Printf( "HUD: WARNING: Missing file after #include\n" );
			return false;
		}

		u64 hash = Hash64( "huds/inc/" );
		hash = Hash64( include.ptr, include.n, hash );
		hash = Hash64( ".hud", strlen( ".hud" ), hash );

		Span< const char > include_contents = AssetString( StringHash( hash ) );
		if( include_contents == "" ) {
			Com_GGPrint( "HUD: Couldn't include huds/inc/{}.hud", include );
			return false;
		}

		// TODO: AssetString includes the trailing '\0' and we don't
		// want to add that to the script
		script.append_raw( include_contents.ptr, include_contents.n - 1 );
	}

	script += cursor;

	return true;
}

static void CG_LoadHUD() {
	CG_RecurseFreeLayoutThread( hud_root );

	TempAllocator temp = cls.frame_arena.temp();
	const char * path = "huds/default.hud";

	DynamicString script( &temp );
	if( !LoadHUDFile( path, script ) ) {
		Com_Printf( "HUD: failed to load %s file\n", path );
		return;
	}

	Span< const char > cursor = script.span();
	hud_root = CG_RecurseParseLayoutScript( &cursor, 0 );

	layout_cursor_font_style = FontStyle_Normal;
	layout_cursor_font_size = cgs.textSizeSmall;
}

void CG_InitHUD() {
	hud_root = NULL;
	CG_LoadHUD();
}

void CG_ShutdownHUD() {
	CG_RecurseFreeLayoutThread( hud_root );
}

void CG_DrawHUD() {
	bool hotload = false;
	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".hud" ) {
			hotload = true;
			break;
		}
	}

	if( hotload ) {
		CG_LoadHUD();
	}

	ZoneScoped;
	CG_RecurseExecuteLayoutThread( hud_root );
}
