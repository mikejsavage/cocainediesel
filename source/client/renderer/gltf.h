#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

enum InterpolationMode {
	InterpolationMode_Step,
	InterpolationMode_Linear,
	// InterpolationMode_CubicSpline,
};

enum ModelVfxType {
	ModelVfxType_Generic,
	ModelVfxType_Vfx,
	ModelVfxType_DynamicLight,
	ModelVfxType_Decal,
};

struct GLTFRenderData {
	struct Primitive {
		StringHash material;
		Mesh mesh;
	};

	template< typename T >
	struct AnimationSample {
		T sample;
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
		u32 name;

		Mat4 global_transform;
		TRS local_transform;
		u8 primitive;

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

	Mat4 transform;
	MinMax3 bounds;
	u8 camera_node;

	Span< Primitive > primitives;
	Span< Node > nodes;
	Span< Joint > skin;
	Span< StringHash > animations;
};

struct cgltf_data;
bool NewGLTFRenderData( GLTFRenderData * render_data, const cgltf_data * gltf, const char * path );
void DeleteGLTFRenderData( GLTFRenderData * render_data );

Span< TRS > SampleAnimation( Allocator * a, const GLTFRenderData * model, float t, u8 animation = 0 );
MatrixPalettes ComputeMatrixPalettes( Allocator * a, const GLTFRenderData * model, Span< const TRS > local_poses );
bool FindNodeByName( const GLTFRenderData * model, u32 name, u8 * idx );
void MergeLowerUpperPoses( Span< TRS > lower, Span< const TRS > upper, const GLTFRenderData * model, u8 upper_root_joint );
