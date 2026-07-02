#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/maplist.h"

#include "nanosort/nanosort.hpp"

static NonRAIIDynamicArray< Span< char > > maps;

// TODO(mike 20260701): this should probably be moved into the server so we can
// use the asset system for the client side map list
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

	Span< char > maps_dir = a->sv( "{}/base/maps", RootDirPath() );
	defer { Free( a, maps_dir.ptr ); };

	Span< Span< const char > > files = ListDir( a, maps_dir, ListDir_DontRecurse );
	defer {
		FreeAll( a, files );
		Free( a, files );
	};

	for( Span< const char > map : files ) {
		if( FileExtension( map ) == ".cdmap" || FileExtension( StripExtension( map ) ) == ".cdmap" ) {
			maps.add( CloneSpan( sys_allocator, StripExtension( StripExtension( map + maps_dir.n + 1 ) ) ) );
		}
	}

	nanosort( maps.begin(), maps.end(), SortSpanStringsComparator );

	for( size_t i = 1; i < maps.size(); i++ ) {
		if( StrEqual( maps[ i ], maps[ i - 1 ] ) ) {
			Free( sys_allocator, maps[ i ].ptr );
			maps.remove_swap( i );
			i--;
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
