#include <new>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"
#include "client/client.h"
#include "qcommon/cm_local.h"
#include "cgame/cg_local.h"

#include "physx/PxPhysicsAPI.h"

#define NEW( a, T, ... ) new ( ALLOC( a, T ) ) T( __VA_ARGS__ )
#define DELETE( a, T, p ) p->~T(); FREE( a, p )

#if 0
struct BulletDebugRenderer : public btIDebugDraw {
	int mode;
	DefaultColors default_colors;

	virtual DefaultColors getDefaultColors() const {
		return default_colors;
	}

	virtual void setDefaultColors( const DefaultColors & colors ) {
		default_colors = colors;
	}

	virtual void drawLine( const btVector3 & start, const btVector3 & end, const btVector3 & color ) {
		Vec3 positions[] = {
			Vec3( start.x(), start.y(), start.z() ),
			Vec3( end.x(), end.y(), end.z() ),
		};

		Vec4 colors[] = {
			Vec4( color.x(), color.y(), color.z(), 1.0f ),
			Vec4( color.x(), color.y(), color.z(), 1.0f ),
		};

		u16 indices[] = { 0, 1 };

		MeshConfig config;
		config.positions = NewVertexBuffer( positions, sizeof( positions ) );
		config.colors = NewVertexBuffer( colors, sizeof( colors ) );
		config.indices = NewIndexBuffer( indices, sizeof( indices ) );
		config.num_vertices = 2;
		config.primitive_type = PrimitiveType_Lines;

		Mesh mesh = NewMesh( config );

		PipelineState pipeline;
		pipeline.pass = frame_static.transparent_pass;
		pipeline.shader = &shaders.standard_vertexcolors;
		pipeline.write_depth = false;
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );
		pipeline.set_uniform( "u_Material", frame_static.identity_material_uniforms );
		pipeline.set_texture( "u_BaseTexture", FindTexture( "$whiteimage" ) );

		DrawMesh( mesh, pipeline );

		DeferDeleteMesh( mesh );
	}

	virtual void drawContactPoint( const btVector3 & p, const btVector3 & normal, btScalar distance, int lifetime, const btVector3 & color ) {
		drawLine( p, p + normal * distance, color );
	}

	virtual void reportErrorWarning( const char * str ) {
		Com_Printf( "%s\n", str );
	}

	virtual void draw3dText( const btVector3 & location, const char * str ) {
		Com_Printf( "%s\n", str );
	}

	virtual void setDebugMode( int debugMode ) {
		mode = debugMode;
	}

	virtual int getDebugMode() const {
		return mode;
	}
};

static BulletDebugRenderer debug_renderer;

static btDefaultCollisionConfiguration * collision_configuration;
static btCollisionDispatcher * collision_dispatcher;
static btBroadphaseInterface * broadphase_pass;
static btSequentialImpulseConstraintSolver * constraint_solver;
static btDiscreteDynamicsWorld * dynamics_world;

btSphereShape * ball0;
btSphereShape * ball1;
btRigidBody * body0;
btRigidBody * body1;

#endif

static physx::PxDefaultAllocator allocator;
static physx::PxDefaultErrorCallback errorCallback;

static physx::PxFoundation * mFoundation;
physx::PxPhysics * physx_physics;
physx::PxCooking * physx_cooking;
static physx::PxScene * mScene;
physx::PxMaterial * physx_default_material;


