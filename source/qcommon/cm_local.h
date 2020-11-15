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

#include "qcommon/cmodel.h"
#include "qcommon/hashmap.h"

#define CM_SUBDIV_LEVEL 16

cmodel_t * CM_NewCModel( CModelServerOrClient soc, u64 hash );

void    CM_InitBoxHull( CollisionModel *cms );
void    CM_InitOctagonHull( CollisionModel *cms );

void    CM_FloodAreaConnections( CollisionModel *cms );

void CM_LoadQ3BrushModel( CModelServerOrClient soc, CollisionModel * cms, Span< const u8 > data );
