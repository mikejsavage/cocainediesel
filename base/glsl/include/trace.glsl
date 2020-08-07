uniform isamplerBuffer u_NodeBuffer;
uniform isamplerBuffer u_LeafBuffer;
uniform isamplerBuffer u_BrushBuffer;
uniform samplerBuffer u_PlaneBuffer;

struct Node
{
	vec4 plane;
	int children[2];
};

struct Leaf
{
	int firstBrush;
	int numBrushes;
};

struct Brush
{
	int firstSide;
	int numSides;
};

vec4 GetPlane( int idx )
{
	return texelFetch( u_PlaneBuffer, idx );
}

Node GetNode( int idx )
{
	Node node;
	ivec3 data = texelFetch( u_NodeBuffer, idx ).xyz;
	node.plane = GetPlane( data.x );
	node.children[ 0 ] = data.y;
	node.children[ 1 ] = data.z;
	return node;
}

Leaf GetLeaf( int idx )
{
	Leaf leaf;
	ivec2 data = texelFetch( u_LeafBuffer, idx ).xy;
	leaf.firstBrush = data.x;
	leaf.numBrushes = data.y;
	return leaf;
}

Brush GetBrush( int idx )
{
	Brush brush;
	ivec2 data = texelFetch( u_BrushBuffer, idx ).xy;
	brush.firstSide = data.x;
	brush.numSides = data.y;
	return brush;
}

vec4 ClipBrush( vec3 p1, vec3 dir, float tmin, float tmax, Brush brush, float radius ) {
	vec4 near = vec4( tmin );

	for ( int i = 0; i < brush.numSides; i++ ) {
		vec4 plane = GetPlane( brush.firstSide + i );
		plane.w += radius;

		float dist = plane.w - dot( p1, plane.xyz );
		float denom = dot( dir, plane.xyz );
		float t = dist / denom;
		if ( denom < 0.0 ) { // entering
			if ( t >= tmax )
				return vec4( 1.0 ); // missed
			if ( t >= near.w ) // new best
				near = vec4( plane.xyz, t );
		}
		else { // leaving
			if ( t <= near.w )
				return vec4( 1.0 ); // missed
			if ( t <= tmax ) // new best
				tmax = t;
		}
	}

	return near;
}

vec4 ClipLeaf( vec3 p1, vec3 dir, float tmin, float tmax, Leaf leaf, float radius )
{
	vec4 near = vec4( 1.0 );

	for ( int i = 0; i < leaf.numBrushes; i++ ) {
		Brush brush = GetBrush( leaf.firstBrush + i );
		if( brush.numSides > 0 ) {
			vec4 frac = ClipBrush( p1, dir, tmin, tmax, brush, radius );
			if ( frac.w <= near.w )
				near = frac;
		}
	}
	return near;
}

#define STACKSIZE 25
vec4 TracePoint( vec3 p1, vec3 dir )
{
	int nodeStack[ STACKSIZE ];
	float timeStack[ STACKSIZE ];
	int stackIdx = 0;

	float tmin = 0.0;
	float tmax = 1.0;
	int nodeIdx = 0;

	do {
		if ( nodeIdx >= 0 ) {
			Node node = GetNode( nodeIdx );

			vec4 plane = node.plane;

			float denom = dot( plane.xyz, dir );
			float dist = plane.w - dot( plane.xyz, p1 );

			int near = int( step( 0.0, dist ) ); // check sign of dist to decide side of plane

			float t = dist / denom;
			if ( 0.0 <= t && t <= tmax ) {
				if ( t >= tmin ) { // we have to check the other side of plane later
					nodeStack[ stackIdx ] = node.children[ 1 - near ]; // far
					timeStack[ stackIdx ] = tmax; // store endpoint of segment
					stackIdx++;
					tmax = t; // set new endpoint of segment
				} else { // the intersection is outside of the segment
					near = 1 - near; // far
				}
			}
			nodeIdx = node.children[ near ];
		} else {
			// in a leaf node
			Leaf leaf = GetLeaf( - ( nodeIdx + 1 ) );
			vec4 frac = ClipLeaf( p1, dir, tmin, tmax, leaf, 0.0f );
			if ( frac.w < 1.0 )
				return frac;

			tmin = tmax; // set new startpoint of segment
			stackIdx--;
			nodeIdx = nodeStack[ stackIdx ];
			tmax = timeStack[ stackIdx ]; // load endpoint of segment
		}
	} while ( stackIdx >= 0 && stackIdx <= STACKSIZE );

	return vec4( 0.0 );
}

struct State {
	int node;
	float tmin;
	float tmax;
};

vec4 TraceSphere( vec3 p1, vec3 dir, float radius )
{
	State stack[ STACKSIZE ];
	int stackIdx = 0;

	State state = State( 0, 0.0, 1.0 );
	vec4 best = vec4( 1.0 );

	do {
		if ( state.node >= 0 ) {
			// in a normal node
			Node node = GetNode( state.node );
			vec4 plane = node.plane;

			float denom = dot( plane.xyz, dir );
			float dist = plane.w - dot( plane.xyz, p1 );
			float dist_add_offset = dist + sign( denom ) * radius;
			float dist_sub_offset = dist - sign( denom ) * radius;

			float t_add_offset = dist_add_offset / denom;
			float t_sub_offset = dist_sub_offset / denom;

			int near = int( step( 0.0, dist_add_offset ) );
			if ( t_add_offset > 0.0 ) {
				if ( state.tmin <= t_add_offset ) {
					if ( state.tmax >= t_sub_offset ) {
						stack[ stackIdx++ ] = State( node.children[ 1 - near ], t_sub_offset, state.tmax );
					}
					state = State( node.children[ near ], state.tmin, min( t_add_offset, state.tmax ) );
				} else {
					state = State( node.children[ 1 - near ], state.tmin, state.tmax );
				}
			} else {
				state = State( node.children[ near ], state.tmin, state.tmax );
			}
		} else {
			// in a leaf node
			if ( state.tmin < best.w ) {
				Leaf leaf = GetLeaf( - ( state.node + 1 ) );
				vec4 frac = ClipLeaf( p1, dir, 0.0, 1.0, leaf, radius );
				if ( frac.w < best.w )
					best = frac;
			}

			state = stack[ --stackIdx ];
			state.tmax = min( state.tmax, best.w );
		}
	} while ( stackIdx >= 0 && stackIdx <= STACKSIZE );

	return best;
}

vec4 Trace( vec3 p1, vec3 dir, float radius )
{
	if ( radius == 0.0 ) {
		return TracePoint( p1, dir );
	}
	return TraceSphere( p1, dir, radius );
}