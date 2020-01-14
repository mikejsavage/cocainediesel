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

//
// qfiles.h: quake file formats
// This file must be identical in the quake and utils directories
//

#pragma once

/*
==============================================================================

.BSP file format

==============================================================================
*/

#define IDBSPHEADER     "IBSP"
#define RBSPHEADER      "RBSP"
#define QFBSPHEADER     "FBSP"
constexpr const uint8_t COMPRESSED_BSP_MAGIC[] = { 0x28, 0xb5, 0x2f, 0xfd };

#define Q3BSPVERSION        46
#define RTCWBSPVERSION      47
#define RBSPVERSION     1
#define QFBSPVERSION        1

#define MAX_MAP_LEAFS       0x20000

// lightmaps
#define MAX_LIGHTMAPS       4

#define LIGHTMAP_BYTES      3

// key / value pair sizes

#define MAX_KEY     32
#define MAX_VALUE   1024

//=============================================================================

typedef struct {
	int fileofs, filelen;
} lump_t;

#define LUMP_ENTITIES       0
#define LUMP_SHADERREFS     1
#define LUMP_PLANES     2
#define LUMP_NODES      3
#define LUMP_LEAFS      4
#define LUMP_LEAFFACES      5
#define LUMP_LEAFBRUSHES    6
#define LUMP_MODELS     7
#define LUMP_BRUSHES        8
#define LUMP_BRUSHSIDES     9
#define LUMP_VERTEXES       10
#define LUMP_ELEMENTS       11
#define LUMP_FOGS       12
#define LUMP_FACES      13
#define LUMP_LIGHTING       14
#define LUMP_LIGHTGRID      15
#define LUMP_VISIBILITY     16
#define LUMP_LIGHTARRAY     17

#define HEADER_LUMPS        18      // 16 for IDBSP

typedef struct {
	int ident;
	int version;
	lump_t lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
	float mins[3], maxs[3];
	int firstface, numfaces;        // submodels just draw faces
	                                // without walking the bsp tree
	int firstbrush, numbrushes;
} dmodel_t;

typedef struct {
	float point[3];
	float tex_st[2];            // texture coords
	float lm_st[2];             // lightmap texture coords
	float normal[3];            // normal
	unsigned char color[4];     // color used for vertex lighting
} dvertex_t;

typedef struct {
	float point[3];
	float tex_st[2];
	float lm_st[MAX_LIGHTMAPS][2];
	float normal[3];
	unsigned char color[MAX_LIGHTMAPS][4];
} rdvertex_t;

// planes (x&~1) and (x&~1)+1 are always opposites
typedef struct {
	float normal[3];
	float dist;
} dplane_t;


// contents flags are separate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

// these definitions also need to be in q_shared.h!

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_SOLID      1       // an eye is never valid in a solid
#define CONTENTS_LAVA       8
#define CONTENTS_SLIME      16
#define CONTENTS_WATER      32

#define CONTENTS_AREAPORTAL 0x8000

#define CONTENTS_PLAYERCLIP 0x10000
#define CONTENTS_MONSTERCLIP    0x20000

// bot specific contents types
#define CONTENTS_TELEPORTER 0x40000
#define CONTENTS_JUMPPAD    0x80000
#define CONTENTS_CLUSTERPORTAL  0x100000
#define CONTENTS_DONOTENTER 0x200000

#define CONTENTS_ORIGIN     0x1000000   // removed before bsping an entity

#define CONTENTS_BODY       0x2000000   // should never be on a brush, only in game
#define CONTENTS_CORPSE     0x4000000
#define CONTENTS_DETAIL     0x8000000   // brushes not used for the bsp
#define CONTENTS_STRUCTURAL 0x10000000  // brushes used for the bsp
#define CONTENTS_TRANSLUCENT    0x20000000  // don't consume surface fragments inside
#define CONTENTS_TRIGGER    0x40000000
#define CONTENTS_NODROP     0x80000000  // don't leave bodies or items (death fog, lava)

