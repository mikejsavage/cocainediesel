#include "qcommon/base.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "cgame/cg_local.h"

#include "imgui/imgui.h"

#include "physx/PxPhysicsAPI.h"

using namespace physx;

extern PxScene * physx_scene; // TODO
extern PxPhysics * physx_physics;
extern PxCooking * physx_cooking;
extern PxMaterial * physx_default_material;

static RagdollConfig editor_ragdoll_config;
static float editor_t;
static bool editor_draw_model;
static bool editor_simulate;

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

static Ragdoll editor_ragdoll;
static Span< TRS > editor_initial_local_pose;

static PxVec4 ToPhysx( Vec4 v ) {
	return PxVec4( v.x, v.y, v.z, v.w );
}

static PxMat44 ToPhysx( Mat4 m ) {
	return PxMat44(
		ToPhysx( m.col0 ),
		ToPhysx( m.col1 ),
		ToPhysx( m.col2 ),
		ToPhysx( m.col3 )
	);
}

static void CreateBone( Ragdoll * ragdoll, RagdollBoneType bone, const Model * model, MatrixPalettes pose, u8 j0, u8 j1, float radius ) {
	Vec3 p0 = ( model->transform * pose.node_transforms[ j0 ] ).col3.xyz();
	Vec3 p1 = ( model->transform * pose.node_transforms[ j1 ] ).col3.xyz();

	// TODO: radius
	float bone_length = Length( p1 - p0 );
	ragdoll->bones[ bone ].capsule = PxCapsuleGeometry( 3.0f, bone_length * 0.5f );

	Mat4 t = Mat4Translation( Vec3( bone_length * 0.5f, 0.0f, 0.0f ) );
	Mat4 t2 = model->transform * pose.node_transforms[ j0 ] * t;
	PxTransform pxt = PxTransform( ToPhysx( t2 ) ).getNormalized();

	PxRigidDynamic * actor = PxCreateDynamic( *physx_physics, pxt, ragdoll->bones[ bone ].capsule, *physx_default_material, 10.0f );
	actor->setLinearDamping( 0.5f );
	actor->setAngularDamping( 0.5f );
	ragdoll->aggregate->addActor( *actor );

	ragdoll->bones[ bone ].actor = actor;
}

/*
static void CreateBone( Ragdoll * ragdoll, RagdollBoneType bone, const Model * model, MatrixPalettes pose, u8 j0, u8 j1, float radius ) {
	Vec3 p0 = ( model->transform * pose.node_transforms[ j0 ] ).col3.xyz();
	Vec3 p1 = ( model->transform * pose.node_transforms[ j1 ] ).col3.xyz();

	// TODO: radius
	ragdoll->bones[ bone ].capsule = PxCapsuleGeometry( 3.0f, Length( p1 - p0 ) * 0.5f );

	Vec3 physx_ragdoll_up = Vec3( -1, 0, 0 );
	Vec3 pose_up = p1 - p0;

	// TODO: this does a lookat transform so the bone's up vector is
	// aligned with the world up vector. we would like the bone to be
	// twisted like how it is in the skeleton instead
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
*/

static PxTransform JointOffsetTransform( const Model * model, MatrixPalettes pose, u8 j0, u8 j1 ) {
	Vec3 p0 = ( model->transform * pose.node_transforms[ j0 ] ).col3.xyz();
	Vec3 p1 = ( model->transform * pose.node_transforms[ j1 ] ).col3.xyz();

	Vec3 d = p1 - p0;

	return PxTransform( -d.z, d.y, d.x, PxQuat( PxPi, PxVec3( 0, 0, 1 ) ) );
}

