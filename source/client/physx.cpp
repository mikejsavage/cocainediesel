#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"

#include "physx/PxConfig.h"
#include "physx/PxPhysicsAPI.h"

using namespace physx;

static PxDefaultAllocator allocator;
static PxDefaultErrorCallback errorCallback;

static PxPvd * physx_pvd;

static PxFoundation * physx_foundation;
PxPhysics * physx_physics;
PxMaterial * physx_default_material;

void InitPhysX() {
	TracyZoneScoped;

	physx_foundation = PxCreateFoundation( PX_PHYSICS_VERSION, allocator, errorCallback );
	if(!physx_foundation)
		return; //fatalError("PxCreateFoundation failed!");

	physx_pvd = PxCreatePvd( *physx_foundation );
	PxPvdTransport * transport = PxDefaultPvdSocketTransportCreate( "127.0.0.1", 5425, 10 );
	physx_pvd->connect( *transport, PxPvdInstrumentationFlag::eALL );

	PxTolerancesScale scale;
	scale.length = 32;
	scale.speed = GRAVITY;
	physx_physics = PxCreateBasePhysics(PX_PHYSICS_VERSION, *physx_foundation, scale, true, physx_pvd );
	if(!physx_physics)
		return; //fatalError("PxCreatePhysics failed!");

	if( !PxInitExtensions( *physx_physics, physx_pvd ) )
		return; //fatalError("PxInitExtensions failed!");

	physx_default_material = physx_physics->createMaterial( 0.5f, 0.5f, 0.1f );
	if(!physx_default_material)
		return; //fatalError("createMaterial failed!");
}

void ShutdownPhysX() {
	TracyZoneScoped;

	physx_physics->release();

	if( physx_pvd != NULL ) {
		physx_pvd->getTransport()->release();
		physx_pvd->release();
	}

	physx_foundation->release();
}
