#include <algorithm>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"

static DynamicArray< char * > maps( NO_INIT );

void InitMapList() {
	maps.init( sys_allocator );
	RefreshMapList();
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

void RefreshMapList() {
	FreeMaps();

	ListDirHandle scan = BeginListDir( "base/maps" );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		if( dir )
			continue;

		Span< const char > ext = FileExtension( name );
		if( ext != ".bsp" )
			continue;

		size_t len = strlen( name ) - strlen( ".bsp" );
		char * map = ALLOC_MANY( sys_allocator, char, len + 1 );
		memcpy( map, name, len );
		map[ len ] = '\0';

		maps.add( map );
	}

	std::sort( maps.begin(), maps.end(), []( const char * a, const char * b ) {
		return strcmp( a, b ) < 0;
	} );
}

Span< const char * > GetMapList() {
	return maps.span().cast< const char * >();
}

bool MapExists( const char * name ) {
	for( const char * map : maps ) {
		if( strcmp( map, name ) == 0 ) {
			return true;
		}
	}

	return false;
}

const char ** CompleteMapName( const char * prefix ) {
	size_t n = 0;
	for( const char * map : maps ) {
		if( Q_strnicmp( prefix, map, strlen( prefix ) ) == 0 ) {
			n++;
		}
	}

	const char ** buf = ( const char ** ) Mem_TempMalloc( sizeof( const char * ) * ( n + 1 ) );

	size_t i = 0;
	for( const char * map : maps ) {
		if( Q_strnicmp( prefix, map, strlen( prefix ) ) == 0 ) {
			buf[ i ] = map;
			i++;
		}
	}

	return buf;
}
