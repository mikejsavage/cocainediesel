#pragma once

#include <vector>

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "qcommon/span2d.h"
#include "gameshared/q_shared.h"

#include "parsing.h"

#include <string.h>

// generic stuff

constexpr Span< const char > NullSpan( NULL, 0 );

inline Span< const char > SkipLiteral( Span< const char > str, const char * lit ) {
	return StartsWith( str, lit ) ? str + strlen( lit ) : NullSpan;
}

inline Span< const char > PEGRange( Span< const char > str, char lo, char hi ) {
	return str.n > 0 && str[ 0 ] >= lo && str[ 0 ] <= hi ? str + 1 : NullSpan;
}

inline Span< const char > PEGSet( Span< const char > str, const char * set ) {
	if( str.n == 0 )
		return NullSpan;

	for( size_t i = 0; i < strlen( set ); i++ ) {
		if( str[ 0 ] == set[ i ] ) {
			return str + 1;
		}
	}

	return NullSpan;
}

inline Span< const char > PEGNotSet( Span< const char > str, const char * set ) {
	if( str.n == 0 )
		return NullSpan;

	for( size_t i = 0; i < strlen( set ); i++ ) {
		if( str[ 0 ] == set[ i ] ) {
			return NullSpan;
		}
	}

	return str + 1;
}

template< typename F >
Span< const char > PEGCapture( Span< const char > * capture, Span< const char > str, F f ) {
	Span< const char > res = f( str );
	if( res.ptr != NULL ) {
		*capture = Span< const char >( str.ptr, res.ptr - str.ptr );
	}
	return res;
}

template< typename F1, typename F2 >
Span< const char > PEGOr( Span< const char > str, F1 parser1, F2 parser2 ) {
	Span< const char > res1 = parser1( str );
	return res1.ptr == NULL ? parser2( str ) : res1;
}

template< typename F1, typename F2, typename F3 >
Span< const char > PEGOr( Span< const char > str, F1 parser1, F2 parser2, F3 parser3 ) {
	Span< const char > res1 = parser1( str );
	if( res1.ptr != NULL )
		return res1;

	Span< const char > res2 = parser2( str );
	if( res2.ptr != NULL )
		return res2;

	return parser3( str );
}

template< typename F >
Span< const char > PEGOptional( Span< const char > str, F parser ) {
	Span< const char > res = parser( str );
	return res.ptr == NULL ? str : res;
}

template< typename F >
Span< const char > PEGNOrMore( Span< const char > str, size_t n, F parser ) {
	size_t parsed = 0;

	while( true ) {
		Span< const char > next = parser( str );
		if( next.ptr == NULL ) {
			return parsed >= n ? str : NullSpan;
		}
		str = next;
		parsed++;
	}
}

template< typename T, typename F >
Span< const char > CaptureNOrMore( std::vector< T > * array, Span< const char > str, size_t n, F parser ) {
	Span< const char > res = PEGNOrMore( str, n, [ &array, &parser ]( Span< const char > str ) {
		T elem;
		Span< const char > res = parser( &elem, str );
		if( res.ptr != NULL ) {
			array->push_back( elem );
		}
		return res;
	} );

	if( res.ptr == NULL ) {
		array->clear();
	}

	return res;
}

template< typename T, typename F >
Span< const char > ParseUpToN( T * array, size_t * num_parsed, size_t n, Span< const char > str, F parser ) {
	*num_parsed = 0;

	while( true ) {
		T elem;
		Span< const char > res = parser( &elem, str );
		if( res.ptr == NULL )
			return str;

		if( *num_parsed == n )
			return NullSpan;

		array[ *num_parsed ] = elem;
		*num_parsed += 1;
		str = res;
	}
}

constexpr const char * whitespace_chars = " \r\n\t";

inline Span< const char > SkipWhitespace( Span< const char > str ) {
	return PEGNOrMore( str, 0, []( Span< const char > str ) {
		return PEGSet( str, whitespace_chars );
	} );
}

static Span< const char > SkipToken( Span< const char > str, const char * token ) {
	str = SkipWhitespace( str );
	str = SkipLiteral( str, token );
	return str;
}

inline Span< const char > ParseWord( Span< const char > * capture, Span< const char > str ) {
	str = SkipWhitespace( str );
	return PEGCapture( capture, str, []( Span< const char > str ) {
		return PEGNOrMore( str, 1, []( Span< const char > str ) {
			return PEGNotSet( str, whitespace_chars );
		} );
	} );
}

template< typename T, size_t N, typename F >
Span< const char > ParseUpToN( StaticArray< T, N > * array, Span< const char > str, F parser ) {
	return ParseUpToN( array->elems, &array->n, N, str, parser );
}

static Span< const char > ParseNOrMoreDigits( size_t n, Span< const char > str ) {
	return PEGNOrMore( str, n, []( Span< const char > str ) {
		return PEGRange( str, '0', '9' );
	} );
}

