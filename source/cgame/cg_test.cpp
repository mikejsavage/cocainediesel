/*
Copyright (C) 2002-2003 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef PUBLIC_BUILD

#include "cg_local.h"

void CG_DrawTestLine( const vec3_t start, const vec3_t end ) {
	CG_QuickPolyBeam( start, end, 6, cgs.media.shaderLaser );
}

void CG_DrawTestBox( const vec3_t origin, const vec3_t mins, const vec3_t maxs, const vec3_t angles ) {
	vec3_t start, end, vec;
	float linewidth = 6;
	mat3_t localAxis;
	mat3_t ax;
	AnglesToAxis( angles, ax );
	Matrix3_Transpose( ax, localAxis );

	//horizontal projection
	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = mins[0];
	end[1] = mins[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = mins[2];

	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = maxs[0];
	start[1] = maxs[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	//x projection
	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = mins[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = maxs[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = mins[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = maxs[2];

	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	//z projection
	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = mins[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = maxs[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = mins[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = maxs[2];

	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );
}

#endif // _DEBUG
