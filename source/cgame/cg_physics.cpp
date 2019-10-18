#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"
#include "client/client.h"
#include "qcommon/cm_local.h"
#include "cgame/cg_local.h"

#include "bullet/btBulletCollisionCommon.h"
#include "bullet/btBulletDynamicsCommon.h"
#include "bullet/LinearMath/btGeometryUtil.h"

btDiscreteDynamicsWorld* dynamicsWorld;

btSphereShape * ball0;
btSphereShape * ball1;
btRigidBody * body0;
btRigidBody * body1;

void InitPhysics() {
	ZoneScoped;

	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
	btCollisionDispatcher* dispatcher = new btCollisionDispatcher( collisionConfiguration );
	btBroadphaseInterface* overlappingPairCache = new btDbvtBroadphase();
	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;
	dynamicsWorld = new btDiscreteDynamicsWorld( dispatcher, overlappingPairCache, solver, collisionConfiguration );

	dynamicsWorld->setGravity( btVector3( 0, 0, -GRAVITY ) );

	{
		ball0 = new btSphereShape( 64 );

		btTransform startTransform;
		startTransform.setIdentity();

		btScalar mass(1.f);

		btVector3 localInertia(0, 0, 0);
		ball0->calculateLocalInertia(mass, localInertia);

		startTransform.setOrigin(btVector3(-215, 400, 1000));

		btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, ball0, localInertia);
		body0 = new btRigidBody(rbInfo);

		body0->setRestitution( 1.0f );

		// body0->setDamping(0.05f, 0.85f);
		// body0->setDeactivationTime(0.8f);
		// body0->setSleepingThresholds(1.6f, 2.5f);

		dynamicsWorld->addRigidBody(body0);
	}

	{
		ball1 = new btSphereShape( 64 );

		btTransform startTransform;
		startTransform.setIdentity();

		btScalar mass(1.f);

		btVector3 localInertia(0, 0, 0);
		ball1->calculateLocalInertia(mass, localInertia);

		startTransform.setOrigin(btVector3(-215, 600, 1000));

		btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, ball1, localInertia);
		body1 = new btRigidBody(rbInfo);

		body1->setRestitution( 1.0f );

		// body1->setDamping(0.05f, 0.85f);
		// body1->setDeactivationTime(0.8f);
		// body1->setSleepingThresholds(1.6f, 2.5f);

		dynamicsWorld->addRigidBody(body1);
	}

	{
		btTransform localA, localB;
		localA.setIdentity(); localB.setIdentity();

		localA.setOrigin( btVector3( 0, 0, 0 ) );
		localB.setOrigin( btVector3( 0, 200, 0 ) );

		btGeneric6DofConstraint * joint = new btGeneric6DofConstraint( *body0, *body1, localA, localB, true );

		joint->setAngularLowerLimit(btVector3(-SIMD_EPSILON,-SIMD_EPSILON,-SIMD_EPSILON));
		joint->setAngularUpperLimit(btVector3(SIMD_EPSILON,SIMD_EPSILON,SIMD_EPSILON));

		joint->setLinearLowerLimit(btVector3(-SIMD_EPSILON, -SIMD_EPSILON,-SIMD_EPSILON));
		joint->setLinearUpperLimit(btVector3(SIMD_EPSILON, SIMD_EPSILON,SIMD_EPSILON));

		dynamicsWorld->addConstraint( joint, true );
	}

	{
		const cmodel_t * model = &cl.cms->map_cmodels[ 0 ];
		for( int i = 0; i < model->nummarkbrushes; i++ ) {
			const cbrush_t * brush = &model->brushes[ i ];
			btAlignedObjectArray< btVector3 > planes;

			for( int j = 0; j < brush->numsides; j++ ) {
				cplane_t plane = brush->brushsides[ j ].plane;
				// plane.normal/plane.dist
				btVector3 bt_plane( plane.normal[ 0 ], plane.normal[ 1 ], plane.normal[ 2 ] );
				bt_plane[ 3 ] = -plane.dist;
				planes.push_back( bt_plane );
			}

			btAlignedObjectArray<btVector3> vertices;
			btGeometryUtil::getVerticesFromPlaneEquations(planes, vertices);

			float mass = 0.f;
			btTransform startTransform;
			startTransform.setIdentity();

			btCollisionShape* shape = new btConvexHullShape(&(vertices[0].getX()), vertices.size());

			btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, shape);
			btRigidBody* body = new btRigidBody(rbInfo);

			body->setRestitution( 1.0f );

			dynamicsWorld->addRigidBody(body);
		}
	}
}

void UpdatePhysics() {
	float dt = cg.frameTime / 1000.0f;
	dynamicsWorld->stepSimulation( dt, 10 );

	{
		int j = 0;
		btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[j];
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
		btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[j];
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
