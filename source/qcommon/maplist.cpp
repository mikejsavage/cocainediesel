#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/maplist.h"

#include "nanosort/nanosort.hpp"

static NonRAIIDynamicArray< Span< char > > maps;

void InitMapList() {
	TracyZoneScoped;

	maps.init( sys_allocator );
	RefreshMapList( sys_allocator );
}

static void FreeMaps() {
	for( Span< char > map : maps ) {
		Free( sys_allocator, map.ptr );
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
	defer { Free( a, path ); };

	ListDirHandle scan = BeginListDir( a, path );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		if( dir )
			continue;

		if( FileExtension( name ) == ".cdmap" || FileExtension( StripExtension( name ) ) == ".cdmap" ) {
			maps.add( CloneSpan( sys_allocator, StripExtension( StripExtension( name ) ) ) );
		}
	}

	nanosort( maps.begin(), maps.end(), SortSpanStringsComparator );
}

Span< Span< const char > > GetMapList() {
	return maps.span().cast< Span< const char > >();
}

bool MapExists( Span< const char > name ) {
	for( Span< const char > map : maps ) {
		if( StrEqual( map, name ) ) {
			return true;
		}
	}

	return false;
}

Span< Span< const char > > CompleteMapName( TempAllocator * a, Span< const char > prefix ) {
	NonRAIIDynamicArray< Span< const char > > completions( a );

	for( Span< const char > map : maps ) {
		if( CaseStartsWith( map, prefix ) ) {
			completions.add( map );
		}
	}

	return completions.span();
}
