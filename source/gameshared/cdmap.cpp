#include "qcommon/types.h"
#include "gameshared/cdmap.h"

#include <string.h>

template< typename T >
static bool DecodeMapSection( Span< T > * span, Span< const u8 > data, MapSectionType type ) {
	const MapHeader * header = ( const MapHeader * ) data.ptr;
	MapSection section = header->sections[ type ];

	if( section.offset + section.size > data.n )
		return false;

	if( section.size % sizeof( T ) != 0 )
		return false;

	*span = ( data + section.offset ).slice( 0, section.size ).cast< const T >();
	return true;
}

DecodeMapResult DecodeMap( MapData * map, Span< const u8 > data ) {
	if( data.n < sizeof( MapHeader ) )
		return DecodeMapResult_NotAMap;

	const MapHeader * header = ( const MapHeader * ) data.ptr;
	if( memcmp( header->magic, CDMAP_MAGIC, sizeof( header->magic ) ) != 0 )
		return DecodeMapResult_NotAMap;

	if( header->format_version != CDMAP_FORMAT_VERSION )
		return DecodeMapResult_WrongFormatVersion;

	bool ok = true;
	ok = ok && DecodeMapSection( &map->entities, data, MapSection_Entities );
	ok = ok && DecodeMapSection( &map->entity_data, data, MapSection_EntityData );
	ok = ok && DecodeMapSection( &map->entity_kvs, data, MapSection_EntityKeyValues );
	ok = ok && DecodeMapSection( &map->models, data, MapSection_Models );
	ok = ok && DecodeMapSection( &map->nodes, data, MapSection_Nodes );
	ok = ok && DecodeMapSection( &map->brushes, data, MapSection_Brushes );
	ok = ok && DecodeMapSection( &map->brush_indices, data, MapSection_BrushIndices );
	ok = ok && DecodeMapSection( &map->brush_planes, data, MapSection_BrushPlanes );
	ok = ok && DecodeMapSection( &map->meshes, data, MapSection_Meshes );
	ok = ok && DecodeMapSection( &map->vertices, data, MapSection_Vertices );
	ok = ok && DecodeMapSection( &map->vertex_indices, data, MapSection_VertexIndices );

	return ok ? DecodeMapResult_Ok : DecodeMapResult_NotAMap;
}
