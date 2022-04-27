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

#include <errno.h>

#include "qcommon/base.h"
#include "qcommon/version.h"
#include "qcommon/array.h"
#include "qcommon/compression.h"
#include "qcommon/fs.h"
#include "qcommon/serialization.h"
#include "gameshared/demo.h"

#include "zstd/zstd.h"

static void Serialize( SerializationBuffer * buf, DemoMetadata & meta ) {
	*buf & meta.metadata_version & meta.server & meta.map & meta.game_version & meta.utc_time;

	if( meta.metadata_version >= 2 ) {
		*buf & meta.duration_seconds;
	}
	else if( !buf->serializing ) {
		meta.duration_seconds = 123;
	}
}

static void FlushDemo( RecordDemoContext * ctx, bool last ) {
	ZSTD_inBuffer in = { ctx->in_buf, ctx->in_buf_cursor };

	while( true ) {
		ZSTD_outBuffer out = { ctx->out_buf, ctx->out_buf_capacity };
		size_t remaining = ZSTD_compressStream2( ctx->zstd, &out, &in, last ? ZSTD_e_end : ZSTD_e_continue );

		if( !WritePartialFile( ctx->temp_file, out.dst, out.pos ) )
			break;

		bool done = last ? remaining == 0 : in.pos == ctx->in_buf_cursor;
		if( done )
			break;
	}
}

static void WriteToDemo( RecordDemoContext * ctx, const void * buf, size_t n ) {
	size_t cursor = 0;
	while( cursor < n ) {
		size_t to_copy = Min2( n, ctx->in_buf_capacity - ctx->in_buf_cursor );
		memcpy( ( u8 * ) ctx->in_buf + ctx->in_buf_cursor, ( u8 * ) buf + cursor, to_copy );
		ctx->in_buf_cursor += to_copy;
		cursor += to_copy;

		if( ctx->in_buf_cursor == ctx->in_buf_capacity ) {
			FlushDemo( ctx, false );
			ctx->in_buf_cursor = 0;
		}
	}
}

void WriteDemoMessage( RecordDemoContext * ctx, msg_t msg, size_t skip ) {
	assert( skip <= msg.cursize );
	u16 len = checked_cast< u16 >( msg.cursize - skip );
	if( len == 0 ) {
		return;
	}

	WriteToDemo( ctx, &len, sizeof( len ) );
	WriteToDemo( ctx, msg.data + skip, len );
}

static void MaybeWriteDemoMessage( RecordDemoContext * ctx, msg_t * msg, bool force ) {
	if( !force && msg->cursize <= msg->maxsize / 2 )
		return;

	WriteDemoMessage( ctx, *msg, 0 );
	MSG_Clear( msg );
}

static void CheckedZstdSetParameter( ZSTD_CStream * zstd, ZSTD_cParameter parameter, int value ) {
	size_t err = ZSTD_CCtx_setParameter( zstd, parameter, value );
	if( ZSTD_isError( err ) ) {
		Fatal( "ZSTD_CCtx_setParameter( {}, {} ): {}", parameter, value, ZSTD_getErrorName( err ) );
	}
}

bool StartRecordingDemo(
	TempAllocator * temp, RecordDemoContext * ctx, const char * filename, unsigned int spawncount, unsigned int snapFrameTime,
	int max_clients, const char * configstrings, SyncEntityState * baselines
) {
	*ctx = { };

	ctx->file = OpenFile( temp, filename, OpenFile_ReadWriteOverwrite );
	if( ctx->file == NULL ) {
		Com_Printf( S_COLOR_YELLOW "Can't open %s for writing\n", filename );
		return false;
	}

	ctx->temp_filename = ( *sys_allocator )( "{}.tmp", filename );
	ctx->temp_file = OpenFile( temp, ctx->temp_filename, OpenFile_ReadWriteOverwrite );
	if( ctx->temp_file == NULL ) {
		Com_Printf( S_COLOR_YELLOW "Can't open %s for writing\n", ctx->temp_filename );
		CloseFile( ctx->file );
		FREE( sys_allocator, ctx->temp_file );
		return false;
	}
	ctx->filename = CopyString( sys_allocator, filename );

	ctx->zstd = ZSTD_createCStream();
	if( ctx->zstd == NULL ) {
		Fatal( "ZSTD_createCStream" );
	}
	CheckedZstdSetParameter( ctx->zstd, ZSTD_c_compressionLevel, ZSTD_CLEVEL_DEFAULT );
	CheckedZstdSetParameter( ctx->zstd, ZSTD_c_checksumFlag, 1 );

	ctx->in_buf_capacity = ZSTD_CStreamInSize();
	ctx->in_buf = ALLOC_SIZE( sys_allocator, ctx->in_buf_capacity, 16 );
	ctx->out_buf_capacity = ZSTD_CStreamOutSize();
	ctx->out_buf = ALLOC_SIZE( sys_allocator, ctx->out_buf_capacity, 16 );

	uint8_t msg_buffer[MAX_MSGLEN];
	msg_t msg = NewMSGWriter( msg_buffer, sizeof( msg_buffer ) );

	// serverdata message
	MSG_WriteUint8( &msg, svc_serverdata );
	MSG_WriteUint32( &msg, APP_PROTOCOL_VERSION );
	MSG_WriteInt32( &msg, spawncount );
	MSG_WriteInt16( &msg, (unsigned short)snapFrameTime );
	MSG_WriteUint8( &msg, max_clients );
	MSG_WriteInt16( &msg, -1 ); // playernum
	MSG_WriteString( &msg, filename ); // server name
	MSG_WriteString( &msg, "" ); // download url

	// config strings
	for( int i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		const char *configstring = configstrings + i * MAX_CONFIGSTRING_CHARS;
		if( configstring[0] ) {
			MSG_WriteUint8( &msg, svc_servercs );
			MSG_WriteString( &msg, ( *temp )( "cs {} \"{}\"", i, configstring ) );

			MaybeWriteDemoMessage( ctx, &msg, false );
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

			MaybeWriteDemoMessage( ctx, &msg, false );
		}
	}

	// client expects the server data to be in a separate packet
	MaybeWriteDemoMessage( ctx, &msg, true );

	MSG_WriteUint8( &msg, svc_servercs );
	MSG_WriteString( &msg, "precache" );

	MaybeWriteDemoMessage( ctx, &msg, true );

	return true;
}