#define SURF_NODAMAGE     0x1         // never give falling damage
#define SURF_SLICK        0x2         // effects game physics
#define SURF_SKY          0x4         // lighting from environment map
#define SURF_LADDER       0x8
#define SURF_NOIMPACT     0x10        // don't make missile explosions
#define SURF_NOMARKS      0x20        // don't leave missile marks
#define SURF_FLESH        0x40        // make flesh sounds and effects
#define SURF_NODRAW       0x80        // don't generate a drawsurface at all
#define SURF_HINT         0x100       // make a primary bsp splitter
#define SURF_SKIP         0x200       // completely ignore, allowing non-closed brushes
#define SURF_NOLIGHTMAP   0x400       // surface doesn't need a lightmap
#define SURF_POINTLIGHT   0x800       // generate lighting info at vertexes
#define SURF_METALSTEPS   0x1000      // clanking footsteps
#define SURF_NOSTEPS      0x2000      // no footstep sounds
#define SURF_NONSOLID     0x4000      // don't collide against curves with this set
#define SURF_LIGHTFILTER  0x8000      // act as a light filter during q3map -light
#define SURF_ALPHASHADOW  0x10000     // do per-pixel light shadow casting in q3map
#define SURF_NODLIGHT     0x20000     // never add dynamic lights
#define SURF_DUST         0x40000     // leave a dust trail when walking on this surface
#define SURF_NOWALLJUMP   0x80000     // can not perform walljumps on this surface

#define SURF_FBSP_START   0x40000     // FBSP specific extensions to BSP


typedef struct {
	int planenum;
	int children[2];            // negative numbers are -(leafs+1), not nodes
	int mins[3];                // for frustum culling
	int maxs[3];
} dnode_t;


typedef struct shaderref_s {
	char name[64];
	int flags;
	int contents;
} dshaderref_t;

enum BSPFaceType {
	FaceType_Bad,
	FaceType_Planar,
	FaceType_Patch,
	FaceType_Mesh,
	FaceType_Flare,
	FaceType_Foliage,
};

typedef struct {
	int shadernum;
	int fognum;
	int facetype;

	int firstvert;
	int numverts;
	unsigned firstelem;
	int numelems;

	int lm_texnum;              // lightmap info
	int lm_offset[2];
	int lm_size[2];

	float origin[3];            // FaceType_Flare only

	float mins[3];
	float maxs[3];              // FaceType_Patch and FaceType_Mesh only
	float normal[3];            // FaceType_Planar only

	int patch_cp[2];            // patch control point dimensions
} dface_t;

typedef struct {
	int shadernum;
	int fognum;
	int facetype;

	int firstvert;
	int numverts;
	unsigned firstelem;
	int numelems;

	unsigned char lightmapStyles[MAX_LIGHTMAPS];
	unsigned char vertexStyles[MAX_LIGHTMAPS];

	int lm_texnum[MAX_LIGHTMAPS];               // lightmap info
	int lm_offset[MAX_LIGHTMAPS][2];
	int lm_size[2];

	float origin[3];            // FaceType_Flare only

	float mins[3];
	float maxs[3];              // FaceType_Patch and FaceType_Mesh only
	float normal[3];            // FaceType_Planar only

	int patch_cp[2];            // patch control point dimensions
} rdface_t;

typedef struct {
	int cluster;
	int area;

	int mins[3];
	int maxs[3];

	int firstleafface;
	int numleaffaces;

	int firstleafbrush;
	int numleafbrushes;
} dleaf_t;

typedef struct {
	int planenum;
	int shadernum;
} dbrushside_t;

typedef struct {
	int planenum;
	int shadernum;
	int surfacenum;
} rdbrushside_t;

typedef struct {
	int firstside;
	int numsides;
	int shadernum;
} dbrush_t;

typedef struct {
	int numclusters;
	int rowsize;
	unsigned char data[1];
} dvis_t;
