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
struct entity_s;
struct cmodel_s;

//
// structs and variables shared with the main engine
//

#define MAX_PARSE_GAMECOMMANDS  256

struct gcommand_t {
	bool all;
	uint8_t targets[MAX_CLIENTS / 8];
	size_t commandOffset;           // offset of the data in gamecommandsData
};

#define MAX_PARSE_ENTITIES  1024
struct snapshot_t {
	bool valid;             // cleared if delta parsing was invalid
	int64_t serverFrame;
	int64_t serverTime;    // time in the server when frame was created
	int64_t ucmdExecuted;
	bool delta;
	bool allentities;
	bool multipov;
	int64_t deltaFrameNum;
	int numplayers;
	SyncPlayerState playerState;
	SyncPlayerState playerStates[MAX_CLIENTS];
	int numEntities;
	SyncEntityState parsedEntities[MAX_PARSE_ENTITIES];
	SyncGameState gameState;
	int numgamecommands;
	gcommand_t gamecommands[MAX_PARSE_GAMECOMMANDS];
	char gamecommandsData[( MAX_STRING_CHARS / 16 ) * MAX_PARSE_GAMECOMMANDS];
	size_t gamecommandsDataHead;
};

//===============================================================

//
// functions provided by the main engine
//
struct cgame_import_t {
	void ( *GetConfigString )( int i, char *str, int size );

	void ( *NET_GetUserCmd )( int frame, usercmd_t *cmd );
	int ( *NET_GetCurrentUserCmdNum )( void );
	void ( *NET_GetCurrentState )( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent );

	// refresh system
	void ( *VID_FlashWindow )();
};

//
// functions exported by the client game subsystem
//
struct cgame_export_t {
	// the init function will be called at each restart
	void ( *Init )( const char *serverName, unsigned int playerNum,
					bool demoplaying, const char *demoName, unsigned int snapFrameTime );

	// "soft restarts" at demo jumps
	void ( *Reset )( void );

	void ( *Shutdown )( void );

	void ( *ConfigString )( int number, const char *value );

	void ( *EscapeKey )( void );

	void ( *GetEntitySpatilization )( int entNum, Vec3 * origin, Vec3 * velocity );

	void ( *Trace )( trace_t *tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, int passent, int contentmask );

	void ( *RenderView )( unsigned extrapolationTime );

	bool ( *NewFrameSnapshot )( snapshot_t *newSnapshot, snapshot_t *currentSnapshot );

	void ( *MouseMove )( int frameTime, Vec2 m );

	/**
	 * Gets input command buttons added by cgame.
	 * May be called multiple times in a frame.
	 *
	 * @return BUTTON_ bitfield with the pressed or simulated actions
	 */
	unsigned int ( *GetButtonBits )( void );
};

cgame_export_t *GetCGameAPI( cgame_import_t * import );
