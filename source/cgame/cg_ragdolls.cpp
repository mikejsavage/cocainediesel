#include "qcommon/base.h"
#include "client/client.h"
#include "cgame/cg_local.h"

#include "imgui/imgui.h"

static RagdollConfig editor_ragdoll_config;
static float editor_t;
static bool editor_draw_model;

void InitRagdollEditor() {
}

void ResetRagdollEditor() {
	editor_ragdoll_config.pelvis = U8_MAX;
	editor_ragdoll_config.spine = U8_MAX;
	editor_ragdoll_config.neck = U8_MAX;
	editor_ragdoll_config.left_shoulder = U8_MAX;
	editor_ragdoll_config.left_elbow = U8_MAX;
	editor_ragdoll_config.right_shoulder = U8_MAX;
	editor_ragdoll_config.right_elbow = U8_MAX;
	editor_ragdoll_config.left_hip = U8_MAX;
	editor_ragdoll_config.left_knee = U8_MAX;
	editor_ragdoll_config.left_ankle = U8_MAX;
	editor_ragdoll_config.right_hip = U8_MAX;
	editor_ragdoll_config.right_knee = U8_MAX;
	editor_ragdoll_config.right_ankle = U8_MAX;

	editor_t = 5.0f;
	editor_draw_model = true;
}

static Mat4 TransformKToSegment( Vec3 start, Vec3 end ) {
	Vec3 K = Vec3( 0, 0, 1 );
	Vec3 dir = end - start;

	Vec3 axis = Normalize( Cross( K, dir ) );
	float c = Dot( K, dir ) / Length( dir );
	float s = sqrtf( 1.0f - c * c );

	Mat4 rotation = Mat4(
		c + axis.x * axis.x * ( 1.0f - c ),
		axis.x * axis.y * ( 1.0f - c ) - axis.z * s,
		axis.x * axis.z * ( 1.0f - c ) + axis.y * s,
		0.0f,

		axis.y * axis.x * ( 1.0f - c ) + axis.z * s,
		c + axis.y * axis.y * ( 1.0f - c ),
		axis.y * axis.z * ( 1.0f - c ) - axis.x * s,
		0.0f,

		axis.z * axis.x * ( 1.0f - c ) - axis.y * s,
		axis.z * axis.y * ( 1.0f - c ) + axis.x * s,
		c + axis.z * axis.z * ( 1.0f - c ),
		0.0f,

		0.0f, 0.0f, 0.0f, 1.0f
	);

	Mat4 translation = Mat4Translation( start );

	Mat4 scale = Mat4Scale( Length( dir ) );

	return translation * rotation * scale;
}

static void DrawBone( const Model * model, MatrixPalettes matrices, u8 j0, u8 j1 ) {
	PipelineState pipeline;
	pipeline.pass = frame_static.nonworld_opaque_pass;
	pipeline.shader = &shaders.standard;
	pipeline.depth_func = DepthFunc_Disabled;
	pipeline.set_texture( "u_BaseTexture", cls.whiteTexture );
	pipeline.set_uniform( "u_View", frame_static.view_uniforms );
	pipeline.set_uniform( "u_Material", frame_static.identity_material_uniforms );

	const Model * capsule = FindModel( "models/capsule" );
	Vec3 p0 = ( model->transform * matrices.joint_poses[ j0 ].col3 ).xyz();
	Vec3 p1 = ( model->transform * matrices.joint_poses[ j1 ].col3 ).xyz();
	pipeline.set_uniform( "u_Model", UploadModelUniforms( TransformKToSegment( p0, p1 ) * capsule->transform ) );

	DrawModelPrimitive( FindModel( "models/capsule" ), &capsule->primitives[ 0 ], pipeline );
}

static void JointPicker( const Model * model, const char * label, u8 * joint ) {
	if( ImGui::BeginCombo( label, *joint == U8_MAX ? "None" : model->joints[ *joint ].name ) ) {
		if( ImGui::Selectable( "None", *joint == U8_MAX ) )
			*joint = U8_MAX;
		if( *joint == U8_MAX )
			ImGui::SetItemDefaultFocus();

		for( u8 i = 0; i < model->num_joints; i++ ) {
			if( ImGui::Selectable( model->joints[ i ].name, *joint == i ) )
				*joint = i;
			if( *joint == i )
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}
}

void DrawRagdollEditor() {
	const Model * padpork = FindModel( "models/players/padpork" );

	TempAllocator temp = cls.frame_arena.temp();

	ImGui::PushFont( cls.console_font );
	ImGui::BeginChild( "Ragdoll editor", ImVec2( 300, 0 ) );
	{
		if( ImGui::TreeNodeEx( "Joints", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoAutoOpenOnLog ) ) {
			JointPicker( padpork, "Pelvis", &editor_ragdoll_config.pelvis );
			JointPicker( padpork, "Spine", &editor_ragdoll_config.spine );
			JointPicker( padpork, "Neck", &editor_ragdoll_config.neck );
			JointPicker( padpork, "Left shoulder", &editor_ragdoll_config.left_shoulder );
			JointPicker( padpork, "Left elbow", &editor_ragdoll_config.left_elbow );
			JointPicker( padpork, "Right shoulder", &editor_ragdoll_config.right_shoulder );
			JointPicker( padpork, "Right elbow", &editor_ragdoll_config.right_elbow );
			JointPicker( padpork, "Left hip", &editor_ragdoll_config.left_hip );
			JointPicker( padpork, "Left knee", &editor_ragdoll_config.left_knee );
			JointPicker( padpork, "Left ankle", &editor_ragdoll_config.left_ankle );
			JointPicker( padpork, "Right hip", &editor_ragdoll_config.right_hip );
			JointPicker( padpork, "Right knee", &editor_ragdoll_config.right_knee );
			JointPicker( padpork, "Right ankle", &editor_ragdoll_config.right_ankle );

			ImGui::TreePop();
		}

		if( ImGui::TreeNodeEx( "Bones", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoAutoOpenOnLog ) ) {
			ImGui::TreePop();
		}

		ImGui::SliderFloat( "t", &editor_t, 5, 10 );

		ImGui::Checkbox( "Draw model", &editor_draw_model );
	}
	ImGui::EndChild();
	ImGui::PopFont();

	RendererSetView( Vec3( 50, -50, 40 ), EulerDegrees3( 20, 135, 0 ), 90 );

	Span< TRS > sample = SampleAnimation( &temp, padpork, editor_t );
	MatrixPalettes pose = ComputeMatrixPalettes( &temp, padpork, sample );

	if( editor_draw_model ) {
		RGB8 rgb = TEAM_COLORS[ 0 ].rgb;
		Vec4 color = Vec4( rgb.r / 255.0f, rgb.g / 255.0f, rgb.b / 255.0f, 1.0f );
		float outline_height = 0.42f;

		DrawModel( padpork, Mat4::Identity(), color, pose.skinning_matrices );
		DrawOutlinedModel( padpork, Mat4::Identity(), vec4_black, outline_height, pose.skinning_matrices );
	}

	for( u8 i = 0; i < padpork->num_joints; i++ ) {
		const Model::Joint & joint = padpork->joints[ i ];
		if( joint.parent == U8_MAX )
			continue;

		DrawBone( padpork, pose, i, joint.parent );
	}
}
