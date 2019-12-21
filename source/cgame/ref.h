/*
Copyright (C) 1997-2001 Id Software, Inc.

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
#pragma once

#include "gameshared/q_math.h"

// refdef flags
#define RDF_UNDERWATER          0x1     // warp the screen as apropriate
#define RDF_CROSSINGWATER       0x2     // potentially crossing water surface
#define RDF_BLURRED             0x4

typedef struct orientation_s {
	mat3_t axis;
	vec3_t origin;
} orientation_t;

struct TRS {
	Quaternion rotation;
	Vec3 translation;
	float scale;
};

struct MatrixPalettes {
	Span< Mat4 > joint_poses;
	Span< Mat4 > skinning_matrices;
};

typedef struct entity_s {
	const Model * model;

	/*
	** most recent data
	*/
	mat3_t axis;
	vec3_t origin, origin2;

	/*
	** texturing
	*/
	const Material * override_material; // NULL for inline skin

	/*
	** misc
	*/
	int64_t shaderTime;
	RGBA8 color;

	float scale;
	float radius;                       // used as RT_SPRITE's radius
	float rotation;

	float outlineHeight;
	RGBA8 outlineColor;
} entity_t;

typedef struct refdef_s {
	int x, y, width, height;            // viewport, in virtual screen coordinates
	int scissor_x, scissor_y, scissor_width, scissor_height;
	int ortho_x, ortho_y;
	float fov_x, fov_y;
	vec3_t vieworg;
	mat3_t viewaxis;
	int64_t time;                       // time is used for timing offsets
	int rdflags;                        // RDF_UNDERWATER, etc
	uint8_t *areabits;                  // if not NULL, only areas with set bits will be drawn
	float minLight;                     // minimum value of ambient lighting applied to RF_MINLIGHT entities
} refdef_t;
