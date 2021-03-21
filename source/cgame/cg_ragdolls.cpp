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

struct Ragdoll {
	struct Bone {
		PxCapsuleGeometry capsule;
		PxRigidDynamic * actor;
	};

	Span< PxJoint * > joints;
	Span< Bone > bones;
	PxAggregate * aggregate;
};

static Ragdoll editor_ragdoll;
static Span< TRS > editor_initial_local_pose;

static PxVec3 ToPhysx( Vec3 v ) {
	return PxVec3( v.x, v.y, v.z );
}

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

static Quaternion FromPhysx( PxQuat q ) {
	return Quaternion( q.x, q.y, q.z, q.w );
}

static PxQuat ToPhysx( Quaternion q ) {
	return PxQuat( q.x, q.y, q.z, q.w );
}

static TRS FromPhysx( PxTransform t ) {
	TRS trs;
	trs.translation = Vec3( t.p.x, t.p.y, t.p.z );
	trs.rotation = Quaternion( t.q.x, t.q.y, t.q.z, t.q.w );
	trs.scale = 1.0f;
	return trs;
}

void format( FormatBuffer * fb, const TRS & trs, const FormatOpts & opts ) {
	format( fb, "TRS(" );
	format( fb, trs.translation, opts );
	format( fb, ", " );
	format( fb, trs.rotation, opts );
	format( fb, ", " );
	format( fb, trs.scale, opts );
	format( fb, ")" );
}

static Quaternion Matrix2Quat( Mat4 m)
{
	Quaternion q;
	q.w = sqrtf( Max2( 0.0f, 1.0f + m.col0.x + m.col1.y + m.col2.z ) ) / 2.0f;
	q.x = sqrtf( Max2( 0.0f, 1.0f + m.col0.x - m.col1.y - m.col2.z ) ) / 2.0f;
	q.y = sqrtf( Max2( 0.0f, 1.0f - m.col0.x + m.col1.y - m.col2.z ) ) / 2.0f;
	q.z = sqrtf( Max2( 0.0f, 1.0f - m.col0.x - m.col1.y + m.col2.z ) ) / 2.0f;
	q.x = copysignf( q.x, m.row2().y - m.row1().z );
	q.y = copysignf( q.y, m.row0().z - m.row2().x );
	q.z = copysignf( q.z, m.row1().x - m.row0().y );
	return q;
}

static void CreateBone( Ragdoll * ragdoll, u8 bone, const Model * model, MatrixPalettes pose, u8 j0, u8 j1, float radius ) {
	Vec3 p0 = ( model->transform * pose.node_transforms[ j0 ] ).col3.xyz();
	Vec3 p1 = ( model->transform * pose.node_transforms[ j1 ] ).col3.xyz();

	// TODO: radius
	float bone_length = Length( p1 - p0 );
	ragdoll->bones[ bone ].capsule = PxCapsuleGeometry( 0.5f, bone_length * 0.5f );

	// PxTransform transform = PxTransformFromSegment( ToPhysx( p0 ), ToPhysx( p1 ), &ragdoll->bones[ bone ].capsule.halfHeight );

	Mat4 t = Mat4Translation( Vec3( 0, 0.0f, 0.0f ) );
	Mat4 t2 = model->transform * pose.node_transforms[ j0 ] * t;
	PxTransform transform = PxTransform( ToPhysx( t2 ) ).getNormalized();

	// PxQuat r = ToPhysx( Matrix2Quat( pose.node_transforms[ j1 ] ) );
	// PxTransform mt = PxTransform( ToPhysx( model->transform ) );
	// // PxTransform t = PxTransform( mt.transform( ToPhysx( p0 ) ) ) * PxTransform( r ) * PxTransform( PxVec3( bone_length * 0.5f, 0.0f, 0.0f ) );
	// PxTransform t = PxTransform( ToPhysx( p1 ) ) * ( mt * PxTransform( r ) ) * PxTransform( PxVec3( bone_length * 0.5f, 0.0f, 0.0f ) );
	// PxTransform transform = t.getNormalized();

	PxTransform shape_offset = PxTransform( bone_length * 0.5f, 0, 0 );

	PxRigidDynamic * actor = PxCreateDynamic( *physx_physics, transform, ragdoll->bones[ bone ].capsule, *physx_default_material, 1.0f, shape_offset );

	char * buf = ( char * ) malloc( 128 );
	snprintf( buf, 128, "%s -> %s", model->nodes[ j0 ].name, model->nodes[ j1 ].name );
	actor->setName( buf );
	actor->setLinearDamping( 0.5f );
	actor->setAngularDamping( 0.5f );

	ragdoll->aggregate->addActor( *actor );

	ragdoll->bones[ bone ].actor = actor;
}

static Mat3 InvertOrthonormal( Mat3 m ) {
	return Mat3( m.row0(), m.row1(), m.row2() );
}

