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

#define MAX_ENT_CLUSTERS    16

struct edict_t;
struct gclient_t;

struct client_shared_t {
	int ping;
	int health;
	int frags;
};

struct entity_shared_t {
	gclient_t *client;
	bool inuse;

	int num_clusters;           // if -1, use headnode instead
	int clusternums[MAX_ENT_CLUSTERS];
	int headnode;               // unused if num_clusters != -1
	int areanum, areanum2;

	//================================

	Vec3 mins, maxs;
	Vec3 absmin, absmax, size;
	solid_t solid;
	int clipmask;
	edict_t *owner;
};
