#include "tracy/Tracy.hpp"

#include "physx/PxConfig.h"
#include "physx/PxPhysicsAPI.h"

using namespace physx;

static physx::PxDefaultAllocator allocator;
static physx::PxDefaultErrorCallback errorCallback;

static physx::PxPvd * physx_pvd;

static physx::PxFoundation * physx_foundation;
physx::PxPhysics * physx_physics;
physx::PxCooking * physx_cooking;
physx::PxMaterial * physx_default_material;

void InitPhysx() {
	ZoneScoped;

	physx_foundation = PxCreateFoundation( PX_PHYSICS_VERSION, allocator, errorCallback );
	if(!physx_foundation)
		return; //fatalError("PxCreateFoundation failed!");

	physx_pvd = physx::PxCreatePvd( *physx_foundation );
	physx::PxPvdTransport * transport = physx::PxDefaultPvdSocketTransportCreate( "127.0.0.1", 5425, 10 );
	physx_pvd->connect( *transport, physx::PxPvdInstrumentationFlag::eALL );

	physx::PxTolerancesScale scale;
	physx_physics = PxCreateBasePhysics(PX_PHYSICS_VERSION, *physx_foundation, scale, true, physx_pvd );
	if(!physx_physics)
		return; //fatalError("PxCreatePhysics failed!");

	if( !PxInitExtensions( *physx_physics, physx_pvd ) )
		return; //fatalError("PxInitExtensions failed!");

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

	if( physx_pvd != NULL ) {
		physx_pvd->getTransport()->release();
		physx_pvd->release();
	}

	physx_foundation->release();
}
