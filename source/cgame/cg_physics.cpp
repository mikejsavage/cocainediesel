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
	Bone_LeftForearm,

	Bone_RightUpperArm,
	Bone_RightForearm,

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

static void CreateBone( Ragdoll * ragdoll, RagdollBoneType bone, const Model * model, MatrixPalettes pose, u8 j0, u8 j1, float radius ) {
	Vec3 p0 = ( model->transform * pose.joint_poses[ j0 ] ).col3.xyz();
	Vec3 p1 = ( model->transform * pose.joint_poses[ j1 ] ).col3.xyz();

	// TODO: radius
	ragdoll->bones[ bone ].capsule = PxCapsuleGeometry( 3.0f, Length( p1 - p0 ) * 0.5f );

	Vec3 physx_ragdoll_up = Vec3( -1, 0, 0 );
	Vec3 pose_up = p1 - p0;

	Vec3 axis = Normalize( Cross( physx_ragdoll_up, pose_up ) );
	float theta = acosf( Dot( physx_ragdoll_up, Normalize( pose_up ) ) );
	PxQuat q = PxQuat( theta, PxVec3( axis.x, axis.y, axis.z ) );
	Vec3 m = ( p0 + p1 ) * 0.5f;

	PxRigidDynamic * actor = PxCreateDynamic( *physx_physics, PxTransform( m.x, m.y, m.z, q ), ragdoll->bones[ bone ].capsule, *physx_default_material, 10.0f );
	actor->setLinearDamping( 0.5f );
	actor->setAngularDamping( 0.5f );
	ragdoll->aggregate->addActor( *actor );

	ragdoll->bones[ bone ].actor = actor;
}

static PxTransform JointOffsetTransform( const Model * model, MatrixPalettes pose, u8 j0, u8 j1 ) {
	Vec3 p0 = ( model->transform * pose.joint_poses[ j0 ] ).col3.xyz();
	Vec3 p1 = ( model->transform * pose.joint_poses[ j1 ] ).col3.xyz();

	Vec3 d = p1 - p0;

	return PxTransform( -d.z, d.y, d.x, PxQuat( PxPi, PxVec3( 0, 0, 1 ) ) );
}

