#include "include/uniforms.glsl"

layout( local_size_x = 64 ) in;

layout( std140 ) uniform u_TileCulling {
  uint rows;
  uint cols;
  uint num_decals;
  uint num_dlights;
};

struct Decal {
	vec3 origin_normal;
	float radius_angle;
	vec4 color_uvwh_height;
};

layout( std430 ) readonly buffer b_Decals {
	Decal decals[];
};

struct DynamicLight {
	vec3 origin_color;
	float radius;
};

layout( std430 ) readonly buffer b_Dlights {
	DynamicLight dlights[];
};

const uint max_per_tile = 50;

struct Tile {
  uint indices[ max_per_tile ];
};

layout( std430 ) writeonly buffer b_DecalTiles {
	Tile decal_tiles[];
};

layout( std430 ) writeonly buffer b_DLightTiles {
	Tile dlight_tiles[];
};

struct Count {
  uint num_decals;
  uint num_dlights;
};

layout( std430 ) writeonly buffer b_TileCounts {
	Count counts[];
};

bool SphereCone( vec3 sphere_origin, float sphere_radius, vec3 cone_origin, vec3 cone_axis, float cone_tan ) {
  vec3 cone_to_sphere = sphere_origin - cone_origin;
  float t = dot( cone_to_sphere, cone_axis );
  vec3 cone_slant = normalize( cone_axis + normalize( cone_to_sphere - t * cone_axis ) * cone_tan );
  t = dot( cone_to_sphere, cone_slant );
  t = max( t, 0.0 );
  vec3 point_on_cone = cone_origin + t * cone_slant;

  return distance( point_on_cone, sphere_origin ) - sphere_radius < 0.0;
}

bool SphereInTile( vec3 origin, float radius, vec3 tile_direction, float cone_tan ) {
  return SphereCone( origin, radius, u_CameraPos, tile_direction, cone_tan );
}

void CullTile( uvec2 tile ) {
  vec2 tile_min = TILE_SIZE * ( tile.xy ) / u_ViewportSize.xy * 2.0 - 1.0;
  vec2 tile_max = TILE_SIZE * ( tile.xy + 1 ) / u_ViewportSize.xy * 2.0 - 1.0;
  // opengl y flip
  tile_min.y = -tile_min.y;
  tile_max.y = -tile_max.y;
  
  vec3 tile_vmin = ( ( u_InverseV * u_InverseP * vec4( tile_min, 1.0, 1.0 ) ).xyz );
  vec3 tile_vmax = ( ( u_InverseV * u_InverseP * vec4( tile_max, 1.0, 1.0 ) ).xyz );
  float cone_tan = distance( tile_vmin, tile_vmax ) * 0.5;
  vec3 tile_direction = normalize( ( tile_vmin + tile_vmax ) * 0.5 );

  uint col = tile.x;
  uint row = tile.y;
  uint tile_idx = row * rows + col;

  Tile decal_tile;
  uint decal_count = 0;
  for( int i = 0; i < num_decals; i++ ) {
    if( decal_count == max_per_tile )
      break;

    Decal decal = decals[ i ];
    vec3 origin = floor( decal.origin_normal );
    float radius = floor( decal.radius_angle );
    if( SphereInTile( origin, radius, tile_direction, cone_tan ) ) {
      decal_tile.indices[ decal_count ] = i;
      decal_count++;
    }
  }

  Tile dlight_tile;
  uint dlight_count = 0;
  for( int i = 0; i < num_dlights; i++ ) {
    if( dlight_count == max_per_tile )
      break;

    DynamicLight dlight = dlights[ i ];
		vec3 origin = floor( dlight.origin_color.xyz );
    float radius = dlight.radius;
    if( SphereInTile( origin, radius, tile_direction, cone_tan ) ) {
      dlight_tile.indices[ dlight_count ] = i;
      dlight_count++;
    }
  }
  
  decal_tiles[ tile_idx ] = decal_tile;
  counts[ tile_idx ].num_decals = decal_count;
  dlight_tiles[ tile_idx ] = dlight_tile;
  counts[ tile_idx ].num_dlights = dlight_count;
}

void main() {
  uint tile_idx = gl_GlobalInvocationID.x;
  if( tile_idx > cols * rows )
    return;
  uint col = tile_idx % rows;
  uint row = tile_idx / rows;
  CullTile( uvec2( col, row ) );
}