/*
enum {
	BODYPART_PELVIS = 0,
	BODYPART_SPINE,
	BODYPART_HEAD,

	BODYPART_LEFT_UPPER_LEG,
	BODYPART_LEFT_LOWER_LEG,

	BODYPART_RIGHT_UPPER_LEG,
	BODYPART_RIGHT_LOWER_LEG,

	BODYPART_LEFT_UPPER_ARM,
	BODYPART_LEFT_LOWER_ARM,

	BODYPART_RIGHT_UPPER_ARM,
	BODYPART_RIGHT_LOWER_ARM,

	BODYPART_COUNT
};

enum {
	JOINT_PELVIS_SPINE = 0,
	JOINT_SPINE_HEAD,

	JOINT_LEFT_HIP,
	JOINT_LEFT_KNEE,

	JOINT_RIGHT_HIP,
	JOINT_RIGHT_KNEE,

	JOINT_LEFT_SHOULDER,
	JOINT_LEFT_ELBOW,

	JOINT_RIGHT_SHOULDER,
	JOINT_RIGHT_ELBOW,

	JOINT_COUNT
};

struct Bone {
	btCollisionShape * collision_shape;
	btRigidBody * rigid_body;
};

static Bone bones[ BODYPART_COUNT ];
static btTypedConstraint * joints[ JOINT_COUNT ];

static btRigidBody* createRigidBody(btScalar mass, const btTransform& startTransform, btCollisionShape* shape)
{
	btVector3 localInertia(0, 0, 0);
	shape->calculateLocalInertia(mass, localInertia);

	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, shape, localInertia);
	btRigidBody* body = new btRigidBody(rbInfo);

	dynamics_world->addRigidBody(body);

	return body;
}

static void AddRagdoll() {
	float scale = 60;

	// Setup the geometry
	bones[BODYPART_PELVIS].collision_shape = new btCapsuleShapeZ(btScalar(0.15) * scale, btScalar(0.20) * scale);
	bones[BODYPART_SPINE].collision_shape = new btCapsuleShapeZ(btScalar(0.15) * scale, btScalar(0.28) * scale);
	bones[BODYPART_HEAD].collision_shape = new btCapsuleShapeZ(btScalar(0.10) * scale, btScalar(0.05) * scale);
	bones[BODYPART_LEFT_UPPER_LEG].collision_shape = new btCapsuleShapeZ(btScalar(0.07) * scale, btScalar(0.45) * scale);
	bones[BODYPART_LEFT_LOWER_LEG].collision_shape = new btCapsuleShapeZ(btScalar(0.05) * scale, btScalar(0.37) * scale);
	bones[BODYPART_RIGHT_UPPER_LEG].collision_shape = new btCapsuleShapeZ(btScalar(0.07) * scale, btScalar(0.45) * scale);
	bones[BODYPART_RIGHT_LOWER_LEG].collision_shape = new btCapsuleShapeZ(btScalar(0.05) * scale, btScalar(0.37) * scale);
	bones[BODYPART_LEFT_UPPER_ARM].collision_shape = new btCapsuleShapeZ(btScalar(0.05) * scale, btScalar(0.33) * scale);
	bones[BODYPART_LEFT_LOWER_ARM].collision_shape = new btCapsuleShapeZ(btScalar(0.04) * scale, btScalar(0.25) * scale);
	bones[BODYPART_RIGHT_UPPER_ARM].collision_shape = new btCapsuleShapeZ(btScalar(0.05) * scale, btScalar(0.33) * scale);
	bones[BODYPART_RIGHT_LOWER_ARM].collision_shape = new btCapsuleShapeZ(btScalar(0.04) * scale, btScalar(0.25) * scale);

	btVector3 positionOffset = btVector3( -215, -1500, 150 );
	// Setup all the rigid bodies
	btTransform offset;
	offset.setIdentity();
	offset.setOrigin(positionOffset);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(0.), btScalar(0.), btScalar(1.)));
	bones[BODYPART_PELVIS].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_PELVIS].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(0.), btScalar(0.), btScalar(1.2)));
	bones[BODYPART_SPINE].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_SPINE].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(0.), btScalar(0.0), btScalar(1.6)));
	bones[BODYPART_HEAD].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_HEAD].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(-0.18), btScalar(0.), btScalar(0.65)));
	bones[BODYPART_LEFT_UPPER_LEG].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_LEFT_UPPER_LEG].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(-0.18), btScalar(0.), btScalar(0.2)));
	bones[BODYPART_LEFT_LOWER_LEG].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_LEFT_LOWER_LEG].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(0.18), btScalar(0.), btScalar(0.65)));
	bones[BODYPART_RIGHT_UPPER_LEG].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_RIGHT_UPPER_LEG].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(0.18), btScalar(0.), btScalar(0.2)));
	bones[BODYPART_RIGHT_LOWER_LEG].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_RIGHT_LOWER_LEG].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(-0.35), btScalar(0.), btScalar(1.45)));
	transform.getBasis().setEulerZYX(0, 0, M_PI_2);
	bones[BODYPART_LEFT_UPPER_ARM].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_LEFT_UPPER_ARM].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(-0.7), btScalar(0.), btScalar(1.45)));
	transform.getBasis().setEulerZYX(0, 0, M_PI_2);
	bones[BODYPART_LEFT_LOWER_ARM].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_LEFT_LOWER_ARM].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(0.35), btScalar(0.), btScalar(1.45)));
	transform.getBasis().setEulerZYX(0, 0, -M_PI_2);
	bones[BODYPART_RIGHT_UPPER_ARM].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_RIGHT_UPPER_ARM].collision_shape);

	transform.setIdentity();
	transform.setOrigin(scale * btVector3(btScalar(0.7), btScalar(0.), btScalar(1.45)));
	transform.getBasis().setEulerZYX(0, 0, -M_PI_2);
	bones[BODYPART_RIGHT_LOWER_ARM].rigid_body = createRigidBody(btScalar(1.), offset * transform, bones[BODYPART_RIGHT_LOWER_ARM].collision_shape);

	// Setup some damping
	for (int i = 0; i < BODYPART_COUNT; ++i)
	{
		bones[i].rigid_body->setDamping(btScalar(0.05), btScalar(0.85));
		bones[i].rigid_body->setDeactivationTime(btScalar(0.8));
		bones[i].rigid_body->setSleepingThresholds(btScalar(1.6), btScalar(2.5));
	}

	// Now setup the constraints
	btHingeConstraint* hingeC;
	btConeTwistConstraint* coneC;

	btTransform localA, localB;

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.15)));
	localB.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.15)));
	hingeC = new btHingeConstraint(*bones[BODYPART_PELVIS].rigid_body, *bones[BODYPART_SPINE].rigid_body, localA, localB);
	hingeC->setLimit(btScalar(-M_PI_4), btScalar(M_PI_2));
	joints[JOINT_PELVIS_SPINE] = hingeC;
	dynamics_world->addConstraint(joints[JOINT_PELVIS_SPINE], true);

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(0, 0, M_PI_2);
	localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.30)));
	localB.getBasis().setEulerZYX(0, 0, M_PI_2);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.14)));
	coneC = new btConeTwistConstraint(*bones[BODYPART_SPINE].rigid_body, *bones[BODYPART_HEAD].rigid_body, localA, localB);
	coneC->setLimit(M_PI_4, M_PI_4, M_PI_2);
	joints[JOINT_SPINE_HEAD] = coneC;
	dynamics_world->addConstraint(joints[JOINT_SPINE_HEAD], true);

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(0, 0, -M_PI_4 * 5);
	localA.setOrigin(scale * btVector3(btScalar(-0.18), btScalar(0), btScalar(-0.1)));
	localB.getBasis().setEulerZYX(0, 0, -M_PI_4 * 5);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.225)));
	coneC = new btConeTwistConstraint(*bones[BODYPART_PELVIS].rigid_body, *bones[BODYPART_LEFT_UPPER_LEG].rigid_body, localA, localB);
	coneC->setLimit(M_PI_4, M_PI_4, 0);
	joints[JOINT_LEFT_HIP] = coneC;
	dynamics_world->addConstraint(joints[JOINT_LEFT_HIP], true);

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.225)));
	localB.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.185)));
	hingeC = new btHingeConstraint(*bones[BODYPART_LEFT_UPPER_LEG].rigid_body, *bones[BODYPART_LEFT_LOWER_LEG].rigid_body, localA, localB);
	hingeC->setLimit(btScalar(0), btScalar(M_PI_2));
	joints[JOINT_LEFT_KNEE] = hingeC;
	dynamics_world->addConstraint(joints[JOINT_LEFT_KNEE], true);

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(0, 0, M_PI_4);
	localA.setOrigin(scale * btVector3(btScalar(0.18), btScalar(0), btScalar(-0.10)));
	localB.getBasis().setEulerZYX(0, 0, M_PI_4);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.225)));
	coneC = new btConeTwistConstraint(*bones[BODYPART_PELVIS].rigid_body, *bones[BODYPART_RIGHT_UPPER_LEG].rigid_body, localA, localB);
	coneC->setLimit(M_PI_4, M_PI_4, 0);
	joints[JOINT_RIGHT_HIP] = coneC;
	dynamics_world->addConstraint(joints[JOINT_RIGHT_HIP], true);

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.225)));
	localB.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.185)));
	hingeC = new btHingeConstraint(*bones[BODYPART_RIGHT_UPPER_LEG].rigid_body, *bones[BODYPART_RIGHT_LOWER_LEG].rigid_body, localA, localB);
	hingeC->setLimit(btScalar(0), btScalar(M_PI_2));
	joints[JOINT_RIGHT_KNEE] = hingeC;
	dynamics_world->addConstraint(joints[JOINT_RIGHT_KNEE], true);

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(0, 0, M_PI);
	localA.setOrigin(scale * btVector3(btScalar(-0.2), btScalar(0), btScalar(0.15)));
	localB.getBasis().setEulerZYX(0, 0, M_PI_2);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.18)));
	coneC = new btConeTwistConstraint(*bones[BODYPART_SPINE].rigid_body, *bones[BODYPART_LEFT_UPPER_ARM].rigid_body, localA, localB);
	coneC->setLimit(M_PI_2, M_PI_2, 0);
	joints[JOINT_LEFT_SHOULDER] = coneC;
	dynamics_world->addConstraint(joints[JOINT_LEFT_SHOULDER], true);

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.18)));
	localB.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.14)));
	hingeC = new btHingeConstraint(*bones[BODYPART_LEFT_UPPER_ARM].rigid_body, *bones[BODYPART_LEFT_LOWER_ARM].rigid_body, localA, localB);
	hingeC->setLimit(btScalar(-M_PI_2), btScalar(0));
	joints[JOINT_LEFT_ELBOW] = hingeC;
	dynamics_world->addConstraint(joints[JOINT_LEFT_ELBOW], true);

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(0, 0, 0);
	localA.setOrigin(scale * btVector3(btScalar(0.2), btScalar(0), btScalar(0.15)));
	localB.getBasis().setEulerZYX(0, 0, M_PI_2);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.18)));
	coneC = new btConeTwistConstraint(*bones[BODYPART_SPINE].rigid_body, *bones[BODYPART_RIGHT_UPPER_ARM].rigid_body, localA, localB);
	coneC->setLimit(M_PI_2, M_PI_2, 0);
	joints[JOINT_RIGHT_SHOULDER] = coneC;
	dynamics_world->addConstraint(joints[JOINT_RIGHT_SHOULDER], true);

	localA.setIdentity();
	localB.setIdentity();
	localA.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localA.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(0.18)));
	localB.getBasis().setEulerZYX(M_PI_2, 0, 0);
	localB.setOrigin(scale * btVector3(btScalar(0.), btScalar(0), btScalar(-0.14)));
	hingeC = new btHingeConstraint(*bones[BODYPART_RIGHT_UPPER_ARM].rigid_body, *bones[BODYPART_RIGHT_LOWER_ARM].rigid_body, localA, localB);
	hingeC->setLimit(btScalar(-M_PI_2), btScalar(0));
	joints[JOINT_RIGHT_ELBOW] = hingeC;
	dynamics_world->addConstraint(joints[JOINT_RIGHT_ELBOW], true);
}
*/


