#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

enum GLTFInterpolationMode {
	GLTFInterpolationMode_Step,
	GLTFInterpolationMode_Linear,
	// GLTFInterpolationMode_CubicSpline,
};

enum ModelVfxType {
	ModelVfxType_None,
	ModelVfxType_Vfx,
	ModelVfxType_DynamicLight,
	ModelVfxType_Decal,
};

struct GLTFRenderData {
	template< typename T >
	struct AnimationSample {
		T value;
		float time;
	};

	template< typename T >
	struct AnimationChannel {
		Span< AnimationSample< T > > samples;
		GLTFInterpolationMode interpolation;
	};

	struct Animation {
		StringHash name;
		float duration;
	};

	struct NodeAnimation {
		AnimationChannel< Quaternion > rotations;
		AnimationChannel< Vec3 > translations;
		AnimationChannel< float > scales;
	};

	struct VfxNode {
		StringHash name;
		Vec4 color;
	};

	struct DynamicLightNode {
		Vec3 color;
		float intensity;
	};

	struct DecalNode {
		StringHash name;
		Vec4 color;
		float radius;
		float angle;
	};

	struct Node {
		StringHash name;

		Mat3x4 global_transform;
		Transform local_transform;

		StringHash material;
		Mesh mesh;

		u8 parent;
		u8 first_child;
		u8 sibling;

		Span< NodeAnimation > animations;
		bool skinned;

		ModelVfxType vfx_type;
		union {
			VfxNode vfx_node;
			DynamicLightNode dlight_node;
			DecalNode decal_node;
		};
	};

	struct Joint {
		Mat3x4 joint_to_bind;
		u8 node_idx;
	};

	Span< char > name;

	Mat3x4 transform;
	MinMax3 bounds;
	Optional< u8 > camera_node;

	Span< Node > nodes;
	Span< Joint > skin;
	Span< Animation > animations;
};

struct cgltf_data;
bool NewGLTFRenderData( GLTFRenderData * render_data, cgltf_data * gltf, Span< const char > path );
void DeleteGLTFRenderData( GLTFRenderData * render_data );

const GLTFRenderData * FindGLTFRenderData( StringHash name );

void DrawGLTFModel( const DrawModelConfig & config, const GLTFRenderData * render_data, const Mat3x4 & transform, const Vec4 & color, MatrixPalettes palettes = MatrixPalettes() );

bool FindNodeByName( const GLTFRenderData * model, StringHash name, u8 * idx );
bool FindAnimationByName( const GLTFRenderData * model, StringHash name, u8 * idx );

Span< Transform > SampleAnimation( Allocator * a, const GLTFRenderData * model, float t, u8 animation = 0 );
void MergeLowerUpperPoses( Span< Transform > lower, Span< const Transform > upper, const GLTFRenderData * model, u8 upper_root_joint );
MatrixPalettes ComputeMatrixPalettes( Allocator * a, const GLTFRenderData * model, Span< const Transform > local_poses );

void InitGLTFInstancing();
void ShutdownGLTFInstancing();
