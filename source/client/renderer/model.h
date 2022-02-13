#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

enum InterpolationMode {
	InterpolationMode_Step,
	InterpolationMode_Linear,
	// InterpolationMode_CubicSpline,
};

struct DrawModelConfig {
	struct DrawModel {
		bool enabled;
		bool view_weapon;
	} draw_model;

	struct DrawShadows {
		bool enabled;
	} draw_shadows;

	struct DrawOutlines {
		bool enabled;
		float outline_height;
		Vec4 outline_color;
	} draw_outlines;

	struct DrawSilhouette {
		bool enabled;
		Vec4 silhouette_color;
	} draw_silhouette;
};

enum ModelVfxType {
	ModelVfxType_Generic,
	ModelVfxType_Vfx,
	ModelVfxType_DynamicLight,
	ModelVfxType_Decal,
};

struct Model {
	struct Primitive {
		const Material * material;
		Mesh mesh;
		u32 first_index;
		u32 num_vertices;
	};

	template< typename T >
	struct AnimationChannel {
		T * samples;
		float * times;
		u32 num_samples;
		InterpolationMode interpolation;
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

	Mesh mesh;

	Mat4 transform;
	MinMax3 bounds;

	Primitive * primitives;
	u32 num_primitives;

	Node * nodes;
	u8 num_nodes;

	Joint * skin;
	u8 num_joints;

	u8 camera;

	Animation * animations;
	u8 num_animations;
};

void InitModels();
void HotloadModels();
void ShutdownModels();

const Model * FindModel( StringHash name );
const Model * FindModel( const char * name );
const Model * FindMapModel( StringHash name );

void DeleteModel( Model * model );

bool LoadGLTFModel( Model * model, const char * path );

struct Map;
bool LoadBSPRenderData( const char * filename, Map * map, u64 base_hash, Span< const u8 > data );
void DeleteBSPRenderData( Map * map );

void DrawModelPrimitive( const Model * model, const Model::Primitive * primitive, const PipelineState & pipeline );
void DrawModel( DrawModelConfig config, const Model * model, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes = MatrixPalettes() );

Span< TRS > SampleAnimation( Allocator * a, const Model * model, float t, u8 animation = 0 );
MatrixPalettes ComputeMatrixPalettes( Allocator * a, const Model * model, Span< const TRS > local_poses );
bool FindNodeByName( const Model * model, u32 name, u8 * idx );
void MergeLowerUpperPoses( Span< TRS > lower, Span< const TRS > upper, const Model * model, u8 upper_root_joint );
