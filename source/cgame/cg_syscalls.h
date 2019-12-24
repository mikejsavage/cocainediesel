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

extern cgame_import_t CGAME_IMPORT;

static inline void trap_Print( const char *msg ) {
	CGAME_IMPORT.Print( msg );
}

static inline void trap_PrintToLog( const char *msg ) {
	CGAME_IMPORT.PrintToLog( msg );
}

#ifndef _MSC_VER
static inline void trap_Error( const char *msg ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) static inline void trap_Error( const char *msg );
#endif

static inline void trap_Error( const char *msg ) {
	CGAME_IMPORT.Error( msg );
}

static inline const char *trap_Key_GetBindingBuf( int binding ) {
	return CGAME_IMPORT.Key_GetBindingBuf( binding );
}

static inline const char *trap_Key_KeynumToString( int keynum ) {
	return CGAME_IMPORT.Key_KeynumToString( keynum );
}

static inline void trap_GetConfigString( int i, char *str, int size ) {
	CGAME_IMPORT.GetConfigString( i, str, size );
}

static inline int64_t trap_Milliseconds( void ) {
	return CGAME_IMPORT.Milliseconds();
}

static inline bool trap_DownloadRequest( const char *filename ) {
	return CGAME_IMPORT.DownloadRequest( filename );
}

static inline void trap_NET_GetUserCmd( int frame, usercmd_t *cmd ) {
	CGAME_IMPORT.NET_GetUserCmd( frame, cmd );
}

static inline int trap_NET_GetCurrentUserCmdNum( void ) {
	return CGAME_IMPORT.NET_GetCurrentUserCmdNum();
}

static inline void trap_NET_GetCurrentState( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent ) {
	CGAME_IMPORT.NET_GetCurrentState( incomingAcknowledged, outgoingSequence, outgoingSent );
}

static inline void trap_VID_FlashWindow() {
	CGAME_IMPORT.VID_FlashWindow();
}

static inline struct cmodel_s *trap_CM_InlineModel( int num ) {
	return CGAME_IMPORT.CM_InlineModel( num );
}

static inline struct cmodel_s *trap_CM_ModelForBBox( vec3_t mins, vec3_t maxs ) {
	return CGAME_IMPORT.CM_ModelForBBox( mins, maxs );
}

static inline struct cmodel_s *trap_CM_OctagonModelForBBox( vec3_t mins, vec3_t maxs ) {
	return CGAME_IMPORT.CM_OctagonModelForBBox( mins, maxs );
}

static inline int trap_CM_NumInlineModels( void ) {
	return CGAME_IMPORT.CM_NumInlineModels();
}

static inline void trap_CM_TransformedBoxTrace( trace_t *tr, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, struct cmodel_s *cmodel, int brushmask, const vec3_t origin, const vec3_t angles ) {
	CGAME_IMPORT.CM_TransformedBoxTrace( tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}

static inline int trap_CM_TransformedPointContents( const vec3_t p, struct cmodel_s *cmodel, const vec3_t origin, const vec3_t angles ) {
	return CGAME_IMPORT.CM_TransformedPointContents( p, cmodel, origin, angles );
}

static inline void trap_CM_InlineModelBounds( const struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs ) {
	CGAME_IMPORT.CM_InlineModelBounds( cmodel, mins, maxs );
}

static inline bool trap_CM_InPVS( const vec3_t p1, const vec3_t p2 ) {
	return CGAME_IMPORT.CM_InPVS( p1, p2 );
}
