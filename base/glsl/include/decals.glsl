struct DecalTile {
	uint indices[ FORWARD_PLUS_TILE_CAPACITY ];
};

layout( std430 ) readonly buffer b_DecalTiles {
	DecalTile decal_tiles[];
};

struct Decal {
	vec3 origin_orientation_xyz;
	float radius_orientation_w;
	vec4 color_uvwh_height;
};

layout( std430 ) readonly buffer b_Decals {
	Decal decals[];
};

uniform sampler2DArray u_DecalAtlases;

float ProjectedScale( vec3 p, vec3 o, vec3 d ) {
	return dot( p - o, d ) / dot( d, d );
}

vec2 DecalUV( vec4 uvwh, vec3 pos, vec3 bottom_left, vec3 basis_u, vec3 basis_v ) {
	vec2 uv = vec2( ProjectedScale( pos, bottom_left, basis_u ), ProjectedScale( pos, bottom_left, basis_v ) );
	uv = uvwh.xy + uvwh.zw * uv;
	return uv;
}

void ApplyDecals( uint count, int tile_index, inout vec4 diffuse, inout vec3 surface_normal ) {
	float accumulated_alpha = 1.0;
	vec3 accumulated_color = vec3( 0.0 );
	float accumulated_height = 0.0;

	vec3 dPos_dx = dFdx( v_Position );
	vec3 dPos_dy = dFdy( v_Position );

	for( uint i = 0; i < count; i++ ) {
		if( accumulated_alpha < 0.001 ) {
			accumulated_alpha = 0.0;
			break;
		}

		uint idx = count - i - 1;
		Decal decal = decals[ decal_tiles[ tile_index ].indices[ idx ] ];

		vec3 origin = floor( decal.origin_orientation_xyz );
		float radius = floor( decal.radius_orientation_w );
		Quaternion orientation = {
			vec4(
				( fract( decal.origin_orientation_xyz ) - 0.5 ) / 0.49,
				( fract( decal.radius_orientation_w ) - 0.5 ) / 0.49
			)
		};
		vec4 decal_color = vec4( fract( floor( decal.color_uvwh_height.yzw ) / 256.0 ), 1.0 );
		float decal_height = floor( decal.color_uvwh_height.y / 256.0 );
		vec4 uvwh = vec4( decal.color_uvwh_height.x, fract( decal.color_uvwh_height.yzw ) );
		float layer = floor( decal.color_uvwh_height.x );

		if( distance( origin, v_Position ) < radius ) {
			vec3 decal_normal, tangent, bitangent;
			QuaternionToBasis( orientation, decal_normal, tangent, bitangent );

			tangent *= radius * 2.0;
			bitangent *= radius * -2.0; // negate because uvs are y-down

			vec3 bottom_left = origin - ( tangent + bitangent ) * 0.5;

			// manually compute UV derivatives because auto derivatives are undefined inside incoherent branches
			vec2 uv = DecalUV( uvwh, v_Position, bottom_left, tangent, bitangent );
			vec2 dUV_dx = DecalUV( uvwh, v_Position + dPos_dx, bottom_left, tangent, bitangent ) - uv;
			vec2 dUV_dy = DecalUV( uvwh, v_Position + dPos_dy, bottom_left, tangent, bitangent ) - uv;

			float alpha = textureGrad( u_DecalAtlases, vec3( uv, layer ), dUV_dx, dUV_dy ).a;
			float inv_cos_45_degrees = 1.41421356237;
			float decal_alpha = min( 1.0, alpha * decal_color.a * max( 0.0, dot( decal_normal, surface_normal ) * inv_cos_45_degrees ) );
			accumulated_color += decal_color.rgb * decal_alpha * accumulated_alpha;
			accumulated_alpha *= 1.0 - decal_alpha;
			accumulated_height += decal_height * decal_alpha;
		}
	}

	vec3 decal_normal = vec3( dFdx( accumulated_alpha ), dFdy( accumulated_alpha ), 0.0 ) * accumulated_height;
	decal_normal = mat3( AffineToMat4( u_InverseV ) ) * decal_normal;
	surface_normal = normalize( surface_normal + decal_normal );

	diffuse.rgb = diffuse.rgb * accumulated_alpha + accumulated_color;
}
