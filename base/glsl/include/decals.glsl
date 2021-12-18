uniform isamplerBuffer u_DecalTiles;
uniform samplerBuffer u_DecalData;
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

void applyDecals( int count, int tile_index, inout vec4 diffuse, inout vec3 normal ) {
	float accumulated_alpha = 1.0;
	vec3 accumulated_color = vec3( 0.0 );
	float accumulated_height = 0.0;

	for( int i = 0; i < count; i++ ) {
		if( accumulated_alpha < 0.001 ) {
			accumulated_alpha = 0.0;
			break;
		}

		int idx = tile_index * 50 + i;
		int decal_index = texelFetch( u_DecalTiles, idx ).x * 2; // decal is 2 vec4's

		vec4 data1 = texelFetch( u_DecalData, decal_index );
		vec3 origin = floor( data1.xyz );
		float radius = floor( data1.w );
		vec3 decal_normal = ( fract( data1.xyz ) - 0.5 ) / 0.49;
		float angle = fract( data1.w ) * M_PI * 2.0;

		vec4 data2 = texelFetch( u_DecalData, decal_index + 1 );
		vec4 decal_color = vec4( fract( floor( data2.yzw ) / 256.0 ), 1.0 );
		float decal_height = floor( data2.y / 256.0 );
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
			accumulated_height += decal_height * decal_alpha;
		}
	}
	vec3 decal_normal = vec3( dFdx( accumulated_alpha ), dFdy( accumulated_alpha ), 0.0 ) * accumulated_height;
	decal_normal = mat3( u_InverseV ) * decal_normal;
	normal = normalize( normal + decal_normal );

	diffuse.rgb = diffuse.rgb * accumulated_alpha + accumulated_color;
}