static int64_t last_reset;
void InitPhysics() {
	last_reset = cg.monotonicTime;
	ZoneScoped;

	mFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorCallback);
	if(!mFoundation)
		return; //fatalError("PxCreateFoundation failed!");

	physx::PxTolerancesScale scale;

	physx_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, scale );
	if(!physx_physics)
		return; //fatalError("PxCreatePhysics failed!");

	physx::PxCookingParams params(scale);
	params.meshWeldTolerance = 0.001f;
	params.meshPreprocessParams = physx::PxMeshPreprocessingFlags(physx::PxMeshPreprocessingFlag::eWELD_VERTICES);
	physx_cooking = PxCreateCooking(PX_PHYSICS_VERSION, *mFoundation, params);
	if(!physx_cooking)
		return; //fatalError("PxCreateCooking failed!");

	physx_default_material = physx_physics->createMaterial(0.5f, 0.5f, 0.1f);
	if(!physx_default_material)
		return; //fatalError("createMaterial failed!");

	physx::PxSceneDesc sceneDesc(physx_physics->getTolerancesScale());
	sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);



	sceneDesc.cpuDispatcher = physx::PxDefaultCpuDispatcherCreate( 2 );
	if(!sceneDesc.cpuDispatcher)
		return; //fatalError("PxDefaultCpuDispatcherCreate failed!");

	sceneDesc.filterShader  = physx::PxDefaultSimulationFilterShader;

	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_PCM;
	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_STABILIZATION;
	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
	sceneDesc.sceneQueryUpdateMode = physx::PxSceneQueryUpdateMode::eBUILD_ENABLED_COMMIT_DISABLED;

	mScene = physx_physics->createScene(sceneDesc);
	if(!mScene)
		return; //fatalError("createScene failed!");


	mScene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f );
	mScene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES,  1.0f);





















