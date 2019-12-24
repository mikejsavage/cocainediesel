#pragma once

#include "qcommon/types.h"
#include "cgame/ref.h"
#include "client/renderer/backend.h"

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
	};

	struct Joint {
		Mat4 joint_to_bind;
		u8 parent;
		u8 next;

		// TODO: remove this with additive animations
		u32 name;
		u8 first_child;
		u8 sibling;

		AnimationChannel< Quaternion > rotations;
		AnimationChannel< Vec3 > translations;
		AnimationChannel< float > scales;
	};

	Mesh mesh;

	Mat4 transform;
	MinMax3 bounds;

	Primitive * primitives;
	u32 num_primitives;

	Joint * joints;
	u8 num_joints;
	u8 root_joint;
};

struct MapMetadata {
	u64 base_hash;

	u32 num_models;

	float fog_strength;

	Span< u8 > pvs;
	u32 cluster_size;
};

void InitModels();
void ShutdownModels();

const Model * FindModel( StringHash name );
const Model * FindModel( const char * name );

const MapMetadata * FindMapMetadata( StringHash name );
const MapMetadata * FindMapMetadata( const char * name );

Model * NewModel( u64 hash );

bool LoadGLTFModel( Model * model, const char * path );
bool LoadBSPMap( MapMetadata * map, const char * path );

void DrawModelPrimitive( const Model * model, const Model::Primitive * primitive, const PipelineState & pipeline );
void DrawModel( const Model * model, const Mat4 & transform, const Vec4 & color, Span< const Mat4 > skinning_matrices = Span< const Mat4 >() );
void DrawModelSilhouette( const Model * model, const Mat4 & transform, const Vec4 & color, Span< const Mat4 > skinning_matrices = Span< const Mat4 >() );
void DrawOutlinedModel( const Model * model, const Mat4 & transform, const Vec4 & color, float outline_height, Span< const Mat4 > skinning_matrices );

MinMax3 ModelBounds( const Model * model );

Span< TRS > SampleAnimation( Allocator * a, const Model * model, float t );
MatrixPalettes ComputeMatrixPalettes( Allocator * a, const Model * model, Span< TRS > local_poses );
bool FindJointByName( const Model * model, u32 name, u8 * joint_idx );
void MergeLowerUpperPoses( Span< TRS > lower, Span< const TRS > upper, const Model * model, u8 upper_root_joint );
