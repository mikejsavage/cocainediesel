layout( std140 ) uniform u_Decal {
	int u_NumDecals;
};

uniform samplerBuffer u_DecalData;
uniform isamplerBuffer u_DecalCount;
uniform sampler2DArray u_DecalAtlases;

float ProjectedScale( vec3 p, vec3 o, vec3 d ) {
	return dot( p - o, d ) / dot( d, d );
}

// must match the CPU OrthonormalBasis
void OrthonormalBasis( vec3 v, out vec3 tangent, out vec3 bitangent ) {
	float s = step( 0.0, v.z ) * 2.0 - 1.0;
	float a = -1.0 / ( s + v.z );
	float b = v.x * v.y * a;

	tangent = vec3( 1.0 + s * v.x * v.x * a, s * b, -s * v.x );
	bitangent = vec3( b, s + v.y * v.y * a, -v.y );
}

void applyDecals( inout vec4 diffuse, inout vec3 normal ) {
  	float tile_size = float( TILE_SIZE );
	int tile_row = int( ( u_ViewportSize.y - gl_FragCoord.y ) / tile_size );
	int tile_col = int( gl_FragCoord.x / tile_size );
	int cols = int( u_ViewportSize.x + tile_size - 1 ) / int( tile_size );
	int tile_index = tile_row * cols + tile_col;

	int count = texelFetch( u_DecalCount, tile_index ).x;

	float accumulated_alpha = 1.0;
	vec3 accumulated_color = vec3( 0.0 );

	for( int i = 0; i < count; i++ ) {
		if( accumulated_alpha < 0.001 ) {
			accumulated_alpha = 0.0;
			break;
		}

		int idx = tile_index * 100 + i * 2; // NOTE(msc): 100 = 2 * MAX_DECALS_PER_TILE

		vec4 data1 = texelFetch( u_DecalData, idx + 0 );
		vec3 origin = floor( data1.xyz );
		float radius = floor( data1.w );
		vec3 decal_normal = ( fract( data1.xyz ) - 0.5 ) / 0.49;
		float angle = fract( data1.w ) * M_PI * 2.0;

		vec4 data2 = texelFetch( u_DecalData, idx + 1 );
		vec4 decal_color = vec4( floor( data2.yzw ) / 255.0, 1.0 );
		vec4 uvwh = vec4( data2.x, fract( data2.yzw ) );
		float layer = floor( uvwh.x );

		if( distance( origin, v_Position ) < radius ) {
			vec3 basis_u;
			vec3 basis_v;
			OrthonormalBasis( decal_normal, basis_u, basis_v );
			basis_u *= radius * 2.0;
			basis_v *= radius * -2.0;
			vec3 bottom_left = origin - ( basis_u + basis_v ) * 0.5;

			float c = cos( angle );
			float s = sin( angle );
			mat2 rotation = mat2( c, s, -s, c );
			vec2 uv = vec2( ProjectedScale( v_Position, bottom_left, basis_u ), ProjectedScale( v_Position, bottom_left, basis_v ) );

			uv -= 0.5;
			uv = rotation * uv;
			uv += 0.5;
			uv = uvwh.xy + uvwh.zw * uv;

			float alpha = texture( u_DecalAtlases, vec3( uv, layer ) ).r;
			float inv_cos_45_degrees = 1.41421356237;
			float decal_alpha = min( 1.0, alpha * decal_color.a * max( 0.0, dot( normal, decal_normal ) * inv_cos_45_degrees ) );
			accumulated_color += decal_color.rgb * decal_alpha * accumulated_alpha;
			accumulated_alpha *= 1.0 - decal_alpha;
		}
	}

	diffuse.rgb = diffuse.rgb * accumulated_alpha + accumulated_color;
}