#include <new>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"
#include "client/client.h"
#include "qcommon/cm_local.h"
#include "cgame/cg_local.h"

#include "physx/PxPhysicsAPI.h"

using namespace physx;

static physx::PxDefaultCpuDispatcher * physx_dispatcher;
static physx::PxScene * physx_scene;

extern physx::PxPhysics * physx_physics; // TODO
extern physx::PxCooking * physx_cooking;
extern physx::PxMaterial * physx_default_material;

static physx::PxRigidDynamic * sphere;
static physx::PxRigidStatic * map_rigid_body;

enum {
	BODYPART_PELVIS = 0,
	BODYPART_SPINE,
	BODYPART_HEAD,

	BODYPART_LEFT_UPPER_LEG,
	BODYPART_LEFT_LOWER_LEG,

	BODYPART_RIGHT_UPPER_LEG,
	BODYPART_RIGHT_LOWER_LEG,

	BODYPART_LEFT_UPPER_ARM,
	BODYPART_LEFT_LOWER_ARM,

	BODYPART_RIGHT_UPPER_ARM,
	BODYPART_RIGHT_LOWER_ARM,

	BODYPART_COUNT
};

enum {
	JOINT_PELVIS_SPINE = 0,
	JOINT_SPINE_HEAD,

	JOINT_LEFT_HIP,
	JOINT_LEFT_KNEE,

	JOINT_RIGHT_HIP,
	JOINT_RIGHT_KNEE,

	JOINT_LEFT_SHOULDER,
	JOINT_LEFT_ELBOW,

	JOINT_RIGHT_SHOULDER,
	JOINT_RIGHT_ELBOW,

	JOINT_COUNT
};

struct Bone {
	physx::PxCapsuleGeometry capsule;
	physx::PxRigidDynamic * actor;
};

static Bone bones[ BODYPART_COUNT ];
static physx::PxJoint * joints[ JOINT_COUNT ];

static void asdf( int part, PxTransform t, float x, float y, float z, physx::PxQuat q = physx::PxQuat( physx::PxIdentity ) ) {
	bones[ part ].actor = physx::PxCreateDynamic( *physx_physics, PxTransform( x, y, z, q ) * t, bones[ part ].capsule, *physx_default_material, 10.0f );
	bones[ part ].actor->setLinearDamping( 0.5f );
	bones[ part ].actor->setAngularDamping( 0.5f );
	physx_scene->addActor( *bones[ part ].actor );
}