static Ragdoll AddRagdoll( const Model * model, RagdollConfig config, MatrixPalettes pose ) {
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

#if 0
	PxSphericalJoint * left_shoulder = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( -ragdoll.bones[ Bone_UpperBack ].capsule.halfHeight, 0, 0 ) * JointOffsetTransform( model, pose, config.neck, config.left_shoulder ) * PxTransform( PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_LeftUpperArm ].actor, PxTransform( ragdoll.bones[ Bone_LeftUpperArm ].capsule.halfHeight, 0, 0 ) );
	left_shoulder->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 2, 0.01f ) );
	left_shoulder->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftShoulder ] = left_shoulder;

	PxRevoluteJoint * left_elbow = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_LeftUpperArm ].actor, PxTransform( -ragdoll.bones[ Bone_LeftUpperArm ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) * PxTransform( PxQuat( PxPi, PxVec3( 1, 0, 0 ) ) ),
		ragdoll.bones[ Bone_LeftForearm ].actor, PxTransform( ragdoll.bones[ Bone_LeftForearm ].capsule.halfHeight, 0, 0, PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
	left_elbow->setLimit( PxJointAngularLimitPair( 0, PxPi * 7.0f / 8.0f, 0.1f ) );
	left_elbow->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_LeftElbow ] = left_elbow;

#endif

	PxSphericalJoint * right_shoulder = PxSphericalJointCreate( *physx_physics,
		ragdoll.bones[ Bone_UpperBack ].actor, PxTransform( -ragdoll.bones[ Bone_UpperBack ].capsule.halfHeight, 0, 0 ) * JointOffsetTransform( model, pose, config.neck, config.right_shoulder ) * PxTransform( PxQuat( -PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_RightUpperArm ].actor, PxTransform( ragdoll.bones[ Bone_RightUpperArm ].capsule.halfHeight, 0, 0 ) );
	right_shoulder->setLimitCone( PxJointLimitCone( PxPi / 2, PxPi / 2, 0.01f ) );
	right_shoulder->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
	ragdoll.joints[ Joint_RightShoulder ] = right_shoulder;

#if 0
	PxRevoluteJoint * right_elbow = PxRevoluteJointCreate( *physx_physics,
		ragdoll.bones[ Bone_RightUpperArm ].actor, PxTransform( -ragdoll.bones[ Bone_RightUpperArm ].capsule.halfHeight, 0, 0, PxQuat( PxPi, PxVec3( 1, 0, 0 ) ) ) * PxTransform( PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ),
		ragdoll.bones[ Bone_RightForearm ].actor, PxTransform( ragdoll.bones[ Bone_RightForearm ].capsule.halfHeight, 0, 0, PxQuat( PxPi, PxVec3( 1, 0, 0 ) ) ) * PxTransform( PxQuat( PxPi / 2, PxVec3( 0, 0, 1 ) ) ) );
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
#endif

	physx_scene->addAggregate( *ragdoll.aggregate );

	return ragdoll;
}

void InitRagdollEditor() {
}

void ResetRagdollEditor() {
	editor_ragdoll_config.pelvis = U8_MAX;
	editor_ragdoll_config.spine = U8_MAX;
	editor_ragdoll_config.neck = U8_MAX;
	editor_ragdoll_config.left_shoulder = U8_MAX;
	editor_ragdoll_config.left_elbow = U8_MAX;
	editor_ragdoll_config.left_wrist = U8_MAX;
	editor_ragdoll_config.right_shoulder = U8_MAX;
	editor_ragdoll_config.right_elbow = U8_MAX;
	editor_ragdoll_config.right_wrist = U8_MAX;
	editor_ragdoll_config.left_hip = U8_MAX;
	editor_ragdoll_config.left_knee = U8_MAX;
	editor_ragdoll_config.left_ankle = U8_MAX;
	editor_ragdoll_config.right_hip = U8_MAX;
	editor_ragdoll_config.right_knee = U8_MAX;
	editor_ragdoll_config.right_ankle = U8_MAX;

	editor_ragdoll_config.pelvis = 3;
	editor_ragdoll_config.spine = 4;
	editor_ragdoll_config.neck = 6;
	editor_ragdoll_config.left_shoulder = 16;
	editor_ragdoll_config.left_elbow = 17;
	editor_ragdoll_config.left_wrist = 18;
	editor_ragdoll_config.right_shoulder = 10;
	editor_ragdoll_config.right_elbow = 11;
	editor_ragdoll_config.right_wrist = 12;
	editor_ragdoll_config.left_hip = 21;
	editor_ragdoll_config.left_knee = 22;
	editor_ragdoll_config.left_ankle = 23;
	editor_ragdoll_config.right_hip = 25;
	editor_ragdoll_config.right_knee = 26;
	editor_ragdoll_config.right_ankle = 27;

	editor_t = 5.0f;
	editor_draw_model = true;
	editor_simulate = false;
}

static Mat4 TransformKToSegment( Vec3 start, Vec3 end ) {
	Vec3 K = Vec3( 0, 0, 1 );
	Vec3 dir = end - start;

	Vec3 axis = Normalize( Cross( K, dir ) );
	float c = Dot( K, dir ) / Length( dir );
	float s = sqrtf( 1.0f - c * c );

	Mat4 rotation = Mat4RotationAxisSinCos( axis, s, c );
	Mat4 translation = Mat4Translation( start );
	Mat4 scale = Mat4Scale( Length( dir ) );
	return translation * rotation * scale;
}

static void DrawBone( const Model * model, MatrixPalettes matrices, u8 j0, u8 j1, float radius ) {
	if( j0 == U8_MAX || j1 == U8_MAX )
		return;

	PipelineState pipeline;
	pipeline.pass = frame_static.nonworld_opaque_pass;
	pipeline.shader = &shaders.standard;
	pipeline.depth_func = DepthFunc_Disabled;
	pipeline.set_texture( "u_BaseTexture", cls.white_material->texture );
	pipeline.set_uniform( "u_View", frame_static.view_uniforms );
	pipeline.set_uniform( "u_Material", frame_static.identity_material_uniforms );

	const Model * capsule = FindModel( "models/capsule" );
	Vec3 p0 = ( model->transform * matrices.node_transforms[ j0 ].col3 ).xyz();
	Vec3 p1 = ( model->transform * matrices.node_transforms[ j1 ].col3 ).xyz();

	if( p0 == p1 )
		return;

	pipeline.set_uniform( "u_Model", UploadModelUniforms( TransformKToSegment( p0, p1 ) * capsule->transform ) );

	DrawModelPrimitive( FindModel( "models/capsule" ), &capsule->primitives[ 0 ], pipeline );
}

static void JointPicker( const Model * model, const char * label, u8 * joint ) {
	if( ImGui::BeginCombo( label, *joint == U8_MAX ? "None" : model->nodes[ *joint ].name ) ) {
		if( ImGui::Selectable( "None", *joint == U8_MAX ) )
			*joint = U8_MAX;
		if( *joint == U8_MAX )
			ImGui::SetItemDefaultFocus();

		for( u8 i = 0; i < model->num_nodes; i++ ) {
			String< 128 > buf( "{} {}", i, model->nodes[ i ].name );
			if( ImGui::Selectable( buf.c_str(), *joint == i ) )
				*joint = i;
			if( *joint == i )
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}
}

// TODO
void InitPhysicsForRagdollEditor();
void ShutdownPhysics();
void UpdatePhysicsCommon( float dt );

static TRS PhysxToTRS( PxTransform t ) {
	TRS trs;

	trs.translation.x = t.p.x;
	trs.translation.y = t.p.y;
	trs.translation.z = t.p.z;

	trs.rotation.x = t.q.x;
	trs.rotation.y = t.q.y;
	trs.rotation.z = t.q.z;
	trs.rotation.w = t.q.w;

	trs.scale = 1.0f;

	return trs;
}

static Quaternion PhysxToDE( PxQuat q ) {
	q = PxQuat( -PxPi / 2, PxVec3( 0, 0, 1 ) ) * q;
	return Quaternion( q.x, q.y, q.z, q.w );
}

static void format( FormatBuffer * fb, const PxVec3 & v, const FormatOpts & opts ) {
	format( fb, "PxVec3(" );
	format( fb, v.x, opts );
	format( fb, ", " );
	format( fb, v.y, opts );
	format( fb, ", " );
	format( fb, v.z, opts );
	format( fb, ")" );
}

static void format( FormatBuffer * fb, const PxQuat & q, const FormatOpts & opts ) {
	format( fb, "PxQuat(" );
	format( fb, q.x, opts );
	format( fb, ", " );
	format( fb, q.y, opts );
	format( fb, ", " );
	format( fb, q.z, opts );
	format( fb, ", " );
	format( fb, q.w, opts );
	format( fb, ")" );
}

static void format( FormatBuffer * fb, const PxTransform & t, const FormatOpts & opts ) {
	format( fb, "PxTransform(" );
	format( fb, t.p, opts );
	format( fb, ", " );
	format( fb, t.q, opts );
	format( fb, ")" );
}

static void format( FormatBuffer * fb, const EulerDegrees3 & a, const FormatOpts & opts ) {
	format( fb, "EulerDegrees3(" );
	format( fb, a.pitch, opts );
	format( fb, ", " );
	format( fb, a.yaw, opts );
	format( fb, ", " );
	format( fb, a.roll, opts );
	format( fb, ")" );
}

EulerDegrees3 QuaternionToEulerAngles( Quaternion q ) {
	EulerDegrees3 euler;

	// roll (x-axis rotation)
	float sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
	euler.roll = Degrees( atan2f(sinr_cosp, cosr_cosp) );

	// pitch (y-axis rotation)
	float sinp = 2 * (q.w * q.y - q.z * q.x);
	if (fabsf(sinp) >= 1)
		euler.pitch = Degrees( copysignf(PI / 2, sinp) ); // use 90 degrees if out of range
	else
		euler.pitch = Degrees( asinf(sinp) );

	// yaw (z-axis rotation)
	float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
	euler.yaw = Degrees( atan2f(siny_cosp, cosy_cosp) );

	return euler;
}

// static PxRigidDynamic * dumpy1;
// static PxRigidDynamic * dumpy2;
// static PxRevoluteJoint * dumpyjoint;

// static void AddDumpyModel() {
// 	PxCapsuleGeometry capsule = PxCapsuleGeometry( 1.0f, 4.0f );
//
// 	PxTransform transform1 = PxTransform( PxVec3( -4.0f, 0.0f, 0.0f ), PxQuat( PxIdentity ) );
// 	PxTransform transform2 = PxTransform( PxVec3( 4.0f, 0.0f, 0.0f ), PxQuat( PxIdentity ) );
//
// 	dumpy1 = PxCreateDynamic( *physx_physics, transform1, capsule, *physx_default_material, 10.0f );
// 	dumpy2 = PxCreateDynamic( *physx_physics, transform2, capsule, *physx_default_material, 10.0f );
//
// 	PxVec3 K = PxVec3( 0.0f, 0.0f, 1.0f );
// 	dumpyjoint = PxRevoluteJointCreate( *physx_physics,
// 		dumpy1, PxTransform( -transform1.p, PxQuat( PxPi / 2.0f, K ) ),
// 		dumpy2, PxTransform( -transform2.p, PxQuat( PxPi / 2.0f, K ) )
// 	);
// 	dumpyjoint->setLimit( PxJointAngularLimitPair( 0.0f, PxPi ) );
// 	dumpyjoint->setRevoluteJointFlag( PxRevoluteJointFlag::eLIMIT_ENABLED, true );
//
// 	physx_scene->addActor( *dumpy1 );
// 	physx_scene->addActor( *dumpy2 );
//
// 	PxRigidDynamic * gg = PxCreateDynamic( *physx_physics, PxTransform( PxVec3( 4.0f, 0.0f, -10.0f ), PxQuat( PxIdentity ) ), PxSphereGeometry( 8.0f ), *physx_default_material, 10.0f );
// 	physx_scene->addActor( *gg );
// }

void DrawRagdollEditor() {
	const Model * padpork = FindModel( "players/padpork/model" );

	TempAllocator temp = cls.frame_arena.temp();

	bool simulate_clicked = false;

	ImGui::PushFont( cls.console_font );
	ImGui::BeginChild( "Ragdoll editor", ImVec2( 300, 0 ) );
	{
		if( ImGui::TreeNodeEx( "Joints", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoAutoOpenOnLog ) ) {
			JointPicker( padpork, "Pelvis", &editor_ragdoll_config.pelvis );
			JointPicker( padpork, "Spine", &editor_ragdoll_config.spine );
			JointPicker( padpork, "Neck", &editor_ragdoll_config.neck );
			JointPicker( padpork, "Left shoulder", &editor_ragdoll_config.left_shoulder );
			JointPicker( padpork, "Left elbow", &editor_ragdoll_config.left_elbow );
			JointPicker( padpork, "Left wrist", &editor_ragdoll_config.left_wrist );
			JointPicker( padpork, "Right shoulder", &editor_ragdoll_config.right_shoulder );
			JointPicker( padpork, "Right elbow", &editor_ragdoll_config.right_elbow );
			JointPicker( padpork, "Right wrist", &editor_ragdoll_config.right_wrist );
			JointPicker( padpork, "Left hip", &editor_ragdoll_config.left_hip );
			JointPicker( padpork, "Left knee", &editor_ragdoll_config.left_knee );
			JointPicker( padpork, "Left ankle", &editor_ragdoll_config.left_ankle );
			JointPicker( padpork, "Right hip", &editor_ragdoll_config.right_hip );
			JointPicker( padpork, "Right knee", &editor_ragdoll_config.right_knee );
			JointPicker( padpork, "Right ankle", &editor_ragdoll_config.right_ankle );

			ImGui::TreePop();
		}

		if( ImGui::TreeNodeEx( "Bones", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoAutoOpenOnLog ) ) {
			ImGui::SliderFloat( "Lower back radius", &editor_ragdoll_config.lower_back_radius, 0, 10 );
			ImGui::SliderFloat( "Upper back radius", &editor_ragdoll_config.upper_back_radius, 0, 10 );
			ImGui::SliderFloat( "Head radius", &editor_ragdoll_config.head_radius, 0, 10 );
			ImGui::SliderFloat( "Left upper arm radius", &editor_ragdoll_config.left_upper_arm_radius, 0, 10 );
			ImGui::SliderFloat( "Left forearm radius", &editor_ragdoll_config.left_forearm_radius, 0, 10 );
			ImGui::SliderFloat( "Right upper arm radius", &editor_ragdoll_config.right_upper_arm_radius, 0, 10 );
			ImGui::SliderFloat( "Right forearm radius", &editor_ragdoll_config.right_forearm_radius, 0, 10 );
			ImGui::SliderFloat( "Left thigh radius", &editor_ragdoll_config.left_thigh_radius, 0, 10 );
			ImGui::SliderFloat( "Left lower leg radius", &editor_ragdoll_config.left_lower_leg_radius, 0, 10 );
			ImGui::SliderFloat( "Left foot radius", &editor_ragdoll_config.left_foot_radius, 0, 10 );
			ImGui::SliderFloat( "Right thigh radius", &editor_ragdoll_config.right_thigh_radius, 0, 10 );
			ImGui::SliderFloat( "Right lower leg radius", &editor_ragdoll_config.right_lower_leg_radius, 0, 10 );
			ImGui::SliderFloat( "Right foot radius", &editor_ragdoll_config.right_foot_radius, 0, 10 );

			ImGui::TreePop();
		}

		ImGui::SliderFloat( "t", &editor_t, 5, 10 );

		ImGui::Checkbox( "Draw model", &editor_draw_model );

		simulate_clicked = ImGui::Checkbox( "Simulate", &editor_simulate );
	}

	RendererSetView( Vec3( 50, -50, 40 ), EulerDegrees3( 20, 135, 0 ), 90 );

	MatrixPalettes pose;

	if( !editor_simulate ) {
		if( simulate_clicked ) {
			FREE( sys_allocator, editor_initial_local_pose.ptr );
			ShutdownPhysics();
		}

		Span< TRS > sample = SampleAnimation( &temp, padpork, editor_t );
		pose = ComputeMatrixPalettes( &temp, padpork, sample );
	}
	else {
		if( simulate_clicked ) {
			editor_initial_local_pose = SampleAnimation( sys_allocator, padpork, editor_t );
			pose = ComputeMatrixPalettes( &temp, padpork, editor_initial_local_pose );

			Com_Printf( "blah %s\n", temp( "{}", QuaternionToEulerAngles( editor_initial_local_pose[ editor_ragdoll_config.right_elbow ].rotation ) ) );

			InitPhysicsForRagdollEditor();
			editor_ragdoll = AddRagdoll( padpork, editor_ragdoll_config, pose );

			// AddDumpyModel();
		}

		PxTransform transform1 = PxTransform( PxVec3( -4.0f, 0.0f, 0.0f ), PxQuat( PxIdentity ) );
		PxTransform transform2 = PxTransform( PxVec3( 4.0f, 0.0f, 0.0f ), PxQuat( PxIdentity ) );

		// dumpy1->setGlobalPose( transform1 );
		// dumpy2->setGlobalPose( transform2 );

		UpdatePhysicsCommon( cls.frametime / 5000.0f );

		// todo: print joint transform

		// Com_Printf( "%s\n", temp( "{}", editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().q ) );
		// Com_Printf( "%s\n", temp( "{}", QuaternionToEulerAngles( PhysxToDE( editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().q ) ) ) );
                //
		// // TODO: copy physics to render pose
		// editor_initial_local_pose[ editor_ragdoll_config.right_elbow ].rotation = PhysxToDE( editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().q );
		// editor_initial_local_pose[ editor_ragdoll_config.left_knee ].rotation = PhysxToDE( editor_ragdoll.joints[ Joint_LeftKnee ]->getRelativeTransform().q );
                //
		// ImGui::TextWrapped( "p.x = %f", editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().p.x );
		// ImGui::TextWrapped( "p.y = %f", editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().p.y );
		// ImGui::TextWrapped( "p.z = %f", editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().p.z );
		// ImGui::TextWrapped( "q.x = %f", editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().q.x );
		// ImGui::TextWrapped( "q.y = %f", editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().q.y );
		// ImGui::TextWrapped( "q.z = %f", editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().q.z );
		// ImGui::TextWrapped( "q.w = %f", editor_ragdoll.joints[ Joint_RightElbow ]->getRelativeTransform().q.w );
                //
		// pose = ComputeMatrixPalettes( &temp, padpork, editor_initial_local_pose );
	}

	/*
	if( editor_draw_model ) {
		RGB8 rgb = TEAM_COLORS[ 0 ];
		Vec4 color = Vec4( rgb.r / 255.0f, rgb.g / 255.0f, rgb.b / 255.0f, 1.0f );
		float outline_height = 0.42f;

		DrawModel( padpork, Mat4::Identity(), color, pose );
		DrawOutlinedModel( padpork, Mat4::Identity(), vec4_black, outline_height, pose );
	}

	DrawBone( padpork, pose, editor_ragdoll_config.pelvis, editor_ragdoll_config.spine, editor_ragdoll_config.lower_back_radius );
	DrawBone( padpork, pose, editor_ragdoll_config.spine, editor_ragdoll_config.neck, editor_ragdoll_config.upper_back_radius );
	// TODO: head

	DrawBone( padpork, pose, editor_ragdoll_config.left_shoulder, editor_ragdoll_config.left_elbow, editor_ragdoll_config.left_upper_arm_radius );
	DrawBone( padpork, pose, editor_ragdoll_config.left_elbow, editor_ragdoll_config.left_wrist, editor_ragdoll_config.left_forearm_radius );

	DrawBone( padpork, pose, editor_ragdoll_config.right_shoulder, editor_ragdoll_config.right_elbow, editor_ragdoll_config.right_upper_arm_radius );
	DrawBone( padpork, pose, editor_ragdoll_config.right_elbow, editor_ragdoll_config.right_wrist, editor_ragdoll_config.right_forearm_radius );

	DrawBone( padpork, pose, editor_ragdoll_config.left_hip, editor_ragdoll_config.left_knee, editor_ragdoll_config.left_thigh_radius );
	DrawBone( padpork, pose, editor_ragdoll_config.left_knee, editor_ragdoll_config.left_ankle, editor_ragdoll_config.left_lower_leg_radius );
	// TODO: left foot

	DrawBone( padpork, pose, editor_ragdoll_config.right_hip, editor_ragdoll_config.right_knee, editor_ragdoll_config.right_thigh_radius );
	DrawBone( padpork, pose, editor_ragdoll_config.right_knee, editor_ragdoll_config.right_ankle, editor_ragdoll_config.right_lower_leg_radius );
	// TODO: right foot
	*/

	ImGui::EndChild();
	ImGui::PopFont();
}
