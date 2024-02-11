#include "qcommon/types.h"
#include "gameshared/cdmap.h"
#include "gameshared/q_shared.h"

#include <string.h>

template< typename T >
static bool DecodeMapSection( Span< T > * span, Span< const u8 > data, MapSectionType type ) {
	const MapHeader * header = align_cast< const MapHeader >( data.ptr );
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

	const MapHeader * header = align_cast< const MapHeader >( data.ptr );
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
	ok = ok && DecodeMapSection( &map->vertex_positions, data, MapSection_VertexPositions );
	ok = ok && DecodeMapSection( &map->vertex_normals, data, MapSection_VertexNormals );
	ok = ok && DecodeMapSection( &map->vertex_indices, data, MapSection_VertexIndices );

	return ok ? DecodeMapResult_Ok : DecodeMapResult_NotAMap;
}

Span< const char > GetWorldspawnKey( const MapData * map, const char * key ) {
	const MapEntity * worldspawn = &map->entities[ 0 ];

	for( u32 i = 0; i < worldspawn->num_key_values; i++ ) {
		const MapEntityKeyValue * kv = &map->entity_kvs[ i + worldspawn->first_key_value ];
		Span< const char > k = map->entity_data.slice( kv->offset, kv->offset + kv->key_size );
		if( StrEqual( k, key ) ) {
			return map->entity_data.slice( kv->offset + kv->key_size, kv->offset + kv->key_size + kv->value_size );
		}
	}

	return Span< const char >();
}
