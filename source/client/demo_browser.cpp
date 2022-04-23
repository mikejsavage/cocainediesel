#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/demo_browser.h"
#include "gameshared/demo.h"

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

// TODO: 1k might not be enough forever
static Span< u8 > ReadFirst1kBytes( TempAllocator * temp, const char * path ) {
	FILE * f = OpenFile( temp, path, OpenFile_Read );
	if( f == NULL )
		return Span< u8 >();

	u8 * buf = ALLOC_MANY( temp, u8, 1024 );
	size_t n;
	if( !ReadPartialFile( f, buf, 1024, &n ) ) {
		return Span< u8 >();
	}

	return Span< u8 >( buf, n );
}

void DemoBrowserFrame() {
	constexpr s64 microseconds_to_spend_per_frame = 2000;
	s64 start_time = Sys_Microseconds();

	while( metadata_load_cursor < demos.size() && Sys_Microseconds() - start_time < microseconds_to_spend_per_frame ) {
		DemoBrowserEntry * demo = &demos[ metadata_load_cursor ];
		metadata_load_cursor++;

		TempAllocator temp = cls.frame_arena.temp();

		const char * path = temp( "{}/demos/{}", HomeDirPath(), demo->path );
		Span< u8 > first_1k = ReadFirst1kBytes( &temp, path );

		DemoMetadata metadata;
		if( !ReadDemoMetadata( &temp, &metadata, first_1k ) )
			continue;

		demo->have_details = true;
		ggformat( demo->server, sizeof( demo->server ), "{}", metadata.server );
		ggformat( demo->map, sizeof( demo->map ), "{}", metadata.map );
		ggformat( demo->version, sizeof( demo->version ), "{}", metadata.game_version );
		Sys_FormatTimestamp( demo->date, sizeof( demo->date ), "%Y-%m-%d %H:%M", metadata.utc_time );
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
