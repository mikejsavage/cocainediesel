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
#include "qcommon/cmodel.h"

static int cg_numSolids;
static SyncEntityState *cg_solidList[MAX_PARSE_ENTITIES];

static int cg_numTriggers;
static SyncEntityState *cg_triggersList[MAX_PARSE_ENTITIES];
static bool cg_triggersListTriggered[MAX_PARSE_ENTITIES];

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

void CG_PredictedUseGadget( int entNum, GadgetType gadget, u64 parm ) {
}

void CG_CheckPredictionError() {
	int delta[3];
	int frame;

	if( !cg.view.playerPrediction ) {
		return;
	}

	// calculate the last UserCommand we sent that the server has processed
	frame = cg.frame.ucmdExecuted & CMD_MASK;

	// compare what the server returned with what we had predicted it to be
	Vec3 origin = cg.predictedOrigins[frame];

	if( cg.predictedGroundEntity != -1 ) {
		SyncEntityState *ent = &cg_entities[cg.predictedGroundEntity].current;
		const cmodel_t * cmodel = CM_TryFindCModel( CM_Client, ent->model );
		if( cmodel != NULL && ent->linearMovement ) {
			Vec3 move;
			GS_LinearMovementDelta( ent, cg.oldFrame.serverTime, cg.frame.serverTime, &move );
			origin = cg.predictedOrigins[frame] + move;
		}
	}

	Vec3 delta_vec = cg.frame.playerState.pmove.origin - origin;
	delta[ 0 ] = delta_vec.x;
	delta[ 1 ] = delta_vec.y;
	delta[ 2 ] = delta_vec.z;

	// save the prediction error for interpolation
	if( Abs( delta[0] ) > 128 || Abs( delta[1] ) > 128 || Abs( delta[2] ) > 128 ) {
		if( cg_showMiss->integer ) {
			Com_Printf( "prediction miss on %" PRIi64 ": %i\n", cg.frame.serverFrame, Abs( delta[0] ) + Abs( delta[1] ) + Abs( delta[2] ) );
		}
		cg.predictionError = Vec3( 0.0f );          // a teleport or something
	} else {
		if( cg_showMiss->integer && ( delta[0] || delta[1] || delta[2] ) ) {
			Com_Printf( "prediction miss on %" PRIi64" : %i\n", cg.frame.serverFrame, Abs( delta[0] ) + Abs( delta[1] ) + Abs( delta[2] ) );
		}
		cg.predictedOrigins[frame] = cg.frame.playerState.pmove.origin;
		cg.predictionError = Vec3( delta[ 0 ], delta[ 1 ], delta[ 2 ] ); // save for error interpolation
	}
}

void CG_BuildSolidList() {
	cg_numSolids = 0;
	cg_numTriggers = 0;

	for( int i = 0; i < cg.frame.numEntities; i++ ) {
		const SyncEntityState * ent = &cg.frame.parsedEntities[ i ];

		if( ISEVENTENTITY( ent ) )
			continue;

		if( ent->bounds.mins == MinMax3::Empty().mins && ent->bounds.maxs == MinMax3::Empty().maxs )
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
				cg_triggersList[cg_numTriggers++] = &cg_entities[ ent->number ].current;
				break;

			default:
				cg_solidList[cg_numSolids++] = &cg_entities[ ent->number ].current;
				break;
		}
	}
}

static bool CG_ClipEntityContact( Vec3 origin, Vec3 mins, Vec3 maxs, int entNum ) {
	Vec3 entorigin, entangles;
	int64_t serverTime = cg.frame.serverTime;

	// find the cmodel
	const cmodel_t * cmodel = CG_CModelForEntity( entNum );
	if( !cmodel ) {
		return false;
	}

	const centity_t * cent = &cg_entities[entNum];

	// find the origin
	if( !cmodel->builtin ) { // special value for bmodel
		if( cent->current.linearMovement ) {
			GS_LinearMovement( &cent->current, serverTime, &entorigin );
		} else {
			entorigin = cent->current.origin;
		}
		entangles = cent->current.angles;
	} else {
		entorigin = cent->current.origin;
		entangles = Vec3( 0.0f ); // boxes don't rotate
	}

	Vec3 absmins = origin + mins;
	Vec3 absmaxs = origin + maxs;
	trace_t tr;
	CM_TransformedBoxTrace( CM_Client, cl.cms, &tr, Vec3( 0.0f ), Vec3( 0.0f ), absmins, absmaxs, cmodel, MASK_ALL, entorigin, entangles );
	return tr.startsolid == true || tr.allsolid == true;
}

void CG_Predict_TouchTriggers( pmove_t *pm, Vec3 previous_origin ) {
	// fixme: more accurate check for being able to touch or not
	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	for( int i = 0; i < cg_numTriggers; i++ ) {
		const SyncEntityState * state = cg_triggersList[i];

		if( state->type == ET_JUMPPAD || state->type == ET_PAINKILLER_JUMPPAD ) {
			if( !cg_triggersListTriggered[i] ) {
				if( CG_ClipEntityContact( pm->playerState->pmove.origin, pm->mins, pm->maxs, state->number ) ) {
					GS_TouchPushTrigger( &client_gs, pm->playerState, state );
					cg_triggersListTriggered[i] = true;
				}
			}
		}
	}
}

