#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

enum InterpolationMode {
	InterpolationMode_Step,
	InterpolationMode_Linear,
	// InterpolationMode_CubicSpline,
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
		InterpolationMode interpolation;
	};

	struct Animation {
		StringHash name;
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
		Vec4 color;
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

		Mat4 global_transform;
		TRS local_transform;

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
		Mat4 joint_to_bind;
		u8 node_idx;
	};

	char * name;

	Mat4 transform;
	MinMax3 bounds;
	Optional< u8 > camera_node;

	Span< Node > nodes;
	Span< Joint > skin;
	Span< StringHash > animations;
};

struct cgltf_data;
bool NewGLTFRenderData( GLTFRenderData * render_data, cgltf_data * gltf, const char * path );
void DeleteGLTFRenderData( GLTFRenderData * render_data );

const GLTFRenderData * FindGLTFRenderData( StringHash name );

void DrawGLTFModel( const DrawModelConfig & config, const GLTFRenderData * render_data, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes = MatrixPalettes() );

bool FindNodeByName( const GLTFRenderData * model, StringHash name, u8 * idx );
bool FindAnimationByName( const GLTFRenderData * model, StringHash name, u8 * idx );

Span< TRS > SampleAnimation( Allocator * a, const GLTFRenderData * model, float t, u8 animation = 0 );
void MergeLowerUpperPoses( Span< TRS > lower, Span< const TRS > upper, const GLTFRenderData * model, u8 upper_root_joint );
MatrixPalettes ComputeMatrixPalettes( Allocator * a, const GLTFRenderData * model, Span< const TRS > local_poses );

void InitGLTFInstancing();
void ShutdownGLTFInstancing();
