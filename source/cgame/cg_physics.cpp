#include <new>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"
#include "client/client.h"
#include "qcommon/cm_local.h"
#include "cgame/cg_local.h"

#include "bullet/btBulletCollisionCommon.h"
#include "bullet/btBulletDynamicsCommon.h"
#include "bullet/LinearMath/btGeometryUtil.h"

#define NEW( a, T, ... ) new ( ALLOC( a, T ) ) T( __VA_ARGS__ )
#define DELETE( a, T, p ) p->~T(); FREE( a, p )

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

void InitPhysics() {
	ZoneScoped;

	collision_configuration = NEW( sys_allocator, btDefaultCollisionConfiguration );
	collision_dispatcher = NEW( sys_allocator, btCollisionDispatcher, collision_configuration );
	broadphase_pass = NEW( sys_allocator, btDbvtBroadphase );
	constraint_solver = NEW( sys_allocator, btSequentialImpulseConstraintSolver );
	dynamics_world = NEW( sys_allocator, btDiscreteDynamicsWorld, collision_dispatcher, broadphase_pass, constraint_solver, collision_configuration );

	dynamics_world->setGravity( btVector3( 0, 0, -GRAVITY ) );
	dynamics_world->setDebugDrawer( &debug_renderer );
	debug_renderer.setDebugMode( 0
		// | btIDebugDraw::DBG_DrawWireframe
		// | btIDebugDraw::DBG_DrawAabb
		| btIDebugDraw::DBG_DrawContactPoints
		| btIDebugDraw::DBG_DrawConstraints
		| btIDebugDraw::DBG_DrawConstraintLimits
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

	{
		const char * suffix = "*0";
		u64 hash = Hash64( suffix, strlen( suffix ), cgs.map->base_hash );
		const Model * model = FindModel( StringHash( hash ) );

		for( u32 i = 0; i < model->num_collision_shapes; i++ ) {
			float mass = 0.0f;
			btTransform startTransform;
			startTransform.setIdentity();

			btDefaultMotionState * myMotionState = NEW( sys_allocator, btDefaultMotionState, startTransform );
			btRigidBody::btRigidBodyConstructionInfo info( mass, myMotionState, model->collision_shapes[ i ] );
			btRigidBody* body = NEW( sys_allocator, btRigidBody, info );

			body->setRestitution( 1.0f );

			dynamics_world->addRigidBody( body );
		}
	}
}

void ShutdownPhysics() {
	while( dynamics_world->getNumConstraints() > 0 ) {
		btTypedConstraint * constraint = dynamics_world->getConstraint( dynamics_world->getNumConstraints() - 1 );
		dynamics_world->removeConstraint( constraint );
		DELETE( sys_allocator, btTypedConstraint, constraint );
	}

	while( dynamics_world->getNumCollisionObjects() > 0 ) {
		btCollisionObject * obj = dynamics_world->getCollisionObjectArray()[ dynamics_world->getNumCollisionObjects() - 1 ];
		btRigidBody * body = btRigidBody::upcast( obj );
		if( body != NULL && body->getMotionState() != NULL ) {
			DELETE( sys_allocator, btMotionState, body->getMotionState() );
		}
		dynamics_world->removeCollisionObject( obj );
		DELETE( sys_allocator, btCollisionObject, obj );
	}

	DELETE( sys_allocator, btSphereShape, ball0 );
	DELETE( sys_allocator, btSphereShape, ball1 );

	DELETE( sys_allocator, btDiscreteDynamicsWorld, dynamics_world );
	DELETE( sys_allocator, btSequentialImpulseConstraintSolver, constraint_solver );
	DELETE( sys_allocator, btBroadphaseInterface, broadphase_pass );
	DELETE( sys_allocator, btCollisionDispatcher, collision_dispatcher );
	DELETE( sys_allocator, btDefaultCollisionConfiguration, collision_configuration );
}

void UpdatePhysics() {
	ZoneScoped;

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
}
