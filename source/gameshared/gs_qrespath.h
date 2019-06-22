/*
Copyright (C) 1997-2001 Id Software, Inc.

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

//
//
// gs_qrespaths.h
//
//

//
//
// SHADERS
//
//

//outlines
#define DEFAULT_OUTLINE_HEIGHT      0.3f

// icons

// weapon
#define PATH_GUNBLADE_ICON      "gfx/hud/icons/weapon/gunblade"
#define PATH_MACHINEGUN_ICON    "gfx/hud/icons/weapon/machinegun"
#define PATH_RIOTGUN_ICON       "gfx/hud/icons/weapon/riot"
#define PATH_GRENADELAUNCHER_ICON   "gfx/hud/icons/weapon/grenade"
#define PATH_ROCKETLAUNCHER_ICON    "gfx/hud/icons/weapon/rocket"
#define PATH_PLASMAGUN_ICON     "gfx/hud/icons/weapon/plasma"
#define PATH_ELECTROBOLT_ICON       "gfx/hud/icons/weapon/electro"
#define PATH_LASERGUN_ICON      "gfx/hud/icons/weapon/laser"

// decals
#define PATH_BULLET_MARK        "gfx/decals/d_bullet_hit"
#define PATH_EXPLOSION_MARK     "gfx/decals/d_explode_hit"

// explosions
#define PATH_ROCKET_EXPLOSION_SPRITE    "gfx/rocket_explosion"
#define PATH_ROCKET_EXPLOSION_RING_SPRITE   "gfx/rocket_explosion_ring"
#define PATH_GRENADE_EXPLOSION_SPRITE    "gfx/grenade_explosion"
#define PATH_GRENADE_EXPLOSION_RING_SPRITE   "gfx/grenade_explosion_ring"

// simpleitems
#define PATH_MACHINEGUN_SIMPLEITEM      "gfx/simpleitems/weapon/machinegun"
#define PATH_RIOTGUN_SIMPLEITEM         "gfx/simpleitems/weapon/riot"
#define PATH_GRENADELAUNCHER_SIMPLEITEM     "gfx/simpleitems/weapon/grenade"
#define PATH_ROCKETLAUNCHER_SIMPLEITEM      "gfx/simpleitems/weapon/rocket"
#define PATH_PLASMAGUN_SIMPLEITEM       "gfx/simpleitems/weapon/plasma"
#define PATH_LASERGUN_SIMPLEITEM        "gfx/simpleitems/weapon/laser"
#define PATH_ELECTROBOLT_SIMPLEITEM     "gfx/simpleitems/weapon/electro"
#define PATH_AMMOPACK_SIMPLEITEM        "gfx/simpleitems/ammo/pack"

#define PATH_KEYICON_FORWARD            "gfx/hud/keys/key_forward"
#define PATH_KEYICON_BACKWARD           "gfx/hud/keys/key_back"
#define PATH_KEYICON_LEFT               "gfx/hud/keys/key_left"
#define PATH_KEYICON_RIGHT              "gfx/hud/keys/key_right"
#define PATH_KEYICON_FIRE               "gfx/hud/keys/act_fire"
#define PATH_KEYICON_JUMP               "gfx/hud/keys/act_jump"
#define PATH_KEYICON_CROUCH             "gfx/hud/keys/act_crouch"
#define PATH_KEYICON_SPECIAL            "gfx/hud/keys/act_special"

//
//
// MODELS
//
//

// weapons
#define PATH_GUNBLADE_MODEL         "models/weapons/gunblade/gunblade.md3"
#define PATH_MACHINEGUN_MODEL       "models/weapons/machinegun/machinegun.md3"
#define PATH_RIOTGUN_MODEL          "models/weapons/riotgun/riotgun.md3"
#define PATH_GRENADELAUNCHER_MODEL  "models/weapons/glauncher/glauncher.md3"
#define PATH_ROCKETLAUNCHER_MODEL   "models/weapons/rl.md3"
#define PATH_PLASMAGUN_MODEL        "models/weapons/plasmagun/plasmagun.md3"
#define PATH_ELECTROBOLT_MODEL      "models/weapons/electrobolt/electrobolt.md3"
#define PATH_LASERGUN_MODEL         "models/weapons/lg.md3"

// weapon projectiles
#define PATH_PLASMA_MODEL        "models/objects/projectile/pg.glb"
#define PATH_GRENADE_MODEL       "models/objects/projectile/gl.glb"
#define PATH_ROCKET_MODEL        "models/objects/projectile/rl.glb"

#define PATH_BULLET_EXPLOSION_MODEL         "models/weapon_hits/bullet/hit_bullet.glb"
#define PATH_PLASMA_EXPLOSION_MODEL         "models/weapon_hits/plasmagun/hit_plasmagun.glb"
#define PATH_ELECTROBLAST_IMPACT_MODEL      "models/weapon_hits/electrobolt/hit_electrobolt.glb"
#define PATH_LASERGUN_IMPACT_MODEL          "models/weapon_hits/lasergun/hit_blastexp.glb"
#define PATH_GUNBLADEBLAST_IMPACT_MODEL     "models/weapon_hits/gunblade/hit_blast.glb"

#define PLASMA_EXPLOSION_MODEL_RADIUS           10.0

//
//
// SOUNDS
//
//

// pickup
#define S_PICKUP_WEAPON     "sounds/items/weapon_pickup"

#define S_ITEM_RESPAWN      "sounds/items/item_spawn"        // item respawn

// misc sounds
#define S_TIMER_BIP_BIP     "sounds/misc/timer_bip_bip"

//wsw: pb disable unreferenced sounds
//#define S_LAND					"sounds/misc/land"
#define S_HIT_WATER     "sounds/misc/hit_water"

#define S_TELEPORT      "sounds/world/tele_in"
#define S_JUMPPAD       "sounds/world/jumppad"
#define S_LAUNCHPAD     "sounds/world/launchpad"

//#define S_PLAT_START		"sounds/movers/elevator_start"
#define S_PLAT_START        NULL
#define S_PLAT_MOVE     "sounds/movers/elevator_move"
//#define S_PLAT_STOP		"sounds/movers/elevator_stop"
#define S_PLAT_STOP     NULL

#define S_DOOR_START        "sounds/movers/door_start"
//#define S_DOOR_MOVE		"sounds/movers/door_move"
#define S_DOOR_MOVE     NULL
#define S_DOOR_STOP     "sounds/movers/door_stop"

//#define S_DOOR_ROTATING_START	"sounds/movers/door_rotating_start"
#define S_DOOR_ROTATING_START   "sounds/movers/door_start"
//#define S_DOOR_ROTATING_MOVE	"sounds/movers/door_rotating_move"
#define S_DOOR_ROTATING_MOVE    NULL
//#define S_DOOR_ROTATING_STOP	"sounds/movers/door_rotating_stop"
#define S_DOOR_ROTATING_STOP    "sounds/movers/door_stop"

//#define S_FUNC_ROTATING_START	"sounds/movers/rotating_start"
//#define S_FUNC_ROTATING_MOVE	"sounds/movers/rotating_move"
//#define S_FUNC_ROTATING_STOP	"sounds/movers/rotating_stop"
#define S_FUNC_ROTATING_START   NULL
#define S_FUNC_ROTATING_MOVE    NULL
#define S_FUNC_ROTATING_STOP    NULL

#define S_BUTTON_START      "sounds/movers/button"

// world sounds
#define S_WORLD_WATER_IN            "sounds/world/water_in"
#define S_WORLD_UNDERWATER          "sounds/world/underwater"
#define S_WORLD_WATER_OUT           "sounds/world/water_out"

#define S_WORLD_SLIME_IN            "sounds/world/water_in" // using water sounds for now
#define S_WORLD_UNDERSLIME          "sounds/world/underwater"
#define S_WORLD_SLIME_OUT           "sounds/world/water_out"

#define S_WORLD_LAVA_IN             "sounds/world/lava_in"
#define S_WORLD_UNDERLAVA           "sounds/world/underwater"
#define S_WORLD_LAVA_OUT            "sounds/world/lava_out"

#define S_WORLD_SECRET              "sounds/misc/secret"
#define S_WORLD_MESSAGE             "sounds/misc/talk"

// player sounds
#define S_PLAYER_JUMP_1_to_2        "*jump_%i"                   // player jump
#define S_PLAYER_WALLJUMP_1_to_2    "*wj_%i"                 // player walljump
#define S_PLAYER_DASH_1_to_2        "*dash_%i"                   // player dash


//#define S_PLAYER_FALL_0_to_3		"*fall_%i"					// player fall (height)
#define S_PLAYER_FALLDEATH      "*falldeath"             // player falling to his death
#define S_PLAYER_PAINS          "*pain%i"                    // player pain (percent)
#define S_PLAYER_DEATH          "*death"

#define S_PLAYER_BURN_1_to_2        "*burn%i"                    // player dash

// PLAYERS FALL MISSING

// combat and weapons
#define S_WEAPON_HITS               "sounds/misc/hit_%i"
#define S_WEAPON_KILL               "sounds/misc/kill"
#define S_WEAPON_HIT_TEAM           "sounds/misc/hit_team"
#define S_WEAPON_SWITCH             "sounds/weapons/weapon_switch"
#define S_WEAPON_NOAMMO             "sounds/weapons/weapon_noammo"

// weapon sounds
#define S_WEAPON_GUNBLADE_SHOT_1_to_3     "sounds/weapons/blade_strike%i"
#define S_WEAPON_GUNBLADE_HIT_FLESH_1_to_3    "sounds/weapons/blade_hitflsh%i"
#define S_WEAPON_GUNBLADE_HIT_WALL_1_to_2 "sounds/weapons/blade_hitwall%i"

#define S_WEAPON_RIOTGUN_HIT            "sounds/weapons/riotgun_strong_hit"

#define S_WEAPON_GRENADE_BOUNCE_1_to_2  "sounds/weapons/gren_strong_bounce%i"
#define S_WEAPON_GRENADE_HIT            "sounds/weapons/gren_strong_explosion1"

#define S_WEAPON_ROCKET_FLY             "sounds/weapons/rocket_fly_strong"
#define S_WEAPON_ROCKET_HIT             "sounds/weapons/rocket_strong_explosion"

#define S_WEAPON_PLASMAGUN_FLY          "sounds/weapons/plasmagun_strong_fly"
#define S_WEAPON_PLASMAGUN_HIT          "sounds/weapons/plasmagun_strong_explosion"

#define S_WEAPON_LASERGUN_HUM           "sounds/weapons/laser_strong_hum"
#define S_WEAPON_LASERGUN_STOP          "sounds/weapons/laser_strong_stop"
#define S_WEAPON_LASERGUN_HIT_0         "sounds/weapons/laser_hit0"
#define S_WEAPON_LASERGUN_HIT_1         "sounds/weapons/laser_hit1"
#define S_WEAPON_LASERGUN_HIT_2         "sounds/weapons/laser_hit2"

#define S_WEAPON_ELECTROBOLT_HIT        "sounds/weapons/electrobolt_hit"

// announcer sounds

// countdown
#define S_ANNOUNCER_COUNTDOWN_READY_1_to_2      "sounds/announcer/countdown/ready%02i"
#define S_ANNOUNCER_COUNTDOWN_GET_READY_TO_FIGHT_1_to_2 "sounds/announcer/countdown/get_ready_to_fight%02i"
#define S_ANNOUNCER_COUNTDOWN_FIGHT_1_to_2      "sounds/announcer/countdown/fight%02i"
//#define S_ANNOUNCER_COUNTDOWN_GO_1_to_2				"sounds/announcer/countdown/go%02i"
#define S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2   "sounds/announcer/countdown/%i_%02i"

// post match
#define S_ANNOUNCER_POSTMATCH_GAMEOVER_1_to_2       "sounds/announcer/postmatch/game_over%02i"

// timeout
#define S_ANNOUNCER_TIMEOUT_TIMEOUT_1_to_2      "sounds/announcer/timeout/timeout%02i"
#define S_ANNOUNCER_TIMEOUT_TIMEIN_1_to_2       "sounds/announcer/timeout/timein%02i"

// score
#define S_ANNOUNCER_SCORE_TAKEN_LEAD_1_to_2     "sounds/announcer/score/taken_lead%02i"
#define S_ANNOUNCER_SCORE_TEAM_TAKEN_LEAD_1_to_2    "sounds/announcer/score/taken_lead%02i"
#define S_ANNOUNCER_SCORE_LOST_LEAD_1_to_2      "sounds/announcer/score/lost_lead%02i"
#define S_ANNOUNCER_SCORE_TEAM_LOST_LEAD_1_to_2     "sounds/announcer/score/team_lost_lead%02i"
#define S_ANNOUNCER_SCORE_TIED_LEAD_1_to_2      "sounds/announcer/score/tied_lead%02i"
#define S_ANNOUNCER_SCORE_TEAM_TIED_LEAD_1_to_2     "sounds/announcer/score/team_tied_lead%02i"
#define S_ANNOUNCER_SCORE_TEAM_1_to_4_TAKEN_LEAD_1_to_2 "sounds/announcer/score/team%i_leads%02i"

// UI
#define UI_SHADER_LOADINGLOGO       "ui/baseui/gfx/logo"
#define UI_SHADER_LOADINGBAR        "gfx/ui/loadingbar"

// vsay icons
#define PATH_VSAY_YES_ICON          "gfx/hud/icons/vsay/yes"

// vsay sounds
#define S_VSAY_AFFIRMATIVE          "sounds/vsay/affirmative"
#define S_VSAY_NEGATIVE             "sounds/vsay/negative"
#define S_VSAY_YES                  "sounds/vsay/yes"
#define S_VSAY_NO                   "sounds/vsay/no"
#define S_VSAY_ONDEFENSE            "sounds/vsay/ondefense"
#define S_VSAY_ONOFFENSE            "sounds/vsay/onoffense"
#define S_VSAY_OOPS                 "sounds/vsay/oops"
#define S_VSAY_SORRY                "sounds/vsay/sorry"
#define S_VSAY_THANKS               "sounds/vsay/thanks"
#define S_VSAY_NOPROBLEM            "sounds/vsay/noproblem"
#define S_VSAY_YEEHAA               "sounds/vsay/yeehaa"
#define S_VSAY_GOODGAME             "sounds/vsay/goodgame"
#define S_VSAY_DEFEND               "sounds/vsay/defend"
#define S_VSAY_ATTACK               "sounds/vsay/attack"
#define S_VSAY_NEEDBACKUP           "sounds/vsay/needbackup"
#define S_VSAY_BOOO                 "sounds/vsay/booo"
#define S_VSAY_NEEDDEFENSE          "sounds/vsay/needdefense"
#define S_VSAY_NEEDOFFENSE          "sounds/vsay/needoffense"
#define S_VSAY_NEEDHELP             "sounds/vsay/needhelp"
#define S_VSAY_ROGER                "sounds/vsay/roger"
#define S_VSAY_AREASECURED          "sounds/vsay/areasecured"
#define S_VSAY_BOOMSTICK            "sounds/vsay/boomstick"
#define S_VSAY_OK                   "sounds/vsay/ok"
#define S_VSAY_SHUTUP               "sounds/vsay/shutup"
