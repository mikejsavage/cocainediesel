#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/time.h"
#include "client/client.h"
#include "client/demo_browser.h"
#include "gameshared/demo.h"

#include "nanosort/nanosort.hpp"

static NonRAIIDynamicArray< DemoBrowserEntry > demos;
static size_t metadata_load_cursor;

void InitDemoBrowser() {
	TracyZoneScoped;
	demos.init( sys_allocator );
}

static void ClearDemos() {
	for( DemoBrowserEntry & demo : demos ) {
		Free( sys_allocator, demo.path );
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
	defer { fclose( f ); };

	u8 * buf = AllocMany< u8 >( temp, 1024 );
	size_t n;
	if( !ReadPartialFile( f, buf, 1024, &n ) ) {
		return Span< u8 >();
	}

	return Span< u8 >( buf, n );
}

void DemoBrowserFrame() {
	constexpr Time time_to_spend_per_frame = Milliseconds( 2 );
	Time start_time = Now();

	while( metadata_load_cursor < demos.size() && Now() - start_time < time_to_spend_per_frame ) {
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
		FormatTimestamp( demo->date, sizeof( demo->date ), "%Y-%m-%d %H:%M", metadata.utc_time );
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
		else if( FileExtension( path->span() + skip ) == APP_DEMO_EXTENSION_STR ) {
			DemoBrowserEntry demo = { };
			demo.path = ( *sys_allocator )( "{}", path->span() + skip );
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

	nanosort( demos.begin(), demos.end(), []( const DemoBrowserEntry & a, const DemoBrowserEntry & b ) {
		return !SortCStringsComparator( a.path, b.path );
	} );
}
