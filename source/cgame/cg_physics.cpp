#include <new>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"
#include "client/client.h"
#include "qcommon/cm_local.h"
#include "cgame/cg_local.h"

#include "physx/PxPhysicsAPI.h"

using namespace physx;

static PxDefaultCpuDispatcher * physx_dispatcher;
static PxScene * physx_scene;

extern PxPhysics * physx_physics; // TODO
extern PxCooking * physx_cooking;
extern PxMaterial * physx_default_material;

static PxRigidDynamic * sphere;
static PxRigidStatic * map_rigid_body;

enum RagdollJointType {
	Joint_Pelvis,
	Joint_Spine,
	Joint_Neck,

	Joint_LeftShoulder,
	Joint_LeftElbow,
	Joint_LeftWrist,

	Joint_RightShoulder,
	Joint_RightElbow,
	Joint_RightWrist,

	Joint_LeftHip,
	Joint_LeftKnee,
	Joint_LeftAnkle,

	Joint_RightHip,
	Joint_RightKnee,
	Joint_RightAnkle,

	Joint_Count
};

enum RagdollBoneType {
	Bone_LowerBack,
	Bone_UpperBack,
	Bone_Head,

	Bone_LeftUpperArm,
	Bone_LeftForeArm,

	Bone_RightUpperArm,
	Bone_RightForeArm,

	Bone_LeftThigh,
	Bone_LeftLowerLeg,
	Bone_LeftFoot,

	Bone_RightThigh,
	Bone_RightLowerLeg,
	Bone_RightFoot,

	Bone_Count
};

struct Ragdoll {
	struct Bone {
		PxCapsuleGeometry capsule;
		PxRigidDynamic * actor;
	};

	PxJoint * joints[ Joint_Count ];
	Bone bones[ Bone_Count ];
	PxAggregate * aggregate;
};

static void CreateActor( Ragdoll * ragdoll, RagdollBoneType bone, PxTransform t, float x, float y, float z, PxQuat q = PxQuat( PxIdentity ) ) {
	PxRigidDynamic * actor = PxCreateDynamic( *physx_physics, PxTransform( PxQuat( -PxPi / 2, PxVec3( 0, 1, 0 ) ) ) * PxTransform( x, y, z, q ) * t, ragdoll->bones[ bone ].capsule, *physx_default_material, 10.0f );
	actor->setLinearDamping( 0.5f );
	actor->setAngularDamping( 0.5f );
	ragdoll->aggregate->addActor( *actor );

	ragdoll->bones[ bone ].actor = actor;
}

static Ragdoll AddRagdoll( RagdollConfig config, MatrixPalettes palette ) {
	Ragdoll ragdoll;

	float scale = 60;

	ragdoll.aggregate = physx_physics->createAggregate( Bone_Count, false );

	// geometry
	ragdoll.bones[ Bone_LowerBack ].capsule = PxCapsuleGeometry( 0.15 * scale, 0.20 * scale * 0.5f);
	ragdoll.bones[ Bone_UpperBack ].capsule = PxCapsuleGeometry( 0.15 * scale, 0.28 * scale * 0.5f);
	ragdoll.bones[ Bone_Head ].capsule = PxCapsuleGeometry(0.10 * scale, 0.05 * scale * 0.5f);
	ragdoll.bones[ Bone_LeftThigh ].capsule = PxCapsuleGeometry(0.07 * scale, 0.45 * scale * 0.5f);
	ragdoll.bones[ Bone_LeftLowerLeg ].capsule = PxCapsuleGeometry(0.05 * scale, 0.37 * scale * 0.5f);
	ragdoll.bones[ Bone_RightThigh ].capsule = PxCapsuleGeometry(0.07 * scale, 0.45 * scale * 0.5f);
	ragdoll.bones[ Bone_RightLowerLeg ].capsule = PxCapsuleGeometry(0.05 * scale, 0.37 * scale * 0.5f);
	ragdoll.bones[ Bone_LeftUpperArm ].capsule = PxCapsuleGeometry(0.05 * scale, 0.33 * scale * 0.5f);
	ragdoll.bones[ Bone_LeftForeArm ].capsule = PxCapsuleGeometry(0.04 * scale, 0.25 * scale * 0.5f);
	ragdoll.bones[ Bone_RightUpperArm ].capsule = PxCapsuleGeometry(0.05 * scale, 0.33 * scale * 0.5f);
	ragdoll.bones[ Bone_RightForeArm ].capsule = PxCapsuleGeometry(0.04 * scale, 0.25 * scale * 0.5f);

	PxTransform transform = PxTransform( 0, 0, -150 );

	// actors
	CreateActor( &ragdoll, Bone_LowerBack, transform, 1.0f * scale, 0, 0 );
	CreateActor( &ragdoll, Bone_UpperBack, transform, 1.4f * scale, 0, 0 );
	CreateActor( &ragdoll, Bone_Head, transform, 1.75f * scale, 0, 0 );
	CreateActor( &ragdoll, Bone_LeftThigh, transform, 0.65f * scale, -0.18f * scale, 0 );
	CreateActor( &ragdoll, Bone_LeftLowerLeg, transform, 0.2f * scale, -0.18f * scale, 0 );
	CreateActor( &ragdoll, Bone_RightThigh, transform, 0.65f * scale, 0.18f * scale, 0 );
	CreateActor( &ragdoll, Bone_RightLowerLeg, transform, 0.2f * scale, 0.18f * scale, 0 );

	PxQuat left_arm_rotation = PxQuat( DEG2RAD( 90 ), PxVec3( 0, 0, 1 ) );
	CreateActor( &ragdoll, Bone_LeftUpperArm, transform, 1.55f * scale, -0.35f * scale, 0, left_arm_rotation );
	CreateActor( &ragdoll, Bone_LeftForeArm, transform, 1.55f * scale, -0.7f * scale, 0, left_arm_rotation );

	PxQuat right_arm_rotation = PxQuat( DEG2RAD( -90 ), PxVec3( 0, 0, 1 ) );
	CreateActor( &ragdoll, Bone_RightUpperArm, transform, 1.55f * scale, 0.35f * scale, 0, right_arm_rotation );
	CreateActor( &ragdoll, Bone_RightForeArm, transform, 1.55f * scale, 0.7f * scale, 0, right_arm_rotation );

	// joints
	PxSphericalJoint * neck = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_Head ].actor, PxTransform( ( -0.1f - 0.05f ) * scale, 0, 0 ),
		ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( ( 0.14f + 0.15f ) * scale, 0, 0 ) );
	neck->setLimitCone( PxJointLimitCone( PxPi / 4, PxPi / 4, 0.01f ) );
	neck->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_Neck ] = neck;

	PxSphericalJoint * spine = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( -0.14f * scale, 0, 0 ),
		ragdoll.bones[ Bone_LowerBack ].actor, PxTransform( 0.1f * scale, 0, 0 ) );
	spine->setLimitCone( PxJointLimitCone( PxPi / 6, PxPi / 6, 0.01f ) );
	spine->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_Spine ] = spine;

	PxSphericalJoint * left_shoulder = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( 0.14f * scale, -0.15f * scale, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_LeftUpperArm ].actor, PxTransform( 0.165f * scale, 0, 0 ) );
	left_shoulder->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 2, 0.01f ) );
	left_shoulder->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftShoulder ] = left_shoulder;

	PxRevoluteJoint * left_elbow = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LeftUpperArm ].actor, PxTransform( -0.165 * scale, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_LeftForeArm ].actor, PxTransform( 0.125f * scale, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
	left_elbow->setLimit( PxJointAngularLimitPair( 0, PxPi * 7.0f / 8.0f, 0.1f ) );
	left_elbow->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftElbow ] = left_elbow;

	PxSphericalJoint * right_shoulder = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( 0.14f * scale, 0.15f * scale, 0, PxQuat( -PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_RightUpperArm ].actor, PxTransform( 0.165f * scale, 0, 0 ) );
	right_shoulder->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 2, 0.01f ) );
	right_shoulder->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_RightShoulder ] = right_shoulder;

	PxRevoluteJoint * right_elbow = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_RightUpperArm ].actor, PxTransform( -0.165 * scale, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_RightForeArm ].actor, PxTransform( 0.125f * scale, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
	right_elbow->setLimit( PxJointAngularLimitPair( 0, PxPi * 7.0f / 8.0f, 0.1f ) );
	right_elbow->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftElbow ] = left_elbow;

	PxSphericalJoint * left_hip = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LowerBack ].actor, PxTransform( -0.1f * scale, -0.18f * scale, 0 ),
		ragdoll.bones[ Bone_LeftThigh ].actor, PxTransform( 0.225f * scale, 0, 0 ) );
	left_hip->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 6, 0.01f ) );
	left_hip->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftHip ] = left_hip;

	PxRevoluteJoint * left_knee = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LeftThigh ].actor, PxTransform( PxQuat( PxPi, PxVec3( 1, 0, 0 ) ) ) * PxTransform( -0.225f * scale, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_LeftLowerLeg ].actor, PxTransform( PxQuat( PxPi, PxVec3( 1, 0, 0 ) ) ) * PxTransform( 0.185f * scale, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
	left_knee->setLimit( PxJointAngularLimitPair( 0, PxPi * 7.0f / 8.0f, 0.1f ) );
	left_knee->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftKnee ] = left_knee;

	PxSphericalJoint * right_hip = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LowerBack ].actor, PxTransform( -0.1f * scale, 0.18f * scale, 0 ),
		ragdoll.bones[ Bone_RightThigh ].actor, PxTransform( 0.225f * scale, 0, 0 ) );
	right_hip->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 6, 0.01f ) );
	right_hip->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_RightHip ] = right_hip;

	PxRevoluteJoint * right_knee = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_RightThigh ].actor, PxTransform( PxQuat( PxPi, PxVec3( 1, 0, 0 ) ) ) * PxTransform( -0.225f * scale, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_RightLowerLeg ].actor, PxTransform( PxQuat( PxPi, PxVec3( 1, 0, 0 ) ) ) * PxTransform( 0.185f * scale, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
	right_knee->setLimit( PxJointAngularLimitPair( 0, PxPi * 7.0f / 8.0f, 0.1f ) );
	right_knee->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_RightKnee ] = right_knee;

	return ragdoll;
}

static int64_t last_reset;
void InitPhysics() {
	last_reset = cg.monotonicTime;
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

	Ragdoll ragdoll = AddRagdoll( RagdollConfig(), MatrixPalettes() );
	physx_scene->addAggregate( *ragdoll.aggregate );
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

	PxMat44 physx_transform = PxMat44( sphere->getGlobalPose() );
	Mat4 transform = bit_cast< Mat4 >( physx_transform );

	const Model * model = FindModel( "models/objects/gibs/gib" );
	DrawModel( model, transform * Mat4Scale( 12.8f ), vec4_red );
}
