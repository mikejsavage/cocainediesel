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

#pragma once

struct orientation_s;
struct shader_s;
struct fragment_s;
struct entity_s;
struct refdef_s;
struct poly_s;
struct cmodel_s;
struct qfontface_s;

// cg_public.h -- client game dll information visible to engine

//
// structs and variables shared with the main engine
//

#define MAX_PARSE_ENTITIES  1024
typedef struct snapshot_s {
	bool valid;             // cleared if delta parsing was invalid
	int64_t serverFrame;
	int64_t serverTime;    // time in the server when frame was created
	int64_t ucmdExecuted;
	bool delta;
	bool allentities;
	bool multipov;
	int64_t deltaFrameNum;
	size_t areabytes;
	uint8_t *areabits;             // portalarea visibility bits
	int numplayers;
	player_state_t playerState;
	player_state_t playerStates[MAX_CLIENTS];
	int numEntities;
	entity_state_t parsedEntities[MAX_PARSE_ENTITIES];
	game_state_t gameState;
	int numgamecommands;
	gcommand_t gamecommands[MAX_PARSE_GAMECOMMANDS];
	char gamecommandsData[( MAX_STRING_CHARS / 16 ) * MAX_PARSE_GAMECOMMANDS];
	size_t gamecommandsDataHead;
} snapshot_t;

//===============================================================

//
// functions provided by the main engine
//
typedef struct {
	// drops to console a client game error
#ifndef _MSC_VER
	void ( *Error )( const char *msg ) __attribute__( ( noreturn ) );
#else
	void ( *Error )( const char *msg );
#endif

	// console messages
	void ( *Print )( const char *msg );
	void ( *PrintToLog )( const char *msg );

	// key bindings
	const char *( *Key_GetBindingBuf )( int binding );
	const char *( *Key_KeynumToString )( int keynum );

	void ( *GetConfigString )( int i, char *str, int size );
	int64_t ( *Milliseconds )( void );
	bool ( *DownloadRequest )( const char *filename );

	void ( *NET_GetUserCmd )( int frame, usercmd_t *cmd );
	int ( *NET_GetCurrentUserCmdNum )( void );
	void ( *NET_GetCurrentState )( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent );

	// refresh system
	void ( *VID_FlashWindow )();

	// collision detection
	int ( *CM_NumInlineModels )( void );
	struct cmodel_s *( *CM_InlineModel )( int num );
	struct cmodel_s *( *CM_ModelForBBox )( vec3_t mins, vec3_t maxs );
	struct cmodel_s *( *CM_OctagonModelForBBox )( vec3_t mins, vec3_t maxs );
	void ( *CM_TransformedBoxTrace )( trace_t *tr, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, struct cmodel_s *cmodel, int brushmask, const vec3_t origin, const vec3_t angles );
	int ( *CM_TransformedPointContents )( const vec3_t p, struct cmodel_s *cmodel, const vec3_t origin, const vec3_t angles );
	void ( *CM_InlineModelBounds )( const struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs );
	bool ( *CM_InPVS )( const vec3_t p1, const vec3_t p2 );
} cgame_import_t;

//
// functions exported by the client game subsystem
//
typedef struct {
	// the init function will be called at each restart
	void ( *Init )( const char *serverName, unsigned int playerNum,
					bool demoplaying, const char *demoName, unsigned int snapFrameTime );

	// "soft restarts" at demo jumps
	void ( *Reset )( void );

	void ( *Shutdown )( void );

	void ( *ConfigString )( int number, const char *value );

	void ( *EscapeKey )( void );

	void ( *GetEntitySpatilization )( int entNum, vec3_t origin, vec3_t velocity );

	void ( *Trace )( trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passent, int contentmask );

	void ( *RenderView )( unsigned extrapolationTime );

	bool ( *NewFrameSnapshot )( snapshot_t *newSnapshot, snapshot_t *currentSnapshot );

	/**
	 * Updates input-related parts of cgame every frame.
	 *
	 * @param frametime real frame time
	 */
	void ( *InputFrame )( int frameTime );

	/**
	* Transmits accumulated mouse movement event for the current frame.
	*
	* @param dx horizontal mouse movement
	* @param dy vertical mouse movement
	*/
	void ( *MouseMove )( int dx, int dy );

	/**
	 * Gets input command buttons added by cgame.
	 * May be called multiple times in a frame.
	 *
	 * @return BUTTON_ bitfield with the pressed or simulated actions
	 */
	unsigned int ( *GetButtonBits )( void );

	/**
	 * Adds input view rotation.
	 * May be called multiple times in a frame.
	 *
	 * @param viewAngles view angles to modify
	 */
	void ( *AddViewAngles )( vec3_t viewAngles );

	/**
	 * Adds player movement.
	 * May be called multiple times in a frame.
	 *
	 * @param movement movement vector to modify
	 */
	void ( *AddMovement )( vec3_t movement );
} cgame_export_t;

cgame_export_t *GetCGameAPI( cgame_import_t * import );