static void AddRagdoll() {
	float scale = 60;

	// Setup the geometry
	bones[BODYPART_PELVIS].capsule = physx::PxCapsuleGeometry( 0.15 * scale, 0.20 * scale * 0.5f);
	bones[BODYPART_SPINE].capsule = physx::PxCapsuleGeometry(0.15 * scale, 0.28 * scale * 0.5f);
	bones[BODYPART_HEAD].capsule = physx::PxCapsuleGeometry(0.10 * scale, 0.05 * scale * 0.5f);
	bones[BODYPART_LEFT_UPPER_LEG].capsule = physx::PxCapsuleGeometry(0.07 * scale, 0.45 * scale * 0.5f);
	bones[BODYPART_LEFT_LOWER_LEG].capsule = physx::PxCapsuleGeometry(0.05 * scale, 0.37 * scale * 0.5f);
	bones[BODYPART_RIGHT_UPPER_LEG].capsule = physx::PxCapsuleGeometry(0.07 * scale, 0.45 * scale * 0.5f);
	bones[BODYPART_RIGHT_LOWER_LEG].capsule = physx::PxCapsuleGeometry(0.05 * scale, 0.37 * scale * 0.5f);
	bones[BODYPART_LEFT_UPPER_ARM].capsule = physx::PxCapsuleGeometry(0.05 * scale, 0.33 * scale * 0.5f);
	bones[BODYPART_LEFT_LOWER_ARM].capsule = physx::PxCapsuleGeometry(0.04 * scale, 0.25 * scale * 0.5f);
	bones[BODYPART_RIGHT_UPPER_ARM].capsule = physx::PxCapsuleGeometry(0.05 * scale, 0.33 * scale * 0.5f);
	bones[BODYPART_RIGHT_LOWER_ARM].capsule = physx::PxCapsuleGeometry(0.04 * scale, 0.25 * scale * 0.5f);

	physx::PxVec3 start_pos( 0, 0, -150 );
	physx::PxTransform transform = physx::PxTransformFromSegment( start_pos, start_pos + physx::PxVec3( 0, 0, 1 ) );

	// Setup all the rigid bodies
	asdf( BODYPART_PELVIS, transform, 0, 0, 1.0f * scale );
	asdf( BODYPART_SPINE, transform, 0, 0, 1.2f * scale );
	asdf( BODYPART_HEAD, transform, 0, 0, 1.6f * scale );
	asdf( BODYPART_LEFT_UPPER_LEG, transform, -0.18f * scale, 0, 0.65f * scale );
	asdf( BODYPART_LEFT_LOWER_LEG, transform, -0.18f * scale, 0, 0.2f * scale );
	asdf( BODYPART_RIGHT_UPPER_LEG, transform, 0.18f * scale, 0, 0.65f * scale );
	asdf( BODYPART_RIGHT_LOWER_LEG, transform, 0.18f * scale, 0, 0.2f * scale );

	physx::PxQuat left_arm_rotation = physx::PxQuat( DEG2RAD( 90 ), physx::PxVec3( 0, 1, 0 ) );
	asdf( BODYPART_LEFT_UPPER_ARM, transform, -0.35f * scale, 0, 1.45f * scale, left_arm_rotation );
	asdf( BODYPART_LEFT_LOWER_ARM, transform, -0.7f * scale, 0, 1.45f * scale, left_arm_rotation );

	physx::PxQuat right_arm_rotation = physx::PxQuat( DEG2RAD( -90 ), physx::PxVec3( 0, 1, 0 ) );
	asdf( BODYPART_RIGHT_UPPER_ARM, transform, 0.35f * scale, 0, 1.45f * scale, right_arm_rotation );
	asdf( BODYPART_RIGHT_LOWER_ARM, transform, 0.7f * scale, 0, 1.45f * scale, right_arm_rotation );

	// Now setup the constraints
	// btHingeConstraint* hingeC;
	// btConeTwistConstraint* coneC;
        //
	// btTransform localA, localB;
        //
	// localA.setIdentity();
	// localB.setIdentity();
	// localA.getBasis().setEulerZYX(M_PI_2, 0, 0);
	// localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.15)));
	// localB.getBasis().setEulerZYX(M_PI_2, 0, 0);
	// localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.15)));
	// hingeC = new btHingeConstraint(*bones[BODYPART_PELVIS].rigid_body, *bones[BODYPART_SPINE].rigid_body, localA, localB);
	// hingeC->setLimit(btScalar(-M_PI_4), btScalar(M_PI_2));
	// joints[JOINT_PELVIS_SPINE] = hingeC;
	// dynamics_world->addConstraint(joints[JOINT_PELVIS_SPINE], true);
        //
	// localA.setIdentity();
	// localB.setIdentity();
	// localA.getBasis().setEulerZYX(0, 0, M_PI_2);
	// localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.30)));
	// localB.getBasis().setEulerZYX(0, 0, M_PI_2);
	// localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.14)));
	// coneC = new btConeTwistConstraint(*bones[BODYPART_SPINE].rigid_body, *bones[BODYPART_HEAD].rigid_body, localA, localB);
	// coneC->setLimit(M_PI_4, M_PI_4, M_PI_2);
	// joints[JOINT_SPINE_HEAD] = coneC;
	// dynamics_world->addConstraint(joints[JOINT_SPINE_HEAD], true);
        //
	// localA.setIdentity();
	// localB.setIdentity();
	// localA.getBasis().setEulerZYX(0, 0, -M_PI_4 * 5);
	// localA.setOrigin(scale * btVector3(btScalar(-0.18), btScalar(0), btScalar(-0.1)));
	// localB.getBasis().setEulerZYX(0, 0, -M_PI_4 * 5);
	// localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.225)));
	// coneC = new btConeTwistConstraint(*bones[BODYPART_PELVIS].rigid_body, *bones[BODYPART_LEFT_UPPER_LEG].rigid_body, localA, localB);
	// coneC->setLimit(M_PI_4, M_PI_4, 0);
	// joints[JOINT_LEFT_HIP] = coneC;
	// dynamics_world->addConstraint(joints[JOINT_LEFT_HIP], true);

	PxSphericalJoint * left_hip =  physx::PxSphericalJointCreate( *physx_physics,
		bones[ BODYPART_PELVIS ].actor, PxTransform( 0 , 0, 0.225 * scale ),
		bones[ BODYPART_LEFT_UPPER_LEG ].actor, PxTransform( -0.18f * scale, 0, -0.1f * scale ) );
	left_hip->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 6, 0.01f ) );
	left_hip->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	joints[ JOINT_LEFT_HIP ] = left_hip;

	PxRevoluteJoint * left_knee =  physx::PxRevoluteJointCreate( *physx_physics,
		bones[ BODYPART_LEFT_UPPER_LEG ].actor, PxTransform( -0.225f * scale, 0, 0, PxQuat( DEG2RAD( 90 ), physx::PxVec3( 0, 0, 1 ) ) ),
		bones[ BODYPART_LEFT_LOWER_LEG ].actor, PxTransform( 0.185f * scale, 0, 0, PxQuat( DEG2RAD( 90 ), physx::PxVec3( 0, 0, 1 ) ) ) );
	left_knee->setLimit( PxJointAngularLimitPair( 0, PxPi, 0.1f ) );
	left_knee->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	joints[ JOINT_LEFT_KNEE ] = left_knee;

	// localA.setIdentity();
	// localB.setIdentity();
	// localA.getBasis().setEulerZYX(0, 0, M_PI_4);
	// localA.setOrigin(scale * btVector3(btScalar(0.18), btScalar(0), btScalar(-0.10)));
	// localB.getBasis().setEulerZYX(0, 0, M_PI_4);
	// localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.225)));
	// coneC = new btConeTwistConstraint(*bones[BODYPART_PELVIS].rigid_body, *bones[BODYPART_RIGHT_UPPER_LEG].rigid_body, localA, localB);
	// coneC->setLimit(M_PI_4, M_PI_4, 0);
	// joints[JOINT_RIGHT_HIP] = coneC;
	// dynamics_world->addConstraint(joints[JOINT_RIGHT_HIP], true);

	PxSphericalJoint * right_hip =  physx::PxSphericalJointCreate( *physx_physics,
		bones[ BODYPART_PELVIS ].actor, PxTransform( 0, 0, 0.225 * scale ),
		bones[ BODYPART_RIGHT_UPPER_LEG ].actor, PxTransform( 0.18f * scale, 0, -0.1f * scale ) );
	right_hip->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 6, 0.01f ) );
	right_hip->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	joints[ JOINT_RIGHT_HIP ] = right_hip;

	PxRevoluteJoint * right_knee = physx::PxRevoluteJointCreate( *physx_physics,
		bones[ BODYPART_RIGHT_UPPER_LEG ].actor, PxTransform( -0.225f * scale, 0, 0, PxQuat( DEG2RAD( 90 ), physx::PxVec3( 0, 0, 1 ) ) ),
		bones[ BODYPART_RIGHT_LOWER_LEG ].actor, PxTransform( 0.185f * scale, 0, 0, PxQuat( DEG2RAD( 90 ), physx::PxVec3( 0, 0, 1 ) ) ) );
	right_knee->setLimit( PxJointAngularLimitPair( 0, PxPi, 0.1f ) );
	right_knee->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	joints[ JOINT_RIGHT_KNEE ] = right_knee;

	// localA.setIdentity();
	// localB.setIdentity();
	// localA.getBasis().setEulerZYX(0, 0, M_PI);
	// localA.setOrigin(scale * btVector3(btScalar(-0.2), btScalar(0), btScalar(0.15)));
	// localB.getBasis().setEulerZYX(0, 0, M_PI_2);
	// localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.18)));
	// coneC = new btConeTwistConstraint(*bones[BODYPART_SPINE].rigid_body, *bones[BODYPART_LEFT_UPPER_ARM].rigid_body, localA, localB);
	// coneC->setLimit(M_PI_2, M_PI_2, 0);
	// joints[JOINT_LEFT_SHOULDER] = coneC;
	// dynamics_world->addConstraint(joints[JOINT_LEFT_SHOULDER], true);
        //
	// localA.setIdentity();
	// localB.setIdentity();
	// localA.getBasis().setEulerZYX(M_PI_2, 0, 0);
	// localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.18)));
	// localB.getBasis().setEulerZYX(M_PI_2, 0, 0);
	// localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.14)));
	// hingeC = new btHingeConstraint(*bones[BODYPART_LEFT_UPPER_ARM].rigid_body, *bones[BODYPART_LEFT_LOWER_ARM].rigid_body, localA, localB);
	// hingeC->setLimit(btScalar(-M_PI_2), btScalar(0));
	// joints[JOINT_LEFT_ELBOW] = hingeC;
	// dynamics_world->addConstraint(joints[JOINT_LEFT_ELBOW], true);
        //
	// localA.setIdentity();
	// localB.setIdentity();
	// localA.getBasis().setEulerZYX(0, 0, 0);
	// localA.setOrigin(scale * btVector3(btScalar(0.2), btScalar(0), btScalar(0.15)));
	// localB.getBasis().setEulerZYX(0, 0, M_PI_2);
	// localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.18)));
	// coneC = new btConeTwistConstraint(*bones[BODYPART_SPINE].rigid_body, *bones[BODYPART_RIGHT_UPPER_ARM].rigid_body, localA, localB);
	// coneC->setLimit(M_PI_2, M_PI_2, 0);
	// joints[JOINT_RIGHT_SHOULDER] = coneC;
	// dynamics_world->addConstraint(joints[JOINT_RIGHT_SHOULDER], true);
        //
	// localA.setIdentity();
	// localB.setIdentity();
	// localA.getBasis().setEulerZYX(M_PI_2, 0, 0);
	// localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.18)));
	// localB.getBasis().setEulerZYX(M_PI_2, 0, 0);
	// localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.14)));
	// hingeC = new btHingeConstraint(*bones[BODYPART_RIGHT_UPPER_ARM].rigid_body, *bones[BODYPART_RIGHT_LOWER_ARM].rigid_body, localA, localB);
	// hingeC->setLimit(btScalar(-M_PI_2), btScalar(0));
	// joints[JOINT_RIGHT_ELBOW] = hingeC;
	// dynamics_world->addConstraint(joints[JOINT_RIGHT_ELBOW], true);
}

