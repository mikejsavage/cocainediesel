/*
Copyright (C) 2008 German Garcia
Copyright (C) 2011 Chasseur de bots

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../qas_local.h"
#include "addon_vec3.h"

// CLASS: Vec3
void objectVec3_DefaultConstructor( asvec3_t *self ) {
	self->v = Vec3( 0.0f );
}

void objectVec3_Constructor3F( float x, float y, float z, asvec3_t *self ) {
	self->v = Vec3( x, y, z );
}

void objectVec3_Constructor1F( float v, asvec3_t *self ) {
	self->v = Vec3( v );
}

void objectVec3_CopyConstructor( asvec3_t *other, asvec3_t *self ) {
	self->v = other->v;
}

static asvec3_t *objectVec3_AssignBehaviour( asvec3_t *other, asvec3_t *self ) {
	self->v = other->v;
	return self;
}

static asvec3_t *objectVec3_AssignBehaviourD( float other, asvec3_t *self ) {
	self->v = Vec3( other, other, other );
	return self;
}

static asvec3_t *objectVec3_AssignBehaviourI( int other, asvec3_t *self ) {
	self->v = Vec3( other, other, other );
	return self;
}

static asvec3_t *objectVec3_AddAssignBehaviour( asvec3_t *other, asvec3_t *self ) {
	self->v += other->v;
	return self;
}

static asvec3_t *objectVec3_SubAssignBehaviour( asvec3_t *other, asvec3_t *self ) {
	self->v -= other->v;
	return self;
}

static asvec3_t *objectVec3_XORAssignBehaviour( asvec3_t *other, asvec3_t *self ) {
	self->v = Cross( self->v, other->v );
	return self;
}

static asvec3_t *objectVec3_MulAssignBehaviourI( int other, asvec3_t *self ) {
	self->v *= other;
	return self;
}

static asvec3_t *objectVec3_MulAssignBehaviourD( float other, asvec3_t *self ) {
	self->v *= other;
	return self;
}

static asvec3_t objectVec3_AddBehaviour( asvec3_t *first, asvec3_t *second ) {
	asvec3_t vec;

	vec.v = first->v + second->v;
	return vec;
}

static asvec3_t objectVec3_SubtractBehaviour( asvec3_t *first, asvec3_t *second ) {
	asvec3_t vec;

	vec.v = first->v - second->v;
	return vec;
}

static float objectVec3_MultiplyBehaviour( asvec3_t *first, asvec3_t *second ) {
	return Dot( first->v, second->v );
}

static asvec3_t objectVec3_MultiplyBehaviourVD( asvec3_t *first, float second ) {
	asvec3_t vec;

	vec.v = first->v * second;
	return vec;
}

static asvec3_t objectVec3_MultiplyBehaviourDV( float first, asvec3_t *second ) {
	return objectVec3_MultiplyBehaviourVD( second, first );
}

static asvec3_t objectVec3_MultiplyBehaviourVI( asvec3_t *first, int second ) {
	asvec3_t vec;

	vec.v = first->v * second;
	return vec;
}

static asvec3_t objectVec3_MultiplyBehaviourIV( int first, asvec3_t *second ) {
	return objectVec3_MultiplyBehaviourVI( second, first );
}

static asvec3_t objectVec3_XORBehaviour( asvec3_t *first, asvec3_t *second ) {
	asvec3_t vec;

	vec.v = Cross( first->v, second->v );
	return vec;
}

static bool objectVec3_EqualBehaviour( asvec3_t *first, asvec3_t *second ) {
	return first->v == second->v;
}

static void objectVec3_Set( float x, float y, float z, asvec3_t *vec ) {
	vec->v = Vec3( x, y, z );
}

static float objectVec3_Length( const asvec3_t *vec ) {
	return Length( vec->v );
}

static float objectVec3_Normalize( asvec3_t *vec ) {
	float len = Length( vec->v );
	vec->v = Normalize( vec->v );
	return len;
}

static float objectVec3_Distance( asvec3_t *other, asvec3_t *self ) {
	return Length( other->v - self->v );
}

static void objectVec3_AngleVectors( asvec3_t *f, asvec3_t *r, asvec3_t *u, asvec3_t *self ) {
	AngleVectors( self->v, &f->v, &r->v, &u->v );
}

static asvec3_t objectVec3_VecToAngles( asvec3_t *self ) {
	asvec3_t angles;

	angles.v = VecToAngles( self->v );
	return angles;
}

static float *objectVec3_Index( unsigned index, asvec3_t *self ) {
	if( index > 2 ) {
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) {
			ctx->SetException( "Index out of bounds" );
		}
		return NULL;
	}
	return &self->v[index];
}

void PreRegisterVec3Addon( asIScriptEngine *engine ) {
	int r;

	// register the vector type
	r = engine->RegisterObjectType( "Vec3", sizeof( asvec3_t ), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}

void RegisterVec3Addon( asIScriptEngine *engine ) {
	int r;

	// register object behaviours
	r = engine->RegisterObjectBehaviour( "Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( objectVec3_DefaultConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec3", asBEHAVE_CONSTRUCT, "void f(float x, float y, float z)", asFUNCTION( objectVec3_Constructor3F ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec3", asBEHAVE_CONSTRUCT, "void f(float v)", asFUNCTION( objectVec3_Constructor1F ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour( "Vec3", asBEHAVE_CONSTRUCT, "void f(const Vec3 &in)", asFUNCTION( objectVec3_CopyConstructor ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	// register object methods

	// assignments
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opAssign(Vec3 &in)", asFUNCTION( objectVec3_AssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opAssign(int)", asFUNCTION( objectVec3_AssignBehaviourI ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opAssign(float)", asFUNCTION( objectVec3_AssignBehaviourD ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opAddAssign(Vec3 &in)", asFUNCTION( objectVec3_AddAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opSubAssign(Vec3 &in)", asFUNCTION( objectVec3_SubAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opXorAssign(Vec3 &in)", asFUNCTION( objectVec3_XORAssignBehaviour ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opMulAssign(int)", asFUNCTION( objectVec3_MulAssignBehaviourI ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 &opMulAssign(float)", asFUNCTION( objectVec3_MulAssignBehaviourD ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opAdd(Vec3 &in) const", asFUNCTION( objectVec3_AddBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opSub(Vec3 &in) const", asFUNCTION( objectVec3_SubtractBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float opMul(Vec3 &in) const", asFUNCTION( objectVec3_MultiplyBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opMul(float) const", asFUNCTION( objectVec3_MultiplyBehaviourVD ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opMul_r(float) const", asFUNCTION( objectVec3_MultiplyBehaviourDV ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opMul(int) const", asFUNCTION( objectVec3_MultiplyBehaviourVI ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opMul_r(int) const", asFUNCTION( objectVec3_MultiplyBehaviourIV ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec3", "Vec3 opXor(const Vec3 &in) const", asFUNCTION( objectVec3_XORBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );

	// == !=
	r = engine->RegisterObjectMethod( "Vec3", "bool opEquals(const Vec3 &in) const", asFUNCTION( objectVec3_EqualBehaviour ), asCALL_CDECL_OBJFIRST ); assert( r >= 0 );

	r = engine->RegisterObjectMethod( "Vec3", "void set(float x, float y, float z)", asFUNCTION( objectVec3_Set ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float length() const", asFUNCTION( objectVec3_Length ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float normalize()", asFUNCTION( objectVec3_Normalize ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float distance(const Vec3 &in) const", asFUNCTION( objectVec3_Distance ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "void angleVectors(Vec3 &out, Vec3 &out, Vec3 &out) const", asFUNCTION( objectVec3_AngleVectors ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "Vec3 toAngles() const", asFUNCTION( objectVec3_VecToAngles ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "float &opIndex(uint)", asFUNCTION( objectVec3_Index ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );
	r = engine->RegisterObjectMethod( "Vec3", "const float &opIndex(uint) const", asFUNCTION( objectVec3_Index ), asCALL_CDECL_OBJLAST ); assert( r >= 0 );

	// properties
	r = engine->RegisterObjectProperty( "Vec3", "float x", asOFFSET( asvec3_t, v.x ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Vec3", "float y", asOFFSET( asvec3_t, v.y ) ); assert( r >= 0 );
	r = engine->RegisterObjectProperty( "Vec3", "float z", asOFFSET( asvec3_t, v.z ) ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}
