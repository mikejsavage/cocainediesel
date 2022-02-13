#include "qcommon/base.h"
#include "gameshared/gs_public.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/physx.h"

#include "physx/PxConfig.h"
#include "physx/PxPhysicsAPI.h"

using namespace physx;

PxScene * physx_scene;

static PxRigidDynamic * balls[ 128 ];

void InitPhysics() {
	PxSceneDesc sceneDesc(physx_physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, 0.0f, -GRAVITY);
	sceneDesc.cpuDispatcher = physx_dispatcher;
	if(!sceneDesc.cpuDispatcher)
		return; //fatalError("PxDefaultCpuDispatcherCreate failed!");

	sceneDesc.filterShader = PxDefaultSimulationFilterShader;

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

	physx_scene->addActor( *cl.map->physx );

	for( size_t i = 0; i < ARRAY_COUNT( balls ); i++ ) {
		PxTransform t( PxVec3( RandomUniformFloat( &cls.rng, -500, 500 ), RandomUniformFloat( &cls.rng, -500, 500 ), 1000 ) );
		balls[ i ] = PxCreateDynamic( *physx_physics, t, PxSphereGeometry( 64 ), *physx_default_material, 10.0f );
		balls[ i ]->setAngularDamping( 0.5f );
		balls[ i ]->setLinearVelocity( PxVec3( 0 ) );
		physx_scene->addActor( *balls[ i ] );
	}
}

void ShutdownPhysics() {
	physx_scene->release();
}

void PhysicsFrame( float dt ) {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	u32 scratch_size = 64 * 1024;
	void * scratch = ALLOC_SIZE( &temp, scratch_size, 16 );

	physx_scene->simulate( dt, NULL, scratch, scratch_size );
	physx_scene->fetchResults( true );
}

void DrawTheBall() {
	const Model * model = FindModel( "models/sphere" );

	for( PxRigidDynamic * ball : balls ) {
		PxMat44 physx_transform = PxMat44( ball->getGlobalPose() );
		Mat4 transform = bit_cast< Mat4 >( physx_transform );

		DrawModelConfig config = { };
		config.draw_model.enabled = true;
		config.draw_shadows.enabled = true;
		DrawModel( config, model, transform * Mat4Scale( 64.0f ), vec4_red );
	}
}
