#include "physx/PxPhysicsAPI.h"

static physx::PxDefaultAllocator allocator;
static physx::PxDefaultErrorCallback errorCallback;

static physx::PxFoundation * physx_foundation;
physx::PxPhysics * physx_physics;
physx::PxCooking * physx_cooking;
physx::PxMaterial * physx_default_material;

void InitPhysx() {
	physx_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorCallback);
	if(!physx_foundation)
		return; //fatalError("PxCreateFoundation failed!");

	physx::PxTolerancesScale scale;
	physx_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *physx_foundation, scale );
	if(!physx_physics)
		return; //fatalError("PxCreatePhysics failed!");

	physx::PxCookingParams params(scale);
	params.meshWeldTolerance = 0.001f;
	params.meshPreprocessParams = physx::PxMeshPreprocessingFlags(physx::PxMeshPreprocessingFlag::eWELD_VERTICES);
	physx_cooking = PxCreateCooking(PX_PHYSICS_VERSION, *physx_foundation, params);
	if(!physx_cooking)
		return; //fatalError("PxCreateCooking failed!");

	physx_default_material = physx_physics->createMaterial(0.5f, 0.5f, 0.9f);
	if(!physx_default_material)
		return; //fatalError("createMaterial failed!");
}

void ShutdownPhysx() {
	physx_physics->release();
	physx_foundation->release();
}