static Ragdoll AddRagdoll( const Model * model, RagdollConfig config, MatrixPalettes pose ) {
	Ragdoll ragdoll;

	ragdoll.aggregate = physx_physics->createAggregate( model->num_nodes, false );
	ragdoll.bones = ALLOC_SPAN( sys_allocator, Ragdoll::Bone, model->num_nodes );
	ragdoll.joints = ALLOC_SPAN( sys_allocator, PxJoint *, model->num_joints );

	for( u8 node = 3; node < model->num_nodes; node++ ) {
		u8 parent = model->nodes[ node ].parent;
		if( parent == U8_MAX ) continue;
		Com_GGPrint( "bone {}: {} -> {}", node, model->nodes[ parent ].name, model->nodes[ node ].name );
		CreateBone( &ragdoll, node, model, pose, parent, node, 0.0f );
	}

	for( u8 joint = 2; joint < model->num_joints; joint++ ) {
		u8 b = model->skin[ joint ].node_idx;
		u8 a = model->nodes[ b ].parent;
		Com_GGPrint( "joint {}: {} -> {}", joint, model->nodes[ a ].name, model->nodes[ b ].name );

		Ragdoll::Bone * bone_a = &ragdoll.bones[ a ];
		Ragdoll::Bone * bone_b = &ragdoll.bones[ b ];

		PxQuat q_a = bone_a->actor->getGlobalPose().q;
		PxQuat q_b = bone_b->actor->getGlobalPose().q;

		// PxFixedJoint * fixedjoint = PxFixedJointCreate( *physx_physics,
		// 	bone_a->actor, PxTransform( bone_a->capsule.halfHeight, 0, 0, q_a.getConjugate() * q_b ),
		// 	bone_b->actor, PxTransform( -bone_b->capsule.halfHeight, 0, 0 )
		// );
		// ragdoll.joints[ joint ] = fixedjoint;

		PxSphericalJoint * spherejoint = PxSphericalJointCreate( *physx_physics,
			bone_a->actor, PxTransform( bone_a->capsule.halfHeight * 2.0f, 0, 0, q_a.getConjugate() * q_b ),
			bone_b->actor, PxTransform( 0, 0, 0 )
		);
		spherejoint->setName( model->nodes[ joint ].name );
		spherejoint->setLimitCone( PxJointLimitCone( PxPi * 0.1, PxPi * 0.1, 0.01f ) );
		spherejoint->setSphericalJointFlag( PxSphericalJointFlag::eLIMIT_ENABLED, true );
		ragdoll.joints[ joint ] = spherejoint;
	}

	physx_scene->addAggregate( *ragdoll.aggregate );

	return ragdoll;
}

void InitRagdollEditor() {
}

void ResetRagdollEditor() {
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

static Quaternion Conjugate( Quaternion q ) {
	return Quaternion( -q.x, -q.y, -q.z, q.w );
}

static void DoSHit( const Model * model, u8 joint, u8 node ) {
	PxTransform gg = editor_ragdoll.joints[ joint ]->getRelativeTransform();
	PxTransform gg2 = editor_ragdoll.joints[ joint ]->getLocalPose( PxJointActorIndex::eACTOR1 );
	Quaternion before = editor_initial_local_pose[ node ].rotation;
	Com_GGPrint( "before: {}", before );
	editor_initial_local_pose[ node ].rotation = FromPhysx( gg2.q.getConjugate() * gg.q ) * before;
	Com_GGPrint( "after: {}", editor_initial_local_pose[ node ].rotation );
}

void DrawRagdollEditor() {
	const Model * padpork = FindModel( "players/padpork/model" );

	TempAllocator temp = cls.frame_arena.temp();

	bool simulate_clicked = false;

	ImGui::PushFont( cls.console_font );
	ImGui::BeginChild( "Ragdoll editor", ImVec2( 300, 0 ) );
	{
		ImGui::SliderFloat( "t", &editor_t, 5, 10 );

		ImGui::Checkbox( "Draw model", &editor_draw_model );

		simulate_clicked = ImGui::Checkbox( "Simulate", &editor_simulate );
	}

	RendererSetView( Vec3( 50, -50, 40 ), EulerDegrees3( 20, 135, 0 ), 90 );

	MatrixPalettes pose;
	Mat4 root_transform = Mat4::Identity();

	if( !editor_simulate ) {
		if( simulate_clicked ) {
			FREE( sys_allocator, editor_initial_local_pose.ptr );
			ShutdownPhysics();
		}

		Span< TRS > sample = SampleAnimation( &temp, padpork, editor_t );
		pose = ComputeMatrixPalettes( &temp, padpork, sample );
	}
	else {
		editor_initial_local_pose = SampleAnimation( sys_allocator, padpork, editor_t );
		if( simulate_clicked ) {
			pose = ComputeMatrixPalettes( &temp, padpork, editor_initial_local_pose );

			InitPhysicsForRagdollEditor();
			editor_ragdoll = AddRagdoll( padpork, editor_ragdoll_config, pose );
		}

		UpdatePhysicsCommon( cls.frametime / 5000.0f );

		for( u8 joint = 2; joint < padpork->num_joints; joint++ ) {
			u8 node = padpork->skin[ joint ].node_idx;
			DoSHit( padpork, joint, node );
		}

		pose = ComputeMatrixPalettes( &temp, padpork, editor_initial_local_pose );
	}

	if( editor_draw_model ) {
		RGB8 rgb = TEAM_COLORS[ 0 ];
		Vec4 color = Vec4( rgb.r / 255.0f, rgb.g / 255.0f, rgb.b / 255.0f, 1.0f );
		float outline_height = 0.42f;

		DrawModel( padpork, root_transform, color, pose );
		DrawOutlinedModel( padpork, root_transform, vec4_black, outline_height, pose );
	}

	ImGui::EndChild();
	ImGui::PopFont();
}