static Span< const char > PEGCapture( int * capture, Span< const char > str ) {
	Span< const char > capture_str;
	Span< const char > res = PEGCapture( &capture_str, str, []( Span< const char > str ) {
		return ParseNOrMoreDigits( 1, str );
	} );

	if( res.ptr == NULL )
		return NullSpan;

	if( !TrySpanToInt( capture_str, capture ) )
		return NullSpan;

	return res;
}

static Span< const char > PEGCapture( float * capture, Span< const char > str ) {
	Span< const char > capture_str;
	Span< const char > res = PEGCapture( &capture_str, str, []( Span< const char > str ) {
		str = PEGOptional( str, []( Span< const char > str ) { return SkipLiteral( str, "-" ); } );
		str = ParseNOrMoreDigits( 1, str );
		str = PEGOptional( str, []( Span< const char > str ) {
			str = SkipLiteral( str, "." );
			str = ParseNOrMoreDigits( 0, str );
			return str;
		} );
		return str;
	} );

	if( res.ptr == NULL )
		return NullSpan;

	if( !TrySpanToFloat( capture_str, capture ) )
		return NullSpan;

	return res;
}

static Span< const char > ParseFloat( float * x, Span< const char > str ) {
	str = SkipWhitespace( str );
	str = PEGCapture( x, str );
	return str;
}

static Span< const char > ParseVec3( Vec3 * v, Span< const char > str ) {
	str = SkipToken( str, "(" );
	for( size_t i = 0; i < 3; i++ ) {
		str = ParseFloat( &v->ptr()[ i ], str );
	}
	str = SkipToken( str, ")" );
	return str;
}

static Span< const char > ParsePlane( Vec3 * points, Span< const char > str ) {
	for( size_t i = 0; i < 3; i++ ) {
		str = ParseVec3( &points[ i ], str );
	}
	return str;
}

// map specific stuff

static Span< const char > SkipFlags( Span< const char > str ) {
	str = PEGOr( str,
		[]( Span< const char > str ) { return SkipToken( str, "0" ); },
		[]( Span< const char > str ) { return SkipToken( str, "134217728" ); } // detail bit
	);
	str = SkipToken( str, "0" );
	str = SkipToken( str, "0" );
	return str;
}

static Span< const char > ParseQ1Face( ParsedBrushFace * face, Span< const char > str ) {
	str = ParsePlane( face->plane, str );
	str = ParseWord( &face->material, str );

	constexpr StringHash base_hash = "textures/";
	face->material_hash = Hash64( face->material.ptr, face->material.num_bytes(), base_hash.hash );

	float u, v, angle, scale_x, scale_y;
	str = ParseFloat( &u, str );
	str = ParseFloat( &v, str );
	str = ParseFloat( &angle, str );
	str = ParseFloat( &scale_x, str );
	str = ParseFloat( &scale_y, str );

	// TODO: convert to transform

	str = PEGOptional( str, SkipFlags );

	return str;
}

static Span< const char > ParseQ1Brush( ParsedBrush * brush, Span< const char > str ) {
	str = SkipToken( str, "{" );
	str = ParseUpToN( &brush->faces, str, ParseQ1Face );
	str = SkipToken( str, "}" );
	return brush->faces.n >= 4 ? str : NullSpan;
}

static Span< const char > ParseQ3Face( ParsedBrushFace * face, Span< const char > str ) {
	str = ParsePlane( face->plane, str );

	Vec3 uv_row1 = { };
	Vec3 uv_row2 = { };
	str = SkipToken( str, "(" );
	str = ParseVec3( &uv_row1, str );
	str = ParseVec3( &uv_row2, str );
	str = SkipToken( str, ")" );

	face->texcoords_transform = Mat3(
		uv_row1.x, uv_row1.y, uv_row1.z,
		uv_row2.x, uv_row2.y, uv_row2.z,
		0.0f, 0.0f, 1.0f
	);

	str = ParseWord( &face->material, str );
	constexpr u64 base_hash = Hash64_CT( "textures/", 9 );
	face->material_hash = Hash64( face->material.ptr, face->material.num_bytes(), base_hash );
	str = SkipFlags( str );

	return str;
}

static Span< const char > ParseQ3Brush( ParsedBrush * brush, Span< const char > str ) {
	str = SkipToken( str, "{" );
	str = SkipToken( str, "brushDef" );
	str = SkipToken( str, "{" );
	str = ParseUpToN( &brush->faces, str, ParseQ3Face );
	str = SkipToken( str, "}" );
	str = SkipToken( str, "}" );
	return brush->faces.n >= 4 ? str : NullSpan;
}

