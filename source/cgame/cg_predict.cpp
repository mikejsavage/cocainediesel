/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "cgame/cg_local.h"
#include "gameshared/collision.h"

static SpatialHashGrid cg_grid;

// static int cg_numSolids;
// static SyncEntityState *cg_solidList[MAX_PARSE_ENTITIES];

// static int cg_numTriggers;
// static SyncEntityState *cg_triggersList[MAX_PARSE_ENTITIES];
// static bool cg_triggersListTriggered[MAX_PARSE_ENTITIES];

static bool ucmdReady = false;

/*
* CG_PredictedEvent - shared code can fire events during prediction
*/
void CG_PredictedEvent( int entNum, int ev, u64 parm ) {
	if( ev >= PREDICTABLE_EVENTS_MAX ) {
		return;
	}

	// ignore this action if it has already been predicted (the unclosed ucmd has timestamp zero)
	if( ucmdReady && cg.predictingTimeStamp > cg.predictedEventTimes[ev] ) {
		cg.predictedEventTimes[ev] = cg.predictingTimeStamp;
		CG_EntityEvent( &cg_entities[entNum].current, ev, parm, true );
	}
}

void CG_PredictedFireWeapon( int entNum, u64 parm ) {
	CG_PredictedEvent( entNum, EV_FIREWEAPON, parm );
}

void CG_PredictedAltFireWeapon( int entNum, u64 parm ) {
	CG_PredictedEvent( entNum, EV_ALTFIREWEAPON, parm );
}

void CG_PredictedUseGadget( int entNum, GadgetType gadget, u64 parm, bool dead ) {
}

void CG_CheckPredictionError() {
	if( !cg.view.playerPrediction ) {
		return;
	}

	// calculate the last UserCommand we sent that the server has processed
	int frame = cg.frame.ucmdExecuted % ARRAY_COUNT( cg.predictedOrigins );

	// compare what the server returned with what we had predicted it to be
	Vec3 origin = cg.predictedOrigins[frame];

	if( cg.predictedGroundEntity != -1 ) {
		const SyncEntityState * ent = &cg_entities[ cg.predictedGroundEntity ].current;
		if( ent->linearMovement ) {
			Vec3 move;
			GS_LinearMovementDelta( ent, cg.oldFrame.serverTime, cg.frame.serverTime, &move );
			origin = cg.predictedOrigins[frame] + move;
		}
	}

	Vec3 delta_vec = cg.frame.playerState.pmove.origin - origin;
	int delta[ 3 ];
	delta[ 0 ] = delta_vec.x;
	delta[ 1 ] = delta_vec.y;
	delta[ 2 ] = delta_vec.z;

	// save the prediction error for interpolation
	if( Abs( delta[0] ) > 128 || Abs( delta[1] ) > 128 || Abs( delta[2] ) > 128 ) {
		if( cg_showMiss->integer ) {
			Com_GGPrint( "prediction miss on {}: {}", cg.frame.serverFrame, Abs( delta[0] ) + Abs( delta[1] ) + Abs( delta[2] ) );
		}
		cg.predictionError = Vec3( 0.0f );          // a teleport or something
	} else {
		if( cg_showMiss->integer && ( delta[0] || delta[1] || delta[2] ) ) {
			Com_GGPrint( "prediction miss on {}: {}", cg.frame.serverFrame, Abs( delta[0] ) + Abs( delta[1] ) + Abs( delta[2] ) );
		}
		cg.predictedOrigins[frame] = cg.frame.playerState.pmove.origin;
		cg.predictionError = Vec3( delta[ 0 ], delta[ 1 ], delta[ 2 ] ); // save for error interpolation
	}
}

void CG_BuildSolidList() {
	// TODO: idk, this is ugly

	ClearSpatialHashGrid( &cg_grid );

	for( int i = 0; i < cg.frame.numEntities; i++ ) {
		const SyncEntityState * ent = &cg.frame.parsedEntities[ i ];

		if( ent->number == 0 )
			continue;

		if( ISEVENTENTITY( ent ) )
			continue;

		MinMax3 bounds = EntityBounds( ClientCollisionModelStorage(), ent );
		if( bounds.mins == MinMax3::Empty().mins && bounds.maxs == MinMax3::Empty().maxs )
			continue;

		switch( ent->type ) {
			// the following entities can never be solid
			case ET_GHOST:
			case ET_ROCKET:
			case ET_GRENADE:
			case ET_ARBULLET:
			case ET_BUBBLE:
			case ET_LASERBEAM:
			case ET_BOMB:
			case ET_BOMB_SITE:
			case ET_LASER:
			case ET_SPIKES:
			case ET_STAKE:
			case ET_BLAST:
				break;

			case ET_JUMPPAD:
			case ET_PAINKILLER_JUMPPAD:
				LinkEntity( &cg_grid, ClientCollisionModelStorage(), ent, i );
				break;

			default:
				LinkEntity( &cg_grid, ClientCollisionModelStorage(), ent, i );
				break;
		}
	}
}