void AddRagdoll( const Model * model, RagdollConfig config, MatrixPalettes pose ) {
	Ragdoll ragdoll;

	float scale = 60;

	ragdoll.aggregate = physx_physics->createAggregate( Bone_Count, true );

	// geometry
	// ragdoll.bones[ Bone_LowerBack ].capsule = PxCapsuleGeometry( 0.15 * scale, 0.20 * scale * 0.5f);
	// ragdoll.bones[ Bone_UpperBack ].capsule = PxCapsuleGeometry( 0.15 * scale, 0.28 * scale * 0.5f);
	// ragdoll.bones[ Bone_Head ].capsule = PxCapsuleGeometry(0.10 * scale, 0.05 * scale * 0.5f);
	// ragdoll.bones[ Bone_LeftThigh ].capsule = PxCapsuleGeometry(0.07 * scale, 0.45 * scale * 0.5f);
	// ragdoll.bones[ Bone_LeftLowerLeg ].capsule = PxCapsuleGeometry(0.05 * scale, 0.37 * scale * 0.5f);
	// ragdoll.bones[ Bone_RightThigh ].capsule = PxCapsuleGeometry(0.07 * scale, 0.45 * scale * 0.5f);
	// ragdoll.bones[ Bone_RightLowerLeg ].capsule = PxCapsuleGeometry(0.05 * scale, 0.37 * scale * 0.5f);
	// ragdoll.bones[ Bone_LeftUpperArm ].capsule = PxCapsuleGeometry(0.05 * scale, 0.33 * scale * 0.5f);
	// ragdoll.bones[ Bone_LeftForearm ].capsule = PxCapsuleGeometry(0.04 * scale, 0.25 * scale * 0.5f);
	// ragdoll.bones[ Bone_RightUpperArm ].capsule = PxCapsuleGeometry(0.05 * scale, 0.33 * scale * 0.5f);
	// ragdoll.bones[ Bone_RightForearm ].capsule = PxCapsuleGeometry(0.04 * scale, 0.25 * scale * 0.5f);

	// actors
	CreateBone( &ragdoll, Bone_LowerBack, model, pose, config.pelvis, config.spine, config.lower_back_radius );
	CreateBone( &ragdoll, Bone_UpperBack, model, pose, config.spine, config.neck, config.upper_back_radius );
	// TODO: head

	CreateBone( &ragdoll, Bone_LeftUpperArm, model, pose, config.left_shoulder, config.left_elbow, config.left_upper_arm_radius );
	CreateBone( &ragdoll, Bone_LeftForearm, model, pose, config.left_elbow, config.left_wrist, config.left_forearm_radius );

	CreateBone( &ragdoll, Bone_RightUpperArm, model, pose, config.right_shoulder, config.right_elbow, config.right_upper_arm_radius );
	CreateBone( &ragdoll, Bone_RightForearm, model, pose, config.right_elbow, config.right_wrist, config.right_forearm_radius );

	CreateBone( &ragdoll, Bone_LeftThigh, model, pose, config.left_hip, config.left_knee, config.left_thigh_radius );
	CreateBone( &ragdoll, Bone_LeftLowerLeg, model, pose, config.left_knee, config.left_ankle, config.left_lower_leg_radius );
	// TODO: left foot

	CreateBone( &ragdoll, Bone_RightThigh, model, pose, config.right_hip, config.right_knee, config.right_thigh_radius );
	CreateBone( &ragdoll, Bone_RightLowerLeg, model, pose, config.right_knee, config.right_ankle, config.right_lower_leg_radius );
	// TODO: right foot

	// CreateActor( &ragdoll, Bone_LowerBack, transform, 1.0f * scale, 0, 0 );
	// CreateActor( &ragdoll, Bone_UpperBack, transform, 1.4f * scale, 0, 0 );
	// CreateActor( &ragdoll, Bone_Head, transform, 1.75f * scale, 0, 0 );
	// CreateActor( &ragdoll, Bone_LeftThigh, transform, 0.65f * scale, -0.18f * scale, 0 );
	// CreateActor( &ragdoll, Bone_LeftLowerLeg, transform, 0.2f * scale, -0.18f * scale, 0 );
	// CreateActor( &ragdoll, Bone_RightThigh, transform, 0.65f * scale, 0.18f * scale, 0 );
	// CreateActor( &ragdoll, Bone_RightLowerLeg, transform, 0.2f * scale, 0.18f * scale, 0 );
        //
	// PxQuat left_arm_rotation = PxQuat( DEG2RAD( 90 ), PxVec3( 0, 0, 1 ) );
	// CreateActor( &ragdoll, Bone_LeftUpperArm, transform, 1.55f * scale, -0.35f * scale, 0, left_arm_rotation );
	// CreateActor( &ragdoll, Bone_LeftForearm, transform, 1.55f * scale, -0.7f * scale, 0, left_arm_rotation );
        //
	// PxQuat right_arm_rotation = PxQuat( DEG2RAD( -90 ), PxVec3( 0, 0, 1 ) );
	// CreateActor( &ragdoll, Bone_RightUpperArm, transform, 1.55f * scale, 0.35f * scale, 0, right_arm_rotation );
	// CreateActor( &ragdoll, Bone_RightForearm, transform, 1.55f * scale, 0.7f * scale, 0, right_arm_rotation );

	// joints
	// PxSphericalJoint * neck = PxSphericalJointCreate( *physx_physics,
	// 	ragdoll.bones[ Bone_Head ].actor, PxTransform( -ragdoll.bones[ Bone_Head ].capsule.halfHeight, 0, 0 ),
	// 	ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( ragdoll.bones[ Bone_UpperBack ].capsule.halfHeight, 0, 0 ) );
	// neck->setLimitCone( PxJointLimitCone( PxPi / 4, PxPi / 4, 0.01f ) );
	// neck->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	// ragdoll.joints[ Joint_Neck ] = neck;

	PxSphericalJoint * spine = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LowerBack ].actor, PxTransform( -ragdoll.bones[ Bone_LowerBack ].capsule.halfHeight, 0, 0 ),
		ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( ragdoll.bones[ Bone_UpperBack ].capsule.halfHeight, 0, 0 ) );
	spine->setLimitCone( PxJointLimitCone( PxPi / 6, PxPi / 6, 0.01f ) );
	spine->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_Spine ] = spine;

	PxSphericalJoint * left_shoulder = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( -ragdoll.bones[ Bone_UpperBack ].capsule.halfHeight, 0, 0 ) * JointOffsetTransform( model, pose, config.neck, config.left_shoulder ) * PxTransform( PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_LeftUpperArm ].actor, PxTransform( ragdoll.bones[ Bone_LeftUpperArm ].capsule.halfHeight, 0, 0 ) );
	left_shoulder->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 2, 0.01f ) );
	left_shoulder->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftShoulder ] = left_shoulder;

	// PxTransform( PxQuat( PxPi, PxVec3( 1, 0, 0 ) )
	PxRevoluteJoint * left_elbow = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LeftUpperArm ].actor, PxTransform( -ragdoll.bones[ Bone_LeftUpperArm ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) * PxTransform( PxQuat( PxPi, PxVec3( 1, 0, 0 ) ) ),
		ragdoll.bones[ Bone_LeftForearm ].actor, PxTransform( ragdoll.bones[ Bone_LeftForearm ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
	left_elbow->setLimit( PxJointAngularLimitPair( 0, PxPi * 7.0f / 8.0f, 0.1f ) );
	left_elbow->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftElbow ] = left_elbow;

	PxSphericalJoint * right_shoulder = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( -ragdoll.bones[ Bone_UpperBack ].capsule.halfHeight, 0, 0 ) * JointOffsetTransform( model, pose, config.neck, config.right_shoulder ) * PxTransform( PxQuat( -PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_RightUpperArm ].actor, PxTransform( ragdoll.bones[ Bone_RightUpperArm ].capsule.halfHeight, 0, 0 ) );
	right_shoulder->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 2, 0.01f ) );
	right_shoulder->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_RightShoulder ] = right_shoulder;

	PxRevoluteJoint * right_elbow = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_RightUpperArm ].actor, PxTransform( -ragdoll.bones[ Bone_RightUpperArm ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) * PxTransform( PxQuat( PxPi, PxVec3( 1, 0, 0 ) ) ),
		ragdoll.bones[ Bone_RightForearm ].actor, PxTransform( ragdoll.bones[ Bone_RightForearm ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
	right_elbow->setLimit( PxJointAngularLimitPair( 0, PxPi * 7.0f / 8.0f, 0.1f ) );
	right_elbow->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_RightElbow ] = right_elbow;

	PxSphericalJoint * left_hip = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LowerBack ].actor, PxTransform( ragdoll.bones[ Bone_LowerBack ].capsule.halfHeight, 0, 0 ) * JointOffsetTransform( model, pose, config.pelvis, config.left_hip ),
		ragdoll.bones[ Bone_LeftThigh ].actor, PxTransform( ragdoll.bones[ Bone_LeftThigh ].capsule.halfHeight, 0, 0 ) );
	left_hip->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 6, 0.01f ) );
	left_hip->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftHip ] = left_hip;

	PxRevoluteJoint * left_knee = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LeftThigh ].actor, PxTransform( -ragdoll.bones[ Bone_LeftThigh ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_LeftLowerLeg ].actor, PxTransform( ragdoll.bones[ Bone_LeftLowerLeg ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
	left_knee->setLimit( PxJointAngularLimitPair( 0, PxPi * 7.0f / 8.0f, 0.1f ) );
	left_knee->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftKnee ] = left_knee;

	PxSphericalJoint * right_hip = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LowerBack ].actor, PxTransform( ragdoll.bones[ Bone_LowerBack ].capsule.halfHeight, 0, 0 ) * JointOffsetTransform( model, pose, config.pelvis, config.right_hip ),
		ragdoll.bones[ Bone_RightThigh ].actor, PxTransform( ragdoll.bones[ Bone_RightThigh ].capsule.halfHeight, 0, 0 ) );
	right_hip->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 6, 0.01f ) );
	right_hip->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_RightHip ] = right_hip;

	PxRevoluteJoint * right_knee = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_RightThigh ].actor, PxTransform( -ragdoll.bones[ Bone_RightThigh ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_RightLowerLeg ].actor, PxTransform( ragdoll.bones[ Bone_RightLowerLeg ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
	right_knee->setLimit( PxJointAngularLimitPair( 0, PxPi * 7.0f / 8.0f, 0.1f ) );
	right_knee->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_RightKnee ] = right_knee;

	physx_scene->addAggregate( *ragdoll.aggregate );
}

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
