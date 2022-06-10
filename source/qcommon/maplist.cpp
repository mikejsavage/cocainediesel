#include <algorithm>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/maplist.h"

static NonRAIIDynamicArray< char * > maps;

void InitMapList() {
	TracyZoneScoped;

	maps.init( sys_allocator );
	RefreshMapList( sys_allocator );
}

static void FreeMaps() {
	for( char * map : maps ) {
		FREE( sys_allocator, map );
	}

	maps.clear();
}

void ShutdownMapList() {
	FreeMaps();
	maps.shutdown();
}

void RefreshMapList( Allocator * a ) {
	TracyZoneScoped;

	FreeMaps();

	char * path = ( *a )( "{}/base/maps", RootDirPath() );
	defer { FREE( a, path ); };

	ListDirHandle scan = BeginListDir( a, path );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		if( dir )
			continue;

		if( FileExtension( name ) == ".bsp" || FileExtension( StripExtension( name ) ) == ".bsp" ) {
			char * map = ( *sys_allocator )( "{}", StripExtension( StripExtension( name ) ) );
			maps.add( map );
		}
	}

	std::sort( maps.begin(), maps.end(), SortCStringsComparator );
}

Span< const char * > GetMapList() {
	return maps.span().cast< const char * >();
}

bool MapExists( const char * name ) {
	for( const char * map : maps ) {
		if( StrEqual( map, name ) ) {
			return true;
		}
	}

	return false;
}

Span< const char * > CompleteMapName( TempAllocator * a, const char * prefix ) {
	NonRAIIDynamicArray< const char * > completions( a );

	for( const char * map : maps ) {
		if( CaseStartsWith( map, prefix ) ) {
			completions.add( map );
		}
	}

	return completions.span();
}
