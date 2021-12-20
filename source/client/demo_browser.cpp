#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/time.h"
#include "client/client.h"
#include "client/demo_browser.h"

static NonRAIIDynamicArray< DemoBrowserEntry > demos;
static size_t metadata_load_cursor;

void InitDemoBrowser() {
	demos.init( sys_allocator );
}

static void ClearDemos() {
	for( DemoBrowserEntry & demo : demos ) {
		FREE( sys_allocator, demo.path );
	}
	demos.clear();
	metadata_load_cursor = 0;
}

void ShutdownDemoBrowser() {
	ClearDemos();
	demos.shutdown();
}

Span< const DemoBrowserEntry > GetDemoBrowserEntries() {
	return demos.span();
}

static Span< u8 > ReadFirst1kBytes( const char * path ) {
	TempAllocator temp = cls.frame_arena.temp();
	FILE * f = OpenFile( &temp, path, "r" );
	if( f == NULL )
		return Span< u8 >();

	u8 * buf = ALLOC_MANY( sys_allocator, u8, 1024 );
	size_t n;
	if( !ReadPartialFile( f, buf, 1024, &n ) )
		return Span< u8 >();

	return Span< u8 >( buf, n );
}

static Span< const char > GetDemoMetadata( Span< const u8 > demo ) {
	constexpr size_t length_offset = 48;
	constexpr size_t metadata_offset = 56;
	if( demo.n < length_offset + sizeof( u32 ) ) {
		return Span< const char >();
	}

	u32 length;
	memcpy( &length, &demo[ length_offset ], sizeof( length ) );

	if( demo.n < metadata_offset + length ) {
		return Span< const char >();
	}

	return demo.slice( metadata_offset, metadata_offset + length ).cast< const char >();
}

static Span< const char > GetDemoKey( Span< const char > metadata, const char * key ) {
	if( metadata.n == 0 )
		return Span< const char >();

	const char * cursor = metadata.ptr;

	while( true ) {
		if( strlen( cursor ) == 0 )
			break;

		if( StrEqual( key, cursor ) ) {
			return MakeSpan( cursor + strlen( key ) + 1 );
		}

		cursor += strlen( cursor ) + 1;
		cursor += strlen( cursor ) + 1;
	}

	return Span< const char >();
}

void DemoBrowserFrame() {
	constexpr Time time_to_spend_per_frame = Milliseconds( 2 );
	Time start_time = Now();

	while( metadata_load_cursor < demos.size() && Now() - start_time < time_to_spend_per_frame ) {
		DemoBrowserEntry * demo = &demos[ metadata_load_cursor ];
		metadata_load_cursor++;

		TempAllocator temp = cls.frame_arena.temp();

		const char * path = temp( "{}/demos/{}", HomeDirPath(), demo->path );
		Span< u8 > first_1k = ReadFirst1kBytes( path );
		defer { FREE( sys_allocator, first_1k.ptr ); };

		Span< const char > metadata = GetDemoMetadata( first_1k );

		demo->have_details = true;
		ggformat( demo->server, sizeof( demo->server ), "{}", GetDemoKey( metadata, "hostname" ) );
		ggformat( demo->map, sizeof( demo->map ), "{}", GetDemoKey( metadata, "mapname" ) );

		s64 timestamp = SpanToInt( GetDemoKey( metadata, "localtime" ), 0 );
		Sys_FormatTimestamp( demo->date, sizeof( demo->date ), "%Y-%m-%d %H:%M", timestamp );
	}
}

static void FindDemosRecursive( TempAllocator * temp, DynamicString * path, size_t skip ) {
	ListDirHandle scan = BeginListDir( temp, path->c_str() );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		size_t old_len = path->length();
		path->append( "/{}", name );
		if( dir ) {
			FindDemosRecursive( temp, path, skip );
		}
		else if( FileExtension( path->c_str() + skip ) == APP_DEMO_EXTENSION_STR ) {
			DemoBrowserEntry demo = { };
			demo.path = CopyString( sys_allocator, path->c_str() + skip );
			demos.add( demo );
		}
		path->truncate( old_len );
	}
}

void RefreshDemoBrowser() {
	ClearDemos();

	TempAllocator temp = cls.frame_arena.temp();
	DynamicString base( &temp, "{}/demos", HomeDirPath() );
	FindDemosRecursive( &temp, &base, base.length() + 1 );
}
