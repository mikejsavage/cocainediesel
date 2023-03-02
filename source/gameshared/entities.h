#pragma once

#include "qcommon/types.h"

enum Team : u8 {
	Team_Count = 4,
	Team_Ally,
	Team_Enemy,
};

struct ModelEntity {
	StringHash model;
};

struct DecalEntity {
	StringHash decal;
};

struct PlayerEntity {
	StringHash hat;

	bool using_gadget;
	WeaponType weapon;
	GadgetType gadget;

	Team team;

	Optional< Vec3 > laser_target;
};

struct SpectatorEntity {
	EntityID spectating;
};

struct ProjectileEntity {
	StringHash model;
	StringHash sound;
	StringHash vfx;
	Team team;
};

struct LinearProjectileEntity {
	// TODO: linear movement stuff

	StringHash model;
	StringHash sound;
	StringHash vfx;
	Team team;
};

struct BombSiteEntity {
	char letter;
};

struct PlantAreaEntity {
	StringHash model;
	EntityID bomb_site;
};

struct TrapEntity {
	StringHash model;
	MinMax3 bounds;
	Optional< Time > triggered_time;
	EntityID group_next;
};

struct Entity {
	EntityType type;
	EntityID id;

	Vec3 position;
	EulerAngles3 angles;
	Vec3 scale;
	Vec3 velocity;

	bool teleported;

	union {
		GenericEntity generic;
		PlayerEntity player;
	};
};
