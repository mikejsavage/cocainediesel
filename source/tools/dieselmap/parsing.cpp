#pragma once

#include <vector>

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "qcommon/span2d.h"

#include "parsing.h"

#include <string.h>

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
			entity->patches.push_back( patch );
		}
		else {
			entity->brushes.push_back( brush );
		}
	}

	str = SkipToken( str, "}" );

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
	cursor = SkipWhitespace( cursor );

	bool ok = cursor.ptr != NULL && cursor.n == 0;
	if( !ok ) {
		Fatal( "Can't parse the map" );
	}

	return entities;
}

