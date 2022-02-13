#pragma once

namespace physx {
class PxPhysics;
class PxCpuDispatcher;
class PxMaterial;
class PxRigidStatic;
}

extern physx::PxPhysics * physx_physics;
extern physx::PxCpuDispatcher * physx_dispatcher;
extern physx::PxMaterial * physx_default_material;

void InitPhysX();
void ShutdownPhysX();