static void CG_ClipMoveToEntities( Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, int ignore, int contentmask, trace_t *tr ) {
	int64_t serverTime = cg.frame.serverTime;

	for( int i = 0; i < cg_numSolids; i++ ) {
		const SyncEntityState * ent = cg_solidList[i];

		if( ent->number == ignore ) {
			continue;
		}

		if( !( contentmask & CONTENTS_CORPSE ) && ent->type == ET_CORPSE ) {
			continue;
		}

		if( ent->type == ET_PLAYER ) {
			int teammask = contentmask & ( CONTENTS_TEAMALPHA | CONTENTS_TEAMBETA );
			if( teammask != 0 ) {
				int team = teammask == CONTENTS_TEAMALPHA ? TEAM_ALPHA : TEAM_BETA;
				if( ent->team != team )
					continue;
			}
		}

		const cmodel_t * cmodel = CG_CModelForEntity( ent->number );
		Vec3 origin, angles;
		if( !cmodel->builtin ) { // special value for bmodel
			if( ent->linearMovement ) {
				GS_LinearMovement( ent, serverTime, &origin );
			} else {
				origin = ent->origin;
			}
			angles = ent->angles;
		} else {
			origin = ent->origin;
			angles = Vec3( 0.0f ); // boxes don't rotate
		}

		trace_t trace;
		CM_TransformedBoxTrace( CM_Client, cl.cms, &trace, start, end, mins, maxs, cmodel, contentmask, origin, angles );
		if( trace.allsolid || trace.fraction < tr->fraction ) {
			trace.ent = ent->number;
			*tr = trace;
		} else if( trace.startsolid ) {
			tr->startsolid = true;
		}

		if( tr->allsolid ) {
			return;
		}
	}
}

void CG_Trace( trace_t *t, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, int ignore, int contentmask ) {
	ZoneScoped;

	// check against world
	CM_TransformedBoxTrace( CM_Client, cl.cms, t, start, end, mins, maxs, NULL, contentmask, Vec3( 0.0f ), Vec3( 0.0f ) );
	t->ent = t->fraction < 1.0 ? 0 : -1; // world entity is 0
	if( t->fraction == 0 ) {
		return; // blocked by the world
	}

	// check all other solid models
	CG_ClipMoveToEntities( start, mins, maxs, end, ignore, contentmask, t );
}

int CG_PointContents( Vec3 point ) {
	ZoneScoped;

	int contents = CM_TransformedPointContents( CM_Client, cl.cms, point, NULL, Vec3( 0.0f ), Vec3( 0.0f ) );

	for( int i = 0; i < cg_numSolids; i++ ) {
		const SyncEntityState * ent = cg_solidList[i];

		cmodel_t * cmodel = CM_TryFindCModel( CM_Client, ent->model );
		if( cmodel != NULL ) {
			contents |= CM_TransformedPointContents( CM_Client, cl.cms, point, cmodel, ent->origin, ent->angles );
		}
	}

	return contents;
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

	i = outgoing;
	while( predictiontime < PREDICTED_STEP_TIME ) {
		if( outgoing - i >= CMD_BACKUP ) {
			break;
		}

		frame = i & CMD_MASK;
		CL_GetUserCmd( frame, &cmd );
		predictiontime += cmd.msec;
		i--;
	}

	// run frames
	while( ++i <= outgoing ) {
		frame = i & CMD_MASK;
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
	ZoneScoped;

	int64_t ucmdExecuted, ucmdHead;
	int64_t frame;
	pmove_t pm;

	CL_GetCurrentState( NULL, &ucmdHead, NULL );
	ucmdExecuted = cg.frame.ucmdExecuted;

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
	if( ucmdHead - ucmdExecuted >= CMD_BACKUP ) {
		if( cg_showMiss->integer ) {
			Com_Printf( "exceeded CMD_BACKUP\n" );
		}

		cg.predictingTimeStamp = cl.serverTime;
		return;
	}

	// copy current state to pmove
	memset( &pm, 0, sizeof( pm ) );
	pm.playerState = &cg.predictedPlayerState;
	pm.scale = cg_entities[cg.frame.playerState.POVnum].current.scale;

	// clear the triggered toggles for this prediction round
	memset( &cg_triggersListTriggered, false, sizeof( cg_triggersListTriggered ) );

	// run frames
	while( ++ucmdExecuted <= ucmdHead ) {
		frame = ucmdExecuted & CMD_MASK;
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
		const cmodel_t * cmodel = CM_TryFindCModel( CM_Client, ent->model );
		if( cmodel != NULL && ent->linearMovement ) {
			Vec3 move;
			s64 serverTime = GS_MatchPaused( &client_gs ) ? cg.frame.serverTime : cl.serverTime + cgs.extrapolationTime;
			GS_LinearMovementDelta( ent, cg.frame.serverTime, serverTime, &move );
			cg.predictedPlayerState.pmove.origin = cg.predictedPlayerState.pmove.origin + move;
		}
	}

	CG_PredictSmoothSteps();
}
