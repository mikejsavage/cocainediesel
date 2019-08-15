#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"

// TODO: mempool asset data

struct Asset {
	char name[ 256 ];
	const void * data;
	size_t len;
};

constexpr u32 MAX_ASSETS = 4096;

static Asset assets[ MAX_ASSETS ];
static const char * asset_names[ MAX_ASSETS ];
static u32 num_assets;

static Hashtable< MAX_ASSETS * 2 > assets_hashtable;

void Assets_Init() {
	num_assets = 0;
	assets_hashtable.clear();
	for( u32 i = 0; i < MAX_ASSETS; i++ ) {
		asset_names[ i ] = assets[ i ].name;
	}
}

const void * Assets_Data( StringHash name, size_t * len ) {
	size_t i;
	if( !assets_hashtable.get( name.hash, &i ) )
		return NULL;

	if( len != NULL )
		*len = assets[ i ].len;
	return assets[ i ].data;
}

static void RegisterAsset( const char * name, const void * data, size_t len ) {
	if( num_assets == MAX_ASSETS )
		return;

	Asset * a = &assets[ num_assets ];
	Q_strncpyz( a->name, name, sizeof( a->name ) );
	a->data = data;
	a->len = len;

	assets_hashtable.add( Hash64( name, strlen( name ) ), num_assets );
	num_assets++;
}

static void Assets_LoadFromFSImpl( const char * path, size_t skip ) {
	ListDirHandle scan = FS_BeginListDir( path );

	const char * name;
	bool dir;
	while( FS_ListDirNext( scan, &name, &dir ) ) {
		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		char full_path[ 256 ];
		Q_snprintfz( full_path, sizeof( full_path ), "%s/%s", path, name );
		if( dir ) {
			Assets_LoadFromFSImpl( full_path, skip );
		}
		else {
			size_t len;
			void * contents = FS_ReadEntireFile( full_path, &len );
			if( contents != NULL )
				RegisterAsset( full_path + skip, contents, len );
		}
	}

	FS_EndListDir( scan );
}

void Assets_LoadFromFS() {
	// TODO: should probably tempalloc this
	char assets_path[ 256 ];
	Q_snprintfz( assets_path, sizeof( assets_path ), "%s/base", FS_RootPath() );
	Assets_LoadFromFSImpl( assets_path, strlen( assets_path ) + 1 );
}

void Assets_Shutdown() {
	for( u32 i = 0; i < num_assets; i++ ) {
		free( const_cast< void * >( assets[ i ].data ) );
	}
}

Span< const char * > Assets_Names() {
	return Span< const char * >( asset_names, num_assets );
}

#if 0
int main() {
	FS_Init();
	Assets_Init();
	Assets_LoadFromFS();

	for( u32 i = 0; i < num_assets; i++ ) {
		printf( "%s\n", asset_names[ i ] );
	}

	printf( "%s\n", ( const char * ) Assets_Data( "glsl/include/fog.glsl" ) );

	Assets_Shutdown();

	return 0;
}
#endif