void StopRecordingDemo( TempAllocator * temp, RecordDemoContext * ctx, const DemoMetadata & metadata ) {
	assert( metadata.metadata_version == DEMO_METADATA_VERSION );

	FlushDemo( ctx, true );

	bool ok = true;
	defer {
		if( !ok ) {
			Com_Printf( S_COLOR_YELLOW "Something went wrong writing the demo: %s\n", strerror( errno ) );
		}
		fclose( ctx->file );
		if( !ok ) {
			RemoveFile( temp, ctx->filename );
		}
		FREE( sys_allocator, ctx->filename );

		fclose( ctx->temp_file );
		RemoveFile( temp, ctx->temp_filename );
		FREE( sys_allocator, ctx->temp_filename );

		ZSTD_freeCStream( ctx->zstd );
		FREE( sys_allocator, ctx->in_buf );
		FREE( sys_allocator, ctx->out_buf );
	};

	if( ferror( ctx->temp_file ) ) {
		ok = false;
		return;
	}

	// serialise metadata to demo file
	DynamicArray< u8 > serialised_metadata( temp );
	Serialize( metadata, &serialised_metadata );

	DemoHeader header;
	memcpy( &header.magic, DEMO_METADATA_MAGIC, sizeof( DEMO_METADATA_MAGIC ) );
	header.metadata_size = serialised_metadata.num_bytes();
	ok = ok && WritePartialFile( ctx->file, &header, sizeof( header ) );
	ok = ok && WritePartialFile( ctx->file, serialised_metadata.ptr(), serialised_metadata.num_bytes() );

	// stream snapshots from temp file to demo
	Seek( ctx->temp_file, 0 );
	while( ok ) {
		char buf[ BUFSIZ ];
		size_t r = 0;
		ok = ok && ReadPartialFile( ctx->temp_file, buf, sizeof( buf ), &r );
		if( r == 0 )
			break;

		ok = ok && WritePartialFile( ctx->file, buf, r );
	}
}

static const DemoHeader * ReadDemoHeader( Span< const u8 > contents ) {
	if( contents.n < sizeof( DemoHeader ) )
		return NULL;

	const DemoHeader * header = ( const DemoHeader * ) contents.ptr;
	if( memcmp( &header->magic, DEMO_METADATA_MAGIC, sizeof( DEMO_METADATA_MAGIC ) ) != 0 )
		return NULL;

	if( contents.n < sizeof( DemoHeader ) + header->metadata_size )
		return NULL;

	return header;
}

bool ReadDemoMetadata( Allocator * a, DemoMetadata * metadata, Span< const u8 > contents ) {
	const DemoHeader * header = ReadDemoHeader( contents );
	if( header == NULL )
		return false;

	Span< const u8 > serialised_metadata = contents.slice( sizeof( DemoHeader ), sizeof( DemoHeader ) + header->metadata_size );
	return Deserialize( a, metadata, serialised_metadata.ptr, serialised_metadata.n );
}

bool DecompressDemo( Allocator * a, Span< u8 > * decompressed, Span< const u8 > contents ) {
	const DemoHeader * header = ReadDemoHeader( contents );
	if( header == NULL )
		return false;
	return Decompress( "TODO", a, contents.slice( sizeof( DemoHeader ) + header->metadata_size, contents.n ), decompressed );
}
