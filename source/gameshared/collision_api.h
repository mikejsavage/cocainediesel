#pragma once

enum SolidBits : u8 {
	Solid_NotSolid = 0,
	Solid_Player = ( 1 << 1 ),
	Solid_Weapon = ( 1 << 2 ),
	Solid_Wallbangable = ( 1 << 3 ),
	Solid_Ladder = ( 1 << 4 ),
	Solid_Trigger = ( 1 << 5 ),
};

struct trace_t {
	float fraction;             // time completed, 1.0 = didn't hit anything
	Vec3 endpos;              // final position
	Vec3 contact;
	Vec3 normal;             // surface normal at impact
	int surfFlags;              // surface hit
	int contents;               // contents on other side of surface hit
	int ent;                    // not set by CM_*() functions
};

trace_t TraceVsEnt( const CollisionModelStorage * storage, const Ray & ray, const Shape & shape, const SyncEntityState * ent );

CollisionModel EntityCollisionModel( const SyncEntityState * ent );
MinMax3 EntityBounds( const CollisionModelStorage * storage, const SyncEntityState * ent );

