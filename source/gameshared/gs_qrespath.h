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

// decals
#define PATH_BULLET_MARK        "gfx/decals/d_bullet_hit"
#define PATH_EXPLOSION_MARK     "gfx/decals/d_explode_hit"

// explosions
#define PATH_ROCKET_EXPLOSION_SPRITE    "gfx/rocket_explosion"
#define PATH_GRENADE_EXPLOSION_SPRITE   "gfx/grenade_explosion"

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
// SOUNDS
//
//

// misc sounds
#define S_TIMER_BIP_BIP "sounds/misc/timer_bip_bip"

#define S_HIT_WATER     "sounds/misc/hit_water"

#define S_TELEPORT      "sounds/world/tele_in"
#define S_JUMPPAD       "sounds/world/jumppad"
#define S_LAUNCHPAD     "sounds/world/launchpad"

#define S_PLAT_START    EMPTY_HASH
#define S_PLAT_MOVE     "sounds/movers/elevator_move"
#define S_PLAT_STOP     EMPTY_HASH

#define S_DOOR_START    "sounds/movers/door_start"
#define S_DOOR_MOVE     EMPTY_HASH
#define S_DOOR_STOP     "sounds/movers/door_stop"

#define S_DOOR_ROTATING_START   "sounds/movers/door_start"
#define S_DOOR_ROTATING_MOVE    EMPTY_HASH
#define S_DOOR_ROTATING_STOP    "sounds/movers/door_stop"

#define S_FUNC_ROTATING_START   EMPTY_HASH
#define S_FUNC_ROTATING_MOVE    EMPTY_HASH
#define S_FUNC_ROTATING_STOP    EMPTY_HASH

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

// combat and weapons
#define S_WEAPON_HITS               "sounds/misc/hit_%i"
#define S_WEAPON_KILL               "sounds/misc/kill"
#define S_WEAPON_HIT_TEAM           "sounds/misc/hit_team"

// announcer sounds

// countdown
#define S_ANNOUNCER_COUNTDOWN_READY_1_to_2      "sounds/announcer/countdown/ready%02i"
#define S_ANNOUNCER_COUNTDOWN_GET_READY_TO_FIGHT_1_to_2 "sounds/announcer/countdown/get_ready_to_fight%02i"
#define S_ANNOUNCER_COUNTDOWN_FIGHT_1_to_2      "sounds/announcer/countdown/fight%02i"
#define S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2   "sounds/announcer/countdown/%i_%02i"

// post match
#define S_ANNOUNCER_POSTMATCH_GAMEOVER_1_to_2       "sounds/announcer/postmatch/game_over%02i"

// timeout
#define S_ANNOUNCER_TIMEOUT_TIMEOUT_1_to_2      "sounds/announcer/timeout/timeout%02i"
#define S_ANNOUNCER_TIMEOUT_TIMEIN_1_to_2       "sounds/announcer/timeout/timein%02i"
