#pragma once

#include "qcommon/types.h"
#include "gameshared/cdmap.h"

struct Intersection {
	float t;
	Vec3 normal;
};

struct Ray {
	Vec3 origin;
	Vec3 direction; // normalized
	Vec3 inv_dir;
	float length;
};

bool Trace( const MapData * map, const MapModel * model, Ray ray, Intersection * intersection );
