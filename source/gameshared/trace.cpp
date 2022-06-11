#include "gameshared/trace.h"
#include "gameshared/q_shared.h"

static constexpr float EPSILON = 0.01f;

static bool IntersectPlane( Plane plane, Ray ray, float & t_hit ) {
  float dist = plane.distance - Dot( ray.origin, plane.normal );
  float denom = Dot( ray.direction, plane.normal );
  if( Abs( denom ) < EPSILON ) {
    if( dist == 0.0f ) {
      t_hit = 0.0f;
      return true;
    }
    return false;
  }

  float t = dist / denom;
  if( t < 0.0f || t > ray.t_max ) {
    return false;
  }

  t_hit = t;
  return true;
}

static bool IntersectMinMax3( MinMax3 bounds, Ray ray, Intersection & min, Intersection & max ) {
  constexpr Vec3 aabb_normals[] = {
    Vec3( 1.0f, 0.0f, 0.0f ),
    Vec3( 0.0f, 1.0f, 0.0f ),
    Vec3( 0.0f, 0.0f, 1.0f ),
  };

  Intersection enter = { 0.0f };
  Intersection leave = { ray.t_max };
  Vec3 inverse_direction = 1.0f / ray.direction;
  for( u32 i = 0; i < 3; i++ ) {
    if( Abs( ray.direction[ i ] ) < FLT_EPSILON ) {
      if( ray.origin[ i ] < bounds.mins[ i ] || ray.origin[ i ] > bounds.maxs[ i ] ) {
        return false;
      }
    }
    else {
      float t_near = ( bounds.mins[ i ] - ray.origin[ i ] ) * inverse_direction[ i ];
      float t_far = ( bounds.maxs[ i ] - ray.origin[ i ] ) * inverse_direction[ i ];
      float s = 1.0f;
      if( t_near > t_far ) {
        Swap2( &t_near, &t_far );
        s = -1.0f;
      }
      if( t_near > enter.t ) {
        enter.t = t_near;
        enter.normal = -s * aabb_normals[ i ];
      }
      if( t_far < leave.t ) {
        leave.t = t_far;
        leave.normal = s * aabb_normals[ i ];
      }
      if( enter.t > leave.t ) {
        return false;
      }
    }
  }
  min = enter;
  max = leave;
  return true;
}

static bool IntersectBrush( const MapData * map, const MapBrush * brush, Ray ray, Intersection & min, Intersection & max ) {
  Intersection enter = { 0.0f };
  Intersection leave = { ray.t_max };

  if( !IntersectMinMax3( brush->bounds, ray, enter, leave ) )
    return false;
  
  for( u32 i = 0; i < brush->num_planes; i++ ) {
    // u32 index = map->brush_plane_indices[ brush->first_plane + i ];
    u32 index = brush->first_plane + i;
    const Plane * plane = &map->brush_planes[ index ];

    float dist = plane->distance - Dot( plane->normal, ray.origin );
    float denom = Dot( plane->normal, ray.direction );
    float t = dist / denom;

    if( denom < 0.0f ) {
      if( t > leave.t ) {
        return false;
      }
      if( t >= enter.t ) {
        enter.t = t;
        enter.normal = plane->normal;
      }
    }
    else {
      if( t < enter.t ) {
        return false;
      }
      if( t <= leave.t ) {
        leave.t = t;
        leave.normal = plane->normal;
      }
    }
  }
  min = enter;
  max = leave;
  return true;
}

static bool IntersectLeaf( const MapData * map, const MapKDTreeNode * leaf, Ray ray, Intersection & min, Intersection & max ) {
  Intersection enter = { ray.t_max };
  Intersection leave = { 0.0f };

  // float t_best = t_max;
  bool hit = false;
  for( u32 i = 0; i < leaf->leaf.num_brushes; i++ ) {
    u32 index = map->brush_indices[ leaf->leaf.first_brush + i ];
    Intersection brush_enter = { 0.0f };
    Intersection brush_leave = { ray.t_max };
    if( IntersectBrush( map, &map->brushes[ index ], ray, brush_enter, brush_leave ) ) {
      if( brush_enter.t < enter.t ) {
        enter = brush_enter;
        leave = brush_leave;
      }
      hit = true;
    }
  }
  if( hit ) {
    min = enter;
    max = leave;
  }
  return hit;
}

constexpr size_t KDTREE_STACKSIZE = 64;

struct KDTreeTodo {
  const MapKDTreeNode * node;
  float t_min;
  float t_max;
};

bool Trace( const MapData * map, const MapModel * model, Ray ray, Intersection & enter, Intersection & leave ) {
  if( !IntersectMinMax3( model->bounds, ray, enter, leave ) ) {
    return false;
  }
  float t_min = enter.t;
  float t_max = leave.t;

  KDTreeTodo todo[ KDTREE_STACKSIZE ];
  u32 num_todo = 0;

  bool hit = false;
  float t_best = t_max;
  const MapKDTreeNode * node = &map->nodes[ model->root_node ];
  while( node != NULL ) {
    if( ray.t_max < t_min )
      break;

    if( node->leaf.is_leaf != 3 ) {
      // process node
      u32 axis = node->node.is_leaf_and_splitting_plane_axis;
      float t_plane = ( node->node.splitting_plane_distance - ray.origin[ axis ] ) / ray.direction[ axis ];

      const MapKDTreeNode * first_child, * second_child;
      bool below_first =
        ( ray.origin[ axis ] < node->node.splitting_plane_distance ) ||
        ( ray.origin[ axis ] == node->node.splitting_plane_distance && ray.direction[ axis ] <= 0.0f );
      if( below_first ) {
        first_child = node + 1;
        second_child = &map->nodes[ node->node.front_child ];
      }
      else {
        first_child = &map->nodes[ node->node.front_child ];
        second_child = node + 1;
      }

      if( t_plane > t_max || t_plane <= 0.0f ) {
        node = first_child;
      }
      else if( t_plane < t_min ) {
        node = second_child;
      }
      else {
        todo[ num_todo ] = { second_child, t_plane, t_max };
        num_todo++;
        node = first_child;
        t_max = t_plane;
      }
    }
    else {
      // process leaf
      Intersection leaf_enter, leaf_leave;
      if( IntersectLeaf( map, node, ray, leaf_enter, leaf_leave ) ) {
        if( leaf_enter.t < enter.t ) {
          enter = leaf_enter;
          leave = leaf_leave;
        }
        hit = true;
      }

      if( num_todo > 0 ) {
        num_todo--;
        node = todo[ num_todo ].node;
        t_min = todo[ num_todo ].t_min;
        t_max = todo[ num_todo ].t_max;
      }
      else {
        break;
      }
    }
  }
  return hit;
}
