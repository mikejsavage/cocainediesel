#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

enum InterpolationMode {
	InterpolationMode_Step,
	InterpolationMode_Linear,
	// InterpolationMode_CubicSpline,
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

	struct Node {
		u32 name;

		Mat4 global_transform;
		TRS local_transform;
		u8 primitive;

		u8 parent;
		u8 first_child;
		u8 sibling;

		AnimationChannel< Quaternion > rotations;
		AnimationChannel< Vec3 > translations;
		AnimationChannel< float > scales;
		bool skinned;
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
void DrawModel( const Model * model, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes = MatrixPalettes() );
void DrawViewWeapon( const Model * model, const Mat4 & transform );
void DrawOutlinedViewWeapon( const Model * model, const Mat4 & transform, const Vec4 & color, float outline_height );
void DrawModelSilhouette( const Model * model, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes = MatrixPalettes() );
void DrawOutlinedModel( const Model * model, const Mat4 & transform, const Vec4 & color, float outline_height, MatrixPalettes palettes = MatrixPalettes() );
void DrawModelShadow( const Model * model, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes = MatrixPalettes() );

Span< TRS > SampleAnimation( Allocator * a, const Model * model, float t );
MatrixPalettes ComputeMatrixPalettes( Allocator * a, const Model * model, Span< const TRS > local_poses );
bool FindNodeByName( const Model * model, u32 name, u8 * idx );
void MergeLowerUpperPoses( Span< TRS > lower, Span< const TRS > upper, const Model * model, u8 upper_root_joint );
