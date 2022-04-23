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

#include "qcommon/qcommon.h"
#include "qcommon/version.h"
#include "qcommon/fs.h"

#define DEMO_SAFEWRITE( demofile,msg,force ) \
	if( force || ( msg )->cursize > ( msg )->maxsize / 2 ) \
	{ \
		SNAP_RecordDemoMessage( demofile, msg, 0 ); \
		MSG_Clear( msg ); \
	}

/*
* SNAP_RecordDemoMessage
*
* Writes given message to demofile
*/
void SNAP_RecordDemoMessage( int demofile, const msg_t * msg, size_t offset ) {
	if( !demofile ) {
		return;
	}

	// now write the entire message to the file, prefixed by length
	u32 len = checked_cast< u32 >( msg->cursize - offset );
	if( len == 0 ) {
		return;
	}

	FS_Write( &len, sizeof( u32 ), demofile );
	FS_Write( msg->data + offset, len, demofile );
}

int SNAP_ReadDemoMessage( int demofile, msg_t *msg ) {
	int msglen;
	FS_Read( &msglen, 4, demofile );
	if( msglen == -1 ) {
		return -1;
	}

	if( msglen > MAX_MSGLEN ) {
		Com_Error( "Error reading demo file: msglen > MAX_MSGLEN" );
	}
	if( (size_t )msglen > msg->maxsize ) {
		Com_Error( "Error reading demo file: msglen > msg->maxsize" );
	}

	int read = FS_Read( msg->data, msglen, demofile );
	if( read != msglen ) {
		Com_Error( "Error reading demo file: End of file" );
	}

	msg->cursize = msglen;
	msg->readcount = 0;

	return read;
}

static void SNAP_DemoMetaDataMessage( msg_t *msg, const char *meta_data, size_t meta_data_realsize ) {
	// demoinfo message
	MSG_WriteUint8( msg, svc_demoinfo );

	int demoinfo_len_pos = msg->cursize;
	MSG_WriteInt32( msg, 0 );    // svc_demoinfo length
	int demoinfo_len = msg->cursize;

	int meta_data_ofs_pos = msg->cursize;
	MSG_WriteInt32( msg, 0 );    // meta data start offset
	int meta_data_ofs = msg->cursize;

	if( meta_data_realsize > SNAP_MAX_DEMO_META_DATA_SIZE ) {
		meta_data_realsize = SNAP_MAX_DEMO_META_DATA_SIZE;
	}
	if( meta_data_realsize > 0 ) {
		meta_data_realsize--;
	}

	meta_data_ofs = msg->cursize - meta_data_ofs;
	MSG_WriteInt32( msg, meta_data_realsize );       // real size
	MSG_WriteInt32( msg, SNAP_MAX_DEMO_META_DATA_SIZE ); // max size
	MSG_Write( msg, meta_data, meta_data_realsize );
	MSG_WriteZeroes( msg, SNAP_MAX_DEMO_META_DATA_SIZE - meta_data_realsize );

	int demoinfo_end = msg->cursize;
	demoinfo_len = msg->cursize - demoinfo_len;

	msg->cursize = demoinfo_len_pos;
	MSG_WriteInt32( msg, demoinfo_len ); // svc_demoinfo length
	msg->cursize = meta_data_ofs_pos;
	MSG_WriteInt32( msg, meta_data_ofs );    // meta data start offset

	msg->cursize = demoinfo_end;
}

static void SNAP_RecordDemoMetaDataMessage( int demofile, msg_t *msg ) {
	int complevel;

	FS_Flush( demofile );

	complevel = FS_GetCompressionLevel( demofile );
	FS_SetCompressionLevel( demofile, 0 );

	DEMO_SAFEWRITE( demofile, msg, true );

	FS_SetCompressionLevel( demofile, complevel );

	FS_Flush( demofile );
}