static bool CG_ClipEntityContact( Vec3 origin, MinMax3 bounds, int entNum ) {
	const centity_t * cent = &cg_entities[ entNum ];

	Ray ray = MakeRayStartEnd( origin, origin );

	Shape shape = { };
	shape.type = ShapeType_AABB;
	shape.aabb = ToCenterExtents( bounds );

	SyncEntityState interpolated = cent->prev;
	interpolated.origin = cent->interpolated.origin;
	interpolated.scale = cent->interpolated.scale;

	trace_t trace = TraceVsEnt( ClientCollisionModelStorage(), ray, shape, &interpolated, SolidMask_Everything );
	return trace.GotNowhere();
}

void CG_Predict_TouchTriggers( pmove_t *pm, Vec3 previous_origin ) {
	// fixme: more accurate check for being able to touch or not
	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	// TODO: triggers

	// for( int i = 0; i < cg_numTriggers; i++ ) {
	// 	const SyncEntityState * state = cg_triggersList[i];

	// 	if( state->type == ET_JUMPPAD || state->type == ET_PAINKILLER_JUMPPAD ) {
	// 		if( !cg_triggersListTriggered[i] ) {
	// 			if( CG_ClipEntityContact( pm->playerState->pmove.origin, pm->mins, pm->maxs, state->number ) ) {
	// 				GS_TouchPushTrigger( &client_gs, pm->playerState, state );
	// 				cg_triggersListTriggered[i] = true;
	// 			}
	// 		}
	// 	}
	// }
}

void CG_Trace( trace_t * tr, Vec3 start, MinMax3 bounds, Vec3 end, int ignore, SolidBits solid_mask ) {
	TracyZoneScoped;

	Ray ray = MakeRayStartEnd( start, end );

	if( solid_mask == Solid_NotSolid ) {
		*tr = MakeMissedTrace( ray );
		return;
	}

	Shape shape;
	if( bounds.mins == bounds.maxs ) {
		Assert( bounds.mins == Vec3( 0.0f ) );
		shape.type = ShapeType_Ray;
	}
	else {
		shape.type = ShapeType_AABB;
		shape.aabb = ToCenterExtents( bounds );
	}

	MinMax3 ray_bounds = Union( Union( MinMax3::Empty(), ray.origin ), ray.origin + ray.direction * ray.length );
	MinMax3 broadphase_bounds = MinkowskiSum( ray_bounds, shape );

	*tr = MakeMissedTrace( ray );

	int touchlist[ 1024 ];
	size_t num = TraverseSpatialHashGrid( &cg_grid, broadphase_bounds, touchlist, SolidMask_AnySolid );

	for( size_t i = 0; i < num; i++ ) {
		SyncEntityState * touch = &cg.frame.parsedEntities[ touchlist[ i ] ];
		
		if( touch->number == ignore )
			continue;
		if( touch->type == ET_PLAYER && touch->team == cg_entities[ ignore ].current.team )
			continue;

		trace_t trace = TraceVsEnt( ClientCollisionModelStorage(), ray, shape, touch, solid_mask );
		if( trace.fraction < tr->fraction ) {
			*tr = trace;
		}
	}
}

static float predictedSteps[CMD_BACKUP]; // for step smoothing
static void CG_PredictAddStep( int virtualtime, int predictiontime, float stepSize ) {

	float oldStep;
	int delta;

	// check for stepping up before a previous step is completed
	delta = cls.realtime - cg.predictedStepTime;
	if( delta < PREDICTED_STEP_TIME ) {
		oldStep = cg.predictedStep * ( (float)( PREDICTED_STEP_TIME - delta ) / (float)PREDICTED_STEP_TIME );
	} else {
		oldStep = 0;
	}

	cg.predictedStep = oldStep + stepSize;
	cg.predictedStepTime = cls.realtime - ( predictiontime - virtualtime );
}

