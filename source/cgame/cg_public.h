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
// functions exported by the client game subsystem
//
struct cgame_export_t {
	// the init function will be called at each restart
	void ( *Init )( unsigned int playerNum, bool demoplaying, const char *demoName, unsigned int snapFrameTime );

	// "soft restarts" at demo jumps
	void ( *Reset )();

	void ( *Shutdown )();

	void ( *ConfigString )( int number );

	void ( *EscapeKey )();

	void ( *RenderView )( unsigned extrapolationTime );

	bool ( *NewFrameSnapshot )( snapshot_t *newSnapshot, snapshot_t *currentSnapshot );

	void ( *MouseMove )( int frameTime, Vec2 m );

	u8 ( *GetButtonBits )();
	u8 ( *GetButtonDownEdges )();
};

cgame_export_t *GetCGameAPI();
