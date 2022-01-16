#pragma once

namespace physx {
class PxPhysics;
class PxMaterial;
class PxRigidStatic;
}

extern physx::PxPhysics * physx_physics;
extern physx::PxMaterial * physx_default_material;

void InitPhysX();
void ShutdownPhysX();