static void CG_PredictSmoothSteps() {
	int64_t outgoing;
	int64_t frame;
	UserCommand cmd;
	int i;
	int virtualtime = 0, predictiontime = 0;

	cg.predictedStepTime = 0;
	cg.predictedStep = 0;

	CL_GetCurrentState( NULL, &outgoing, NULL );

	for( i = outgoing; (outgoing - i) < CMD_BACKUP && predictiontime < PREDICTED_STEP_TIME; i-- ) {
		frame = i % ARRAY_COUNT( predictedSteps );
		CL_GetUserCmd( frame, &cmd );
		predictiontime += cmd.msec;
	}

	// run frames
	while( ++i <= outgoing ) {
		frame = i % ARRAY_COUNT( predictedSteps );
		CL_GetUserCmd( frame, &cmd );
		virtualtime += cmd.msec;

		if( predictedSteps[frame] ) {
			CG_PredictAddStep( virtualtime, predictiontime, predictedSteps[frame] );
		}
	}
}

/*
* CG_PredictMovement
*
* Sets cg.predictedVelocty, cg.predictedOrigin and cg.predictedAngles
*/
void CG_PredictMovement() {
	TracyZoneScoped;

	int64_t ucmdHead;
	CL_GetCurrentState( NULL, &ucmdHead, NULL );
	int64_t ucmdExecuted = cg.frame.ucmdExecuted;

	if( ucmdHead - cg.predictFrom >= CMD_BACKUP ) {
		cg.predictFrom = 0;
	}

	if( cg.predictFrom > 0 ) {
		ucmdExecuted = cg.predictFrom;
		cg.predictedPlayerState = cg.predictFromPlayerState;
		cg_entities[cg.frame.playerState.POVnum].current = cg.predictFromEntityState;
	} else {
		cg.predictedPlayerState = cg.frame.playerState; // start from the final position
	}

	cg.predictedPlayerState.POVnum = cgs.playerNum + 1;

	// if we are too far out of date, just freeze
	if( ucmdHead - ucmdExecuted >= ARRAY_COUNT( predictedSteps ) ) {
		if( cg_showMiss->integer ) {
			Com_Printf( "exceeded CMD_BACKUP\n" );
		}

		cg.predictingTimeStamp = cl.serverTime;
		return;
	}

	// copy current state to pmove
	pmove_t pm = { };
	pm.playerState = &cg.predictedPlayerState;
	pm.scale = cg_entities[cg.frame.playerState.POVnum].interpolated.scale;

	// clear the triggered toggles for this prediction round
	// memset( &cg_triggersListTriggered, 0, sizeof( cg_triggersListTriggered ) );

	// run frames
	while( ++ucmdExecuted <= ucmdHead ) {
		int64_t frame = ucmdExecuted % ARRAY_COUNT( predictedSteps );
		CL_GetUserCmd( frame, &pm.cmd );

		ucmdReady = ( pm.cmd.serverTimeStamp != 0 );
		if( ucmdReady ) {
			cg.predictingTimeStamp = pm.cmd.serverTimeStamp;
		}

		Pmove( &client_gs, &pm );

		// copy for stair smoothing
		predictedSteps[frame] = pm.step;

		if( ucmdReady ) { // hmm fixme: the wip command may not be run enough time to get proper key presses
			UpdateWeapons( &client_gs, &cg.predictedPlayerState, pm.cmd, 0 );
		}

		// save for debug checking
		cg.predictedOrigins[frame] = cg.predictedPlayerState.pmove.origin; // store for prediction error checks

		// backup the last predicted ucmd which has a timestamp (it's closed)
		if( ucmdExecuted == ucmdHead - 1 ) {
			if( ucmdExecuted != cg.predictFrom ) {
				cg.predictFrom = ucmdExecuted;
				cg.predictFromPlayerState = cg.predictedPlayerState;
				cg.predictFromEntityState = cg_entities[cg.frame.playerState.POVnum].current;
			}
		}
	}

	cg.predictedGroundEntity = pm.groundentity;

	// compensate for ground entity movement
	if( pm.groundentity != -1 ) {
		const SyncEntityState * ent = &cg_entities[pm.groundentity].current;
		if( ent->linearMovement ) {
			Vec3 move;
			s64 serverTime = client_gs.gameState.paused ? cg.frame.serverTime : cl.serverTime + cgs.extrapolationTime;
			GS_LinearMovementDelta( ent, cg.frame.serverTime, serverTime, &move );
			cg.predictedPlayerState.pmove.origin = cg.predictedPlayerState.pmove.origin + move;
		}
	}

	CG_PredictSmoothSteps();
}
