#pragma once

#include "qcommon/types.h"
#include "gameshared/q_collision.h"

enum ShapeType {
	ShapeType_Ray,
	ShapeType_AABB,
	ShapeType_Sphere,
	ShapeType_Capsule,
};

struct Shape {
	ShapeType type;

	union {
		CenterExtents3 aabb;
		Sphere sphere;
		Capsule capsule;
	};
};

struct Intersection {
	float t;
	Vec3 normal;
	SolidBits solidity;
};

struct Ray {
	Vec3 origin;
	Vec3 direction; // normalized
	Vec3 inv_dir;
	float length;
};

Ray MakeRayOriginDirection( Vec3 origin, Vec3 direction, float length );
Ray MakeRayStartEnd( Vec3 start, Vec3 end );

MinMax3 MinkowskiSum( const MinMax3 & bounds, const Shape & shape );
float Support( const Shape & shape, Vec3 dir );

bool RayVsAABB( const Ray & ray, const MinMax3 & aabb, Intersection * enter, Intersection * leave );
bool RayVsSphere( const Ray & ray, const Sphere & sphere, float * t );
bool RayVsCapsule( const Ray & ray, const Capsule & capsule, float * t );

// TODO: special case stationary traces
struct MapData;
struct MapModel;
bool SweptShapeVsMapModel( const MapData * map, const MapModel * model, Ray ray, const Shape & shape, SolidBits solid_mask, Intersection * intersection );
bool SweptAABBVsAABB( const MinMax3 & a, Vec3 va, const MinMax3 & b, Vec3 vb, Intersection * intersection );

struct GLTFCollisionData;
bool SweptShapeVsGLTF( const GLTFCollisionData * gltf, const Mat3x4 & transform, Ray ray, const Shape & shape, SolidBits solid_mask, Intersection * intersection );