#if 0
	collision_configuration = NEW( sys_allocator, btDefaultCollisionConfiguration );
	collision_dispatcher = NEW( sys_allocator, btCollisionDispatcher, collision_configuration );
	broadphase_pass = NEW( sys_allocator, btDbvtBroadphase );
	constraint_solver = NEW( sys_allocator, btSequentialImpulseConstraintSolver );
	dynamics_world = NEW( sys_allocator, btDiscreteDynamicsWorld, collision_dispatcher, broadphase_pass, constraint_solver, collision_configuration );

	dynamics_world->setGravity( btVector3( 0, 0, -GRAVITY / 4 ) );
	dynamics_world->setDebugDrawer( &debug_renderer );
	debug_renderer.setDebugMode( 0
		// | btIDebugDraw::DBG_DrawWireframe
		// | btIDebugDraw::DBG_DrawAabb
		// | btIDebugDraw::DBG_DrawContactPoints
		// | btIDebugDraw::DBG_DrawConstraints
		// | btIDebugDraw::DBG_DrawConstraintLimits
	);

	{
		ball0 = NEW( sys_allocator, btSphereShape, 64 );

		btTransform startTransform;
		startTransform.setIdentity();

		btScalar mass(1.f);

		btVector3 localInertia(0, 0, 0);
		ball0->calculateLocalInertia(mass, localInertia);

		startTransform.setOrigin(btVector3(-215, 400, 1000));

		btDefaultMotionState* myMotionState = NEW( sys_allocator, btDefaultMotionState, startTransform );
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, ball0, localInertia);
		body0 = NEW( sys_allocator, btRigidBody, rbInfo );

		body0->setRestitution( 1.0f );

		// body0->setDamping(0.05f, 0.85f);
		// body0->setDeactivationTime(0.8f);
		// body0->setSleepingThresholds(1.6f, 2.5f);

		dynamics_world->addRigidBody(body0);
	}

	{
		ball1 = NEW( sys_allocator, btSphereShape, 64 );

		btTransform startTransform;
		startTransform.setIdentity();

		btScalar mass(1.f);

		btVector3 localInertia(0, 0, 0);
		ball1->calculateLocalInertia(mass, localInertia);

		startTransform.setOrigin(btVector3(-215, 600, 1000));

		btDefaultMotionState* myMotionState = NEW( sys_allocator, btDefaultMotionState, startTransform );
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, ball1, localInertia);
		body1 = NEW( sys_allocator, btRigidBody, rbInfo );

		body1->setRestitution( 1.0f );

		// body1->setDamping(0.05f, 0.85f);
		// body1->setDeactivationTime(0.8f);
		// body1->setSleepingThresholds(1.6f, 2.5f);

		dynamics_world->addRigidBody(body1);
	}

	{
		btTransform localA, localB;
		localA.setIdentity(); localB.setIdentity();

		localA.setOrigin( btVector3( 0, 0, 0 ) );
		localB.setOrigin( btVector3( 0, 200, 0 ) );

		// btGeneric6DofConstraint * joint = NEW( sys_allocator, btGeneric6DofConstraint, *body0, *body1, localA, localB, true );
                //
		// joint->setAngularLowerLimit(btVector3(-SIMD_EPSILON,-SIMD_EPSILON,-SIMD_EPSILON));
		// joint->setAngularUpperLimit(btVector3(SIMD_EPSILON,SIMD_EPSILON,SIMD_EPSILON));
                //
		// joint->setLinearLowerLimit(btVector3(-SIMD_EPSILON, -SIMD_EPSILON,-SIMD_EPSILON));
		// joint->setLinearUpperLimit(btVector3(SIMD_EPSILON, SIMD_EPSILON,SIMD_EPSILON));

		btSliderConstraint * joint = NEW( sys_allocator, btSliderConstraint, *body0, *body1, localA, localB, true );
		joint->setLowerLinLimit( -100 );
		joint->setUpperLinLimit( 100 );
		joint->setUpperAngLimit( 0 );
		joint->setLowerAngLimit( 0 );

		dynamics_world->addConstraint( joint, true );
	}

	// AddRagdoll();

	{
		const char * suffix = "*0";
		u64 hash = Hash64( suffix, strlen( suffix ), cgs.map->base_hash );
		const Model * model = FindModel( StringHash( hash ) );

		// for( u32 i = 0; i < model->num_collision_shapes; i++ ) {
		// 	float mass = 0.0f;
		// 	btTransform startTransform;
		// 	startTransform.setIdentity();
                //
		// 	btDefaultMotionState * myMotionState = NEW( sys_allocator, btDefaultMotionState, startTransform );
		// 	btRigidBody::btRigidBodyConstructionInfo info( mass, myMotionState, model->collision_shapes[ i ] );
		// 	btRigidBody* body = NEW( sys_allocator, btRigidBody, info );
                //
		// 	body->setRestitution( 1.0f );
                //
		// 	dynamics_world->addRigidBody( body );
		// }
	}
