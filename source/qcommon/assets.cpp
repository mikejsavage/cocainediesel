#include "qcommon/qcommon.h"
#include "qcommon/base.h"
#include "qcommon/assets.h"
#include "qcommon/fs.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"

struct Asset {
	char * name;
	Span< u8 > data;
};

static constexpr u32 MAX_ASSETS = 4096;

static Asset assets[ MAX_ASSETS ];
static const char * asset_names[ MAX_ASSETS ];
static u32 num_assets;

static Hashtable< MAX_ASSETS * 2 > assets_hashtable;

static void LoadAsset( const char * path, size_t skip ) {
	Span< u8 > contents = FS_ReadEntireFile( sys_allocator, path );
	if( contents.ptr == NULL )
		return;

	const char * name = path + skip;

	Asset * a = &assets[ num_assets ];
	a->name = ALLOC_MANY( sys_allocator, char, strlen( name ) + 1 );
	Q_strncpyz( a->name, name, strlen( name ) + 1 );
	a->data = contents;

	asset_names[ num_assets ] = a->name;
	assets_hashtable.add( Hash64( name, strlen( name ) ), num_assets );
	num_assets++;
}

static void LoadAssetsRecursive( DynamicString * path, size_t skip ) {
	ListDirHandle scan = FS_BeginListDir( path->c_str() );

	const char * name;
	bool dir;
	while( FS_ListDirNext( &scan, &name, &dir ) ) {
		if( num_assets == MAX_ASSETS ) {
			Com_Printf( S_COLOR_YELLOW "Too many assets\n" );
			return;
		}

		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		size_t old_len = path->length();
		path->append( "/{}", name );
		if( dir ) {
			LoadAssetsRecursive( path, skip );
		}
		else {
			LoadAsset( path->c_str(), skip );
		}
		path->truncate( old_len );
	}
}

void InitAssets( TempAllocator * temp ) {
	num_assets = 0;
	assets_hashtable.clear();

	DynamicString path( temp, "{}/base", FS_RootPath() );
	LoadAssetsRecursive( &path, path.length() + 1 );
}

void ShutdownAssets() {
	for( u32 i = 0; i < num_assets; i++ ) {
		FREE( sys_allocator, assets[ i ].name );
		FREE( sys_allocator, assets[ i ].data.ptr );
	}
}

Span< const u8 > AssetData( StringHash name ) {
	size_t i;
	if( !assets_hashtable.get( name.hash, &i ) )
		return Span< const u8 >();
	return assets[ i ].data;
}

Span< const char * > AssetNames() {
	return Span< const char * >( asset_names, num_assets );
}
