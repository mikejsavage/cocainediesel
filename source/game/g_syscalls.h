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

static inline void trap_Print( const char *msg ) {
	GAME_IMPORT.Print( msg );
}

#ifndef _MSC_VER
static inline void trap_Error( const char *msg ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) static inline void trap_Error( const char *msg );
#endif

static inline void trap_Error( const char *msg ) {
	GAME_IMPORT.Error( msg );
}

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

static inline int64_t trap_Milliseconds( void ) {
	return GAME_IMPORT.Milliseconds();
}

static inline bool trap_inPVS( const vec3_t p1, const vec3_t p2 ) {
	return GAME_IMPORT.inPVS( p1, p2 ) == true;
}

static inline int trap_CM_TransformedPointContents( const vec3_t p, struct cmodel_s *cmodel, const vec3_t origin, const vec3_t angles ) {
	return GAME_IMPORT.CM_TransformedPointContents( p, cmodel, origin, angles );
}

static inline void trap_CM_TransformedBoxTrace( trace_t *tr, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, struct cmodel_s *cmodel, int brushmask, const vec3_t origin, const vec3_t angles ) {
	GAME_IMPORT.CM_TransformedBoxTrace( tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}

static inline int trap_CM_NumInlineModels( void ) {
	return GAME_IMPORT.CM_NumInlineModels();
}

static inline struct cmodel_s *trap_CM_InlineModel( int num ) {
	return GAME_IMPORT.CM_InlineModel( num );
}

static inline void trap_CM_InlineModelBounds( const struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs ) {
	GAME_IMPORT.CM_InlineModelBounds( cmodel, mins, maxs );
}

static inline struct cmodel_s *trap_CM_ModelForBBox( vec3_t mins, vec3_t maxs ) {
	return GAME_IMPORT.CM_ModelForBBox( mins, maxs );
}

static inline struct cmodel_s *trap_CM_OctagonModelForBBox( vec3_t mins, vec3_t maxs ) {
	return GAME_IMPORT.CM_OctagonModelForBBox( mins, maxs );
}

static inline void trap_CM_SetAreaPortalState( int area, int otherarea, bool open ) {
	GAME_IMPORT.CM_SetAreaPortalState( area, otherarea, open == true ? true : false );
}

static inline bool trap_CM_AreasConnected( int area1, int area2 ) {
	return GAME_IMPORT.CM_AreasConnected( area1, area2 ) == true;
}

static inline int trap_CM_BoxLeafnums( vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode ) {
	return GAME_IMPORT.CM_BoxLeafnums( mins, maxs, list, listsize, topnode );
}
static inline int trap_CM_LeafCluster( int leafnum ) {
	return GAME_IMPORT.CM_LeafCluster( leafnum );
}
static inline int trap_CM_LeafArea( int leafnum ) {
	return GAME_IMPORT.CM_LeafArea( leafnum );
}
static inline int trap_CM_LeafsInPVS( int leafnum1, int leafnum2 ) {
	return GAME_IMPORT.CM_LeafsInPVS( leafnum1, leafnum2 );
}

static inline bool trap_ML_Update( void ) {
	return GAME_IMPORT.ML_Update() == true;
}

static inline bool trap_ML_FilenameExists( const char *filename ) {
	return GAME_IMPORT.ML_FilenameExists( filename ) == true;
}

static inline size_t trap_ML_GetMapByNum( int num, char *out, size_t size ) {
	return GAME_IMPORT.ML_GetMapByNum( num, out, size );
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