#endif
}

void ShutdownPhysics() {
#if 0
	while( dynamics_world->getNumConstraints() > 0 ) {
		btTypedConstraint * constraint = dynamics_world->getConstraint( dynamics_world->getNumConstraints() - 1 );
		dynamics_world->removeConstraint( constraint );
		// DELETE( sys_allocator, btTypedConstraint, constraint );
	}

	while( dynamics_world->getNumCollisionObjects() > 0 ) {
		btCollisionObject * obj = dynamics_world->getCollisionObjectArray()[ dynamics_world->getNumCollisionObjects() - 1 ];
		btRigidBody * body = btRigidBody::upcast( obj );
		if( body != NULL && body->getMotionState() != NULL ) {
			// DELETE( sys_allocator, btMotionState, body->getMotionState() );
		}
		dynamics_world->removeCollisionObject( obj );
		// DELETE( sys_allocator, btCollisionObject, obj );
	}

	DELETE( sys_allocator, btSphereShape, ball0 );
	DELETE( sys_allocator, btSphereShape, ball1 );

	DELETE( sys_allocator, btDiscreteDynamicsWorld, dynamics_world );
	DELETE( sys_allocator, btSequentialImpulseConstraintSolver, constraint_solver );
	DELETE( sys_allocator, btBroadphaseInterface, broadphase_pass );
	DELETE( sys_allocator, btCollisionDispatcher, collision_dispatcher );
	DELETE( sys_allocator, btDefaultCollisionConfiguration, collision_configuration );
#endif
}