void SNAP_BeginDemoRecording( TempAllocator * temp, int demofile, unsigned int spawncount, unsigned int snapFrameTime,
		const char *configstrings, SyncEntityState *baselines ) {
	uint8_t msg_buffer[MAX_MSGLEN];
	msg_t msg = NewMSGWriter( msg_buffer, sizeof( msg_buffer ) );

	SNAP_DemoMetaDataMessage( &msg, "", 0 );

	SNAP_RecordDemoMetaDataMessage( demofile, &msg );

	// serverdata message
	MSG_WriteUint8( &msg, svc_serverdata );
	MSG_WriteUint32( &msg, APP_PROTOCOL_VERSION );
	MSG_WriteInt32( &msg, spawncount );
	MSG_WriteInt16( &msg, (unsigned short)snapFrameTime );
	MSG_WriteInt16( &msg, -1 ); // playernum
	MSG_WriteString( &msg, "" ); // download url

	// config strings
	for( int i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		const char *configstring = configstrings + i * MAX_CONFIGSTRING_CHARS;
		if( configstring[0] ) {
			MSG_WriteUint8( &msg, svc_servercs );
			MSG_WriteString( &msg, ( *temp )( "cs {} \"{}\"", i, configstring ) );

			DEMO_SAFEWRITE( demofile, &msg, false );
		}
	}

	// baselines
	SyncEntityState nullstate;
	memset( &nullstate, 0, sizeof( nullstate ) );

	for( int i = 0; i < MAX_EDICTS; i++ ) {
		const SyncEntityState * base = &baselines[i];
		if( base->number != 0 ) {
			MSG_WriteUint8( &msg, svc_spawnbaseline );
			MSG_WriteDeltaEntity( &msg, &nullstate, base, true );

			DEMO_SAFEWRITE( demofile, &msg, false );
		}
	}

	// client expects the server data to be in a separate packet
	DEMO_SAFEWRITE( demofile, &msg, true );

	MSG_WriteUint8( &msg, svc_servercs );
	MSG_WriteString( &msg, "precache" );

	DEMO_SAFEWRITE( demofile, &msg, true );
}

/*
* SNAP_SetDemoMetaKeyValue
*
* Stores a key-value pair of strings in a buffer in the following format:
* key1\0value1\0key2\0value2\0...keyN\0valueN\0
* The resulting string is ensured to be null-terminated.
*/
size_t SNAP_SetDemoMetaKeyValue( char *meta_data, size_t meta_data_max_size, size_t meta_data_realsize,
								 const char *key, const char *value ) {
	size_t key_size = strlen( key ) + 1;
	size_t value_size = strlen( value ) + 1;

	if( meta_data_realsize + key_size + value_size > meta_data_max_size ) {
		// no space
		Com_Printf( "SNAP_SetDemoMetaKeyValue: omitting value '%s' key '%s'\n", value, key );
		return meta_data_realsize;
	}

	memcpy( meta_data + meta_data_realsize, key, key_size );
	meta_data_realsize += key_size;
	memcpy( meta_data + meta_data_realsize, value, value_size );
	meta_data_realsize += value_size;

	meta_data[meta_data_max_size - 1] = 0;

	return meta_data_realsize;
}

void SNAP_StopDemoRecording( int demofile ) {
	int i = -1;
	FS_Write( &i, 4, demofile );
}

void SNAP_WriteDemoMetaData( const char * filename, const char * meta_data, size_t meta_data_realsize ) {
	uint8_t msg_buffer[MAX_MSGLEN];
	msg_t msg = NewMSGWriter( msg_buffer, sizeof( msg_buffer ) );

	// write to a temp file
	char * tmpn = ( *sys_allocator )( "{}.tmp", filename );
	defer { FREE( sys_allocator, tmpn ); };

	int filenum;
	if( FS_FOpenAbsoluteFile( tmpn, &filenum, FS_WRITE | SNAP_DEMO_GZ ) == -1 ) {
		return;
	}

	SNAP_DemoMetaDataMessage( &msg, meta_data, meta_data_realsize );
	SNAP_RecordDemoMetaDataMessage( filenum, &msg );

	// now open the original file in update mode and overwrite metadata

	// important note: we need to the load the temp file before closing it
	// because in the case of gz compression, closing the file may actually
	// write some data we don't want to copy
	Span< u8 > metadata_only = ReadFileBinary( sys_allocator, tmpn );
	if( metadata_only.ptr != NULL ) {
		FILE * original = OpenFile( sys_allocator, filename, "rb+" );
		bool ok = false;
		if( original != NULL ) {
			ok = WritePartialFile( original, metadata_only.ptr, metadata_only.num_bytes() );
			fclose( original );
		}
		if( !ok ) {
			Com_Printf( "Couldn't overwrite demo metadata\n" );
		}
		FREE( sys_allocator, metadata_only.ptr );
	}

	FS_FCloseFile( filenum );

	if( !RemoveFile( sys_allocator, tmpn ) ) {
		Com_Printf( "Couldn't remove temp demo file: %s\n", tmpn );
	}
}
