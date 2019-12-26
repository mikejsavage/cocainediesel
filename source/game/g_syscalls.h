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

#ifdef GAME_HARD_LINKED
#define GAME_IMPORT gi_imp_local
#endif

extern game_import_t GAME_IMPORT;

static inline void trap_GameCmd( struct edict_s *ent, const char *cmd ) {
	GAME_IMPORT.GameCmd( ent, cmd );
}

static inline void trap_ConfigString( int num, const char *string ) {
	GAME_IMPORT.ConfigString( num, string );
}

static inline const char *trap_GetConfigString( int num ) {
	return GAME_IMPORT.GetConfigString( num );
}

static inline int trap_ModelIndex( const char *name ) {
	return GAME_IMPORT.ModelIndex( name );
}

static inline int trap_SoundIndex( const char *name ) {
	return GAME_IMPORT.SoundIndex( name );
}

static inline int trap_ImageIndex( const char *name ) {
	return GAME_IMPORT.ImageIndex( name );
}

static inline int trap_FakeClientConnect( char *fakeUserinfo, char *fakeSocketType, const char *fakeIP ) {
	return GAME_IMPORT.FakeClientConnect( fakeUserinfo, fakeSocketType, fakeIP );
}

static inline int trap_GetClientState( int numClient ) {
	return GAME_IMPORT.GetClientState( numClient );
}

static inline void trap_ExecuteClientThinks( int clientNum ) {
	GAME_IMPORT.ExecuteClientThinks( clientNum );
}

static inline void trap_DropClient( edict_t *ent, int type, const char *message ) {
	GAME_IMPORT.DropClient( ent, type, message );
}

static inline void trap_LocateEntities( struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts ) {
	GAME_IMPORT.LocateEntities( edicts, edict_size, num_edicts, max_edicts );
}