void UpdatePhysics() {
	ZoneScoped;

#if 0
	if( cg.monotonicTime - last_reset > 5000 ) {
		ShutdownPhysics();
		InitPhysics();
		last_reset = cg.monotonicTime;
	}

	float dt = cg.frameTime / 1000.0f;
	dynamics_world->stepSimulation( dt, 10 );

	{
		ZoneScopedN( "Debug draw world" );
		dynamics_world->debugDrawWorld();
	}

	{
		int j = 0;
		btCollisionObject* obj = dynamics_world->getCollisionObjectArray()[j];
		btRigidBody* body = btRigidBody::upcast(obj);
		btTransform trans;
		if (body && body->getMotionState())
		{
			body->getMotionState()->getWorldTransform(trans);
		}
		else
		{
			trans = obj->getWorldTransform();
		}

		const Model * model = FindModel( "models/objects/gibs/gib" );
		Mat4 translation = Mat4Translation( trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ() );
		DrawModel( model, translation * Mat4Scale( 12.8f, 12.8f, 12.8f ), vec4_red );
	}

	{
		int j = 1;
		btCollisionObject* obj = dynamics_world->getCollisionObjectArray()[j];
		btRigidBody* body = btRigidBody::upcast(obj);
		btTransform trans;
		if (body && body->getMotionState())
		{
			body->getMotionState()->getWorldTransform(trans);
		}
		else
		{
			trans = obj->getWorldTransform();
		}

		const Model * model = FindModel( "models/objects/gibs/gib" );
		Mat4 translation = Mat4Translation( trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ() );
		DrawModel( model, translation * Mat4Scale( 12.8f, 12.8f, 12.8f ), vec4_yellow );
	}

	// for( size_t i = 0; i < ARRAY_COUNT( bones ); i++ ) {
	// 	btTransform trans;
	// 	bones[ i ].rigid_body->getMotionState()->getWorldTransform( trans );
        //
	// 	btCapsuleShape * capsule = ( btCapsuleShape * ) bones[ i ].collision_shape;
	// 	debug_renderer.drawCapsule( capsule->getRadius(), capsule->getHalfHeight(), 2, trans, btVector3( 0, 0, 0 ) );
	// }
#endif
}