static int64_t last_reset;
void InitPhysics() {
	last_reset = cg.monotonicTime;
	ZoneScoped;

	physx_dispatcher = physx::PxDefaultCpuDispatcherCreate( 2 );
	physx::PxSceneDesc sceneDesc(physx_physics->getTolerancesScale());
	sceneDesc.gravity = physx::PxVec3(0.0f, 0.0f, -GRAVITY);
	sceneDesc.cpuDispatcher = physx_dispatcher;
	if(!sceneDesc.cpuDispatcher)
		return; //fatalError("PxDefaultCpuDispatcherCreate failed!");

	sceneDesc.filterShader  = physx::PxDefaultSimulationFilterShader;

	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_PCM;
	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_STABILIZATION;
	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
	sceneDesc.sceneQueryUpdateMode = physx::PxSceneQueryUpdateMode::eBUILD_ENABLED_COMMIT_DISABLED;

	physx_scene = physx_physics->createScene(sceneDesc);
	if(!physx_scene)
		return; //fatalError("createScene failed!");

	physx::PxPvdSceneClient * pvd = physx_scene->getScenePvdClient();
	if( pvd != NULL ) {
		pvd->setScenePvdFlag( physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true );
		pvd->setScenePvdFlag( physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS, true );
		pvd->setScenePvdFlag( physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true );
	}

	map_rigid_body = physx_physics->createRigidStatic( physx::PxTransform( physx::PxIdentity ) );

	{
		const char * suffix = "*0";
		u64 hash = Hash64( suffix, strlen( suffix ), cgs.map->base_hash );
		const Model * model = FindModel( StringHash( hash ) );

		for( u32 i = 0; i < model->num_collision_shapes; i++ ) {
			map_rigid_body->attachShape( *model->collision_shapes[ i ] );
		}
	}

	physx_scene->addActor( *map_rigid_body );

	physx::PxTransform t( physx::PxVec3( -215, 310, 1000 ) );
	sphere = physx::PxCreateDynamic( *physx_physics, t, physx::PxSphereGeometry( 64 ), *physx_default_material, 10.0f );
	sphere->setAngularDamping( 0.5f );
	sphere->setLinearVelocity( physx::PxVec3( 0 ) );
	physx_scene->addActor( *sphere );

	AddRagdoll();
}

void ShutdownPhysics() {
	physx_scene->release();
	physx_dispatcher->release();
}

void UpdatePhysics() {
	ZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	u32 scratch_size = 64 * 1024;
	void * scratch = ALLOC_SIZE( &temp, scratch_size, 16 );

	float dt = cg.frameTime / 1000.0f;
	physx_scene->simulate( dt, NULL, scratch, scratch_size );
	physx_scene->fetchResults( true );

	physx::PxMat44 physx_transform = physx::PxMat44( sphere->getGlobalPose() );
	Mat4 transform = bit_cast< Mat4 >( physx_transform );

	const Model * model = FindModel( "models/objects/gibs/gib" );
	DrawModel( model, transform * Mat4Scale( 12.8f ), vec4_red );
}
