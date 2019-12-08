#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"
#include "client/client.h"
#include "qcommon/cm_local.h"
#include "cgame/cg_local.h"

#include "physx/PxPhysicsAPI.h"

using namespace physx;

static PxDefaultCpuDispatcher * physx_dispatcher;
PxScene * physx_scene;

extern PxPhysics * physx_physics; // TODO
extern PxCooking * physx_cooking;
extern PxMaterial * physx_default_material;

static PxRigidDynamic * sphere;
static PxRigidStatic * map_rigid_body;

static void InitPhysicsCommon() {
	TracyMessageL( "InitPhysicsCommon" );
	ZoneScoped;

	physx_dispatcher = PxDefaultCpuDispatcherCreate( 2 );
	PxSceneDesc sceneDesc(physx_physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, 0.0f, -GRAVITY);
	sceneDesc.cpuDispatcher = physx_dispatcher;
	if(!sceneDesc.cpuDispatcher)
		return; //fatalError("PxDefaultCpuDispatcherCreate failed!");

	sceneDesc.filterShader  = PxDefaultSimulationFilterShader;

	sceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
	sceneDesc.flags |= PxSceneFlag::eENABLE_STABILIZATION;
	sceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
	sceneDesc.sceneQueryUpdateMode = PxSceneQueryUpdateMode::eBUILD_ENABLED_COMMIT_DISABLED;

	physx_scene = physx_physics->createScene(sceneDesc);
	if(!physx_scene)
		return; //fatalError("createScene failed!");

	PxPvdSceneClient * pvd = physx_scene->getScenePvdClient();
	if( pvd != NULL ) {
		pvd->setScenePvdFlag( PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true );
		pvd->setScenePvdFlag( PxPvdSceneFlag::eTRANSMIT_CONTACTS, true );
		pvd->setScenePvdFlag( PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true );
	}
}

void InitPhysics() {
	InitPhysicsCommon();

	map_rigid_body = physx_physics->createRigidStatic( PxTransform( PxIdentity ) );

	{
		const char * suffix = "*0";
		u64 hash = Hash64( suffix, strlen( suffix ), cgs.map->base_hash );
		const Model * model = FindModel( StringHash( hash ) );

		for( u32 i = 0; i < model->num_collision_shapes; i++ ) {
			map_rigid_body->attachShape( *model->collision_shapes[ i ] );
		}
	}

	physx_scene->addActor( *map_rigid_body );

	PxTransform t( PxVec3( -215, 310, 1000 ) );
	sphere = PxCreateDynamic( *physx_physics, t, PxSphereGeometry( 64 ), *physx_default_material, 10.0f );
	sphere->setAngularDamping( 0.5f );
	sphere->setLinearVelocity( PxVec3( 0 ) );
	physx_scene->addActor( *sphere );
}

void InitPhysicsForRagdollEditor() {
	TracyMessageL( "InitPhysicsForRagdollEditor" );
	InitPhysicsCommon();

	PxPlane plane( 0.0f, 0.0f, 1.0f, 40.0f );
	PxRigidStatic * floor = PxCreatePlane( *physx_physics, plane, *physx_default_material );
	physx_scene->addActor( *floor );
}

void ShutdownPhysics() {
	physx_scene->release();
	physx_dispatcher->release();
}

void UpdatePhysicsCommon( float dt ) {
	ZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	u32 scratch_size = 64 * 1024;
	void * scratch = ALLOC_SIZE( &temp, scratch_size, 16 );

	physx_scene->simulate( dt, NULL, scratch, scratch_size );
	physx_scene->fetchResults( true );
}

void UpdatePhysics() {
	UpdatePhysicsCommon( cg.frameTime / 1000.0f );

	PxMat44 physx_transform = PxMat44( sphere->getGlobalPose() );
	Mat4 transform = bit_cast< Mat4 >( physx_transform );

	const Model * model = FindModel( "models/objects/gibs/gib" );
	DrawModel( model, transform * Mat4Scale( 12.8f ), vec4_red );
}