static Span< const char > ParsePatch( ParsedPatch * patch, Span< const char > str ) {
	patch->w = 0;
	patch->h = 0;

	str = SkipToken( str, "{" );
	str = SkipToken( str, "patchDef2" );
	str = SkipToken( str, "{" );

	str = ParseWord( &patch->material, str );
	constexpr u64 base_hash = Hash64_CT( "textures/", 9 );
	patch->material_hash = Hash64( patch->material.ptr, patch->material.num_bytes(), base_hash );

	str = SkipToken( str, "(" );

	str = SkipWhitespace( str );
	str = PEGCapture( &patch->w, str );
	str = SkipWhitespace( str );
	str = PEGCapture( &patch->h, str );
	str = SkipFlags( str );

	str = SkipToken( str, ")" );

	str = SkipToken( str, "(" );

	Span2D< ParsedControlPoint > control_points( patch->control_points, patch->w, patch->h );

	for( int x = 0; x < patch->w; x++ ) {
		str = SkipToken( str, "(" );
		for( int y = 0; y < patch->h; y++ ) {
			str = SkipToken( str, "(" );

			ParsedControlPoint cp;

			for( int i = 0; i < 3; i++ ) {
				str = ParseFloat( &cp.position[ i ], str );
			}

			for( int i = 0; i < 2; i++ ) {
				str = ParseFloat( &cp.uv[ i ], str );
			}

			control_points( x, y ) = cp;

			str = SkipToken( str, ")" );
		}
		str = SkipToken( str, ")" );
	}

	str = SkipToken( str, ")" );

	str = SkipToken( str, "}" );
	str = SkipToken( str, "}" );

	return str;
}

static Span< const char > ParseQuoted( Span< const char > * quoted, Span< const char > str ) {
	str = SkipToken( str, "\"" );
	str = PEGCapture( quoted, str, []( Span< const char > str ) {
		return PEGNOrMore( str, 0, []( Span< const char > str ) {
			return PEGOr( str,
				[]( Span< const char > str ) { return PEGNotSet( str, "\\\"" ); },
				[]( Span< const char > str ) { return SkipLiteral( str, "\\\"" ); },
				[]( Span< const char > str ) { return SkipLiteral( str, "\\\\" ); }
			);
		} );
	} );
	str = SkipLiteral( str, "\"" );
	return str;
}

static Span< const char > ParseKeyValue( ParsedKeyValue * kv, Span< const char > str ) {
	str = ParseQuoted( &kv->key, str );
	str = ParseQuoted( &kv->value, str );
	return str;
}

static Span< const char > ParseEntity( ParsedEntity * entity, Span< const char > str ) {
	TracyZoneScoped;

	str = SkipToken( str, "{" );

	str = ParseUpToN( &entity->kvs, str, ParseKeyValue );

	entity->brushes.init( sys_allocator );
	entity->patches.init( sys_allocator );

	while( true ) {
		ParsedBrush brush;
		ParsedPatch patch;
		bool is_patch = false;

		Span< const char > res = PEGOr( str,
			[&]( Span< const char > str ) { return ParseQ1Brush( &brush, str ); },
			[&]( Span< const char > str ) { return ParseQ3Brush( &brush, str ); },
			[&]( Span< const char > str ) {
				is_patch = true;
				return ParsePatch( &patch, str );
			}
		);

		if( res.ptr == NULL )
			break;

		str = res;

		if( is_patch ) {
			entity->patches.add( patch );
		}
		else {
			entity->brushes.add( brush );
		}
	}

	str = SkipToken( str, "}" );

	if( str.ptr == NULL ) {
		entity->brushes.shutdown();
		entity->patches.shutdown();
	}

	return str;
}

static Span< const char > ParseComment( Span< const char > * comment, Span< const char > str ) {
	return PEGCapture( comment, str, []( Span< const char > str ) {
		str = SkipLiteral( str, "//" );
		str = PEGNOrMore( str, 0, []( Span< const char > str ) {
			return PEGNotSet( str, "\n" );
		} );
		return str;
	} );
}

static void StripComments( Span< char > map ) {
	TracyZoneScoped;

	for( size_t i = 0; i < map.n; i++ ) {
		Span< const char > comment;
		Span< const char > res = ParseComment( &comment, map + i );
		if( res.ptr == NULL )
			continue;

		memset( const_cast< char * >( comment.ptr ), ' ', comment.n );
		i += comment.n;
	}
}

std::vector< ParsedEntity > ParseEntities( Span< char > map ) {
	TracyZoneScoped;

	std::vector< ParsedEntity > entities;

	StripComments( map );

	Span< const char > cursor = map;
	cursor = CaptureNOrMore( &entities, cursor, 1, ParseEntity );
	cursor = SkipWhitespace( map );

	bool ok = cursor.ptr != NULL && cursor.n == 0;
	if( !ok ) {
		Fatal( "Can't parse the map" );
	}

	return entities;
}

