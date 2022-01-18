/*
Copyright (C) 2009 German Garcia Fernandez ("Jal")

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

#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/qcommon.h"
#include "qcommon/string.h"
#include "qcommon/cmodel.h"
#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

// Thanks to Xavatar (xavatar2004@hotmail.com) for the path spline implementation

//===================================================================

static bool democam_editing_mode;
static int64_t demo_initial_timestamp;
static int64_t demo_time;

static bool CamIsFree;

#define CG_DemoCam_UpdateDemoTime() ( demo_time = cl.serverTime - demo_initial_timestamp )

//===================================================================

enum
{
	DEMOCAM_FIRSTPERSON,
	DEMOCAM_THIRDPERSON,
	DEMOCAM_POSITIONAL,
	DEMOCAM_PATH_LINEAR,
	DEMOCAM_PATH_SPLINE,
	DEMOCAM_ORBITAL,

	DEMOCAM_MAX_TYPES
};

static const char *cam_TypeNames[] = {
	"FirstPerson",
	"ThirdPerson",
	"Positional",
	"Path_linear",
	"Path_spline",
	"orbital",
	NULL
};

typedef struct cg_democam_s
{
	int type;
	int64_t timeStamp;
	int trackEnt;
	Vec3 origin;
	Vec3 angles;
	int fov;
	Vec3 tangent;
	Vec3 angles_tangent;
	float speed;
	struct cg_democam_s *next;
} cg_democam_t;

cg_democam_t *cg_cams_headnode = NULL;
cg_democam_t *currentcam, *nextcam;

static Vec3 cam_origin, cam_angles, cam_velocity;
static float cam_fov = 90;
static int cam_viewtype;
static int cam_POVent;
static bool cam_3dPerson;
static Vec3 cam_orbital_angles;
static float cam_orbital_radius;

static cg_democam_t *CG_Democam_FindCurrent( int64_t time ) {
	int64_t higher_time = 0;
	cg_democam_t *cam, *curcam;

	cam = cg_cams_headnode;
	curcam = NULL;
	while( cam != NULL ) {
		if( curcam == NULL || ( cam->timeStamp <= time && cam->timeStamp > higher_time ) ) {
			higher_time = cam->timeStamp;
			curcam = cam;
		}
		cam = cam->next;
	}

	return curcam;
}

static cg_democam_t *CG_Democam_FindNext( int64_t time ) {
	int64_t lower_time = INT64_MAX;
	cg_democam_t *cam, *ncam;

	cam = cg_cams_headnode;
	ncam = NULL;
	while( cam != NULL ) {
		if( cam->timeStamp > time && cam->timeStamp < lower_time ) {
			lower_time = cam->timeStamp;
			ncam = cam;
		}
		cam = cam->next;
	}

	return ncam;
}

static cg_democam_t *CG_Democam_RegisterCam( int type ) {
	cg_democam_t *cam;

	CG_DemoCam_UpdateDemoTime();

	cam = cg_cams_headnode;
	while( cam != NULL ) {
		if( cam->timeStamp == demo_time ) { // a cam exists with the very same timestamp
			Com_Printf( "warning: There was a cam with the same timestamp, it's being replaced\n" );
			break;
		}
		cam = cam->next;
	}

	if( cam == NULL ) {
		cam = ALLOC( sys_allocator, cg_democam_t );
		cam->next = cg_cams_headnode;
		cg_cams_headnode = cam;
	}

	cam->timeStamp = demo_time;
	cam->type = type;
	cam->origin = cam_origin;
	cam->angles = cam_angles;
	if( type == DEMOCAM_ORBITAL ) { // in orbital cams, the angles are the angular velocity
		cam->angles = Vec3( 0, 96, 0 );
	}
	if( type == DEMOCAM_FIRSTPERSON || type == DEMOCAM_THIRDPERSON ) {
		cam->fov = 0;
	} else {
		cam->fov = 90;
	}

	return cam;
}

static void CG_Democam_UnregisterCam( cg_democam_t *cam ) {
	cg_democam_t *tcam;

	if( !cam ) {
		return;
	}

	// headnode shortcut
	if( cg_cams_headnode == cam ) {
		cg_cams_headnode = cg_cams_headnode->next;
		FREE( sys_allocator, cam );
		return;
	}

	// find the camera which has this one as next;
	tcam = cg_cams_headnode;
	while( tcam != NULL ) {
		if( tcam->next == cam ) {
			tcam->next = cam->next;
			FREE( sys_allocator, cam );
			break;
		}
		tcam = tcam->next;
	}
}

void CG_Democam_FreeCams() {
	while( cg_cams_headnode )
		CG_Democam_UnregisterCam( cg_cams_headnode );

	cg_cams_headnode = NULL;
}

//===================================================================


//===================================================================

static void CG_Democam_ExecutePathAnalysis() {
	int64_t pathtime;
	cg_democam_t *ccam, *ncam, *pcam, *sncam;
	int count;

	pathtime = 0;

	count = 0;
	while( ( ncam = CG_Democam_FindNext( pathtime ) ) != NULL ) {
		ccam = CG_Democam_FindCurrent( pathtime );
		if( ccam ) {
			count++;
			if( ccam->type == DEMOCAM_PATH_SPLINE ) {
				pcam = NULL;
				sncam = CG_Democam_FindNext( ncam->timeStamp );
				if( ccam->timeStamp > 0 ) {
					pcam = CG_Democam_FindCurrent( ccam->timeStamp - 1 );
				}

				if( !pcam ) {
					ccam->tangent = ncam->origin - ccam->origin;
					ccam->tangent = ccam->tangent * ( 1.0 / 4.0 );

					if( ncam->angles.y - ccam->angles.y > 180 ) {
						ncam->angles.y -= 360;
					}
					if( ncam->angles.y - ccam->angles.y < -180 ) {
						ncam->angles.y += 360;
					}

					if( ncam->angles.z - ccam->angles.z > 180 ) {
						ncam->angles.z -= 360;
					}
					if( ncam->angles.z - ccam->angles.z < -180 ) {
						ncam->angles.z += 360;
					}

					ccam->angles_tangent = ncam->angles - ccam->angles;
					ccam->angles_tangent = ccam->angles_tangent * ( 1.0 / 4.0 );
				} else if( pcam ) {
					ccam->tangent = ncam->origin - pcam->origin;
					ccam->tangent = ccam->tangent * ( 1.0 / 4.0 );

					if( pcam->angles.y - ccam->angles.y > 180 ) {
						pcam->angles.y -= 360;
					}
					if( pcam->angles.y - ccam->angles.y < -180 ) {
						pcam->angles.y += 360;
					}
					if( ncam->angles.y - ccam->angles.y > 180 ) {
						ncam->angles.y -= 360;
					}
					if( ncam->angles.y - ccam->angles.y < -180 ) {
						ncam->angles.y += 360;
					}

					if( pcam->angles.z - ccam->angles.z > 180 ) {
						pcam->angles.z -= 360;
					}
					if( pcam->angles.z - ccam->angles.z < -180 ) {
						pcam->angles.z += 360;
					}
					if( ncam->angles.z - ccam->angles.z > 180 ) {
						ncam->angles.z -= 360;
					}
					if( ncam->angles.z - ccam->angles.z < -180 ) {
						ncam->angles.z += 360;
					}

					ccam->angles_tangent = ncam->angles - pcam->angles;
					ccam->angles_tangent = ccam->angles_tangent * ( 1.0 / 4.0 );
				}

				if( sncam ) {
					ncam->tangent = sncam->origin - ccam->origin;
					ncam->tangent = ncam->tangent * ( 1.0 / 4.0 );

					if( ccam->angles.y - ncam->angles.y > 180 ) {
						ccam->angles.y -= 360;
					}
					if( ccam->angles.y - ncam->angles.y < -180 ) {
						ccam->angles.y += 360;
					}
					if( sncam->angles.y - ncam->angles.y > 180 ) {
						sncam->angles.y -= 360;
					}
					if( sncam->angles.y - ncam->angles.y < -180 ) {
						sncam->angles.y += 360;
					}

					if( ccam->angles.z - ncam->angles.z > 180 ) {
						ccam->angles.z -= 360;
					}
					if( ccam->angles.z - ncam->angles.z < -180 ) {
						ccam->angles.z += 360;
					}
					if( sncam->angles.z - ncam->angles.z > 180 ) {
						sncam->angles.z -= 360;
					}
					if( sncam->angles.z - ncam->angles.z < -180 ) {
						sncam->angles.z += 360;
					}

					ncam->angles_tangent = sncam->angles - ccam->angles;
					ncam->angles_tangent = ncam->angles_tangent * ( 1.0 / 4.0 );
				} else if( !sncam ) {
					ncam->tangent = ncam->origin - ccam->origin;
					ncam->tangent = ncam->tangent * ( 1.0 / 4.0 );

					if( ncam->angles.y - ccam->angles.y > 180 ) {
						ncam->angles.y -= 360;
					}
					if( ncam->angles.y - ccam->angles.y < -180 ) {
						ncam->angles.y += 360;
					}

					if( ncam->angles.z - ccam->angles.z > 180 ) {
						ncam->angles.z -= 360;
					}
					if( ncam->angles.z - ccam->angles.z < -180 ) {
						ncam->angles.z += 360;
					}

					ncam->angles_tangent = ncam->angles - ccam->angles;
					ncam->angles_tangent = ncam->angles_tangent * ( 1.0 / 4.0 );
				}
			}
		}

		pathtime = ncam->timeStamp;
	}
}

static bool CG_LoadRecamScriptFile( const char *filename ) {
	cg_democam_t *cam = NULL;

	size_t len;
	char * buf = ReadFileString( sys_allocator, filename, &len );
	defer { FREE( sys_allocator, buf ); };
	if( buf == NULL )
		return false;

	// parse the script
	int linecount = 0;
	Span< const char > cursor = Span< const char >( buf, len );
	while( true ) {
		Span< const char > token = ParseToken( &cursor, Parse_DontStopOnNewLine );
		if( token == "" ) {
			break;
		}

		switch( linecount ) {
			case 0:
				cam = CG_Democam_RegisterCam( SpanToInt( token, 0 ) );
				break;
			case 1:
				cam->timeStamp = SpanToInt( token, 0 );
				break;
			case 2:
				cam->origin.x = SpanToFloat( token, 0.0f );
				break;
			case 3:
				cam->origin.y = SpanToFloat( token, 0.0f );
				break;
			case 4:
				cam->origin.z = SpanToFloat( token, 0.0f );
				break;
			case 5:
				cam->angles.x = SpanToFloat( token, 0.0f );
				break;
			case 6:
				cam->angles.y = SpanToFloat( token, 0.0f );
				break;
			case 7:
				cam->angles.z = SpanToFloat( token, 0.0f );
				break;
			case 8:
				cam->trackEnt = SpanToInt( token, 0 );
				break;
			case 9:
				cam->fov = SpanToInt( token, 0 );
				break;
			default:
				Com_Error( "CG_LoadRecamScriptFile: bad switch\n" );
		}

		linecount++;
		if( linecount == 10 ) {
			linecount = 0;
		}
	}

	if( linecount != 0 ) {
		Com_Printf( "CG_LoadRecamScriptFile: Invalid script. Ignored\n" );
		CG_Democam_FreeCams();
		return false;
	}

	CG_Democam_ExecutePathAnalysis();
	return true;
}

static void CG_SaveRecamScriptFile( const char *filename ) {
	if( !cg_cams_headnode ) {
		Com_Printf( "CG_SaveRecamScriptFile: no cameras to save\n" );
		return;
	}

	DynamicString output( sys_allocator );
	output.append( "// demo start time: {}\n", demo_initial_timestamp );

	const cg_democam_t * cam = cg_cams_headnode;
	while( cam != NULL ) {
		output.append( "{} {} {} {} {} {} {} {} {} {}\n",
			cam->type, cam->timeStamp,
			cam->origin.x, cam->origin.y, cam->origin.z,
			cam->angles.x, cam->angles.y, cam->angles.z,
			cam->trackEnt, cam->fov );
		cam = cam->next;
	}

	TempAllocator temp = cls.frame_arena.temp();
	WriteFile( &temp, filename, output.c_str(), output.length() );

	Com_Printf( "cam file saved\n" );
}

//===================================================================

static void CG_DrawEntityNumbers() {
	float zfar = 2048;
	int i, entnum;
	centity_t *cent;
	Vec3 dir;
	float dist;
	trace_t trace;
	Vec3 eorigin;
	// int shadowOffset = Max2( 1, frame_static.viewport_height / 600 );

	for( i = 0; i < cg.frame.numEntities; i++ ) {
		entnum = cg.frame.parsedEntities[i & ( MAX_PARSE_ENTITIES - 1 )].number;
		if( entnum < 1 || entnum >= MAX_EDICTS ) {
			continue;
		}
		cent = &cg_entities[entnum];
		if( cent->serverFrame != cg.frame.serverFrame ) {
			continue;
		}

		if( cent->current.model == EMPTY_HASH ) {
			continue;
		}

		// Kill if behind the view
		eorigin = Lerp( cent->prev.origin, cg.lerpfrac, cent->current.origin );
		dir = eorigin - cam_origin;
		dist = Length( dir ) * cg.view.fracDistFOV; dir = Normalize( dir );
		if( dist > zfar ) {
			continue;
		}

		if( Dot( dir, FromQFAxis( cg.view.axis, AXIS_FORWARD ) ) < 0 ) {
			continue;
		}

		CG_Trace( &trace, cam_origin, Vec3( 0.0f ), Vec3( 0.0f ), eorigin, cent->current.number, MASK_OPAQUE );
		if( trace.fraction == 1.0f ) {
			Vec2 coords = WorldToScreen( eorigin );
			if( ( coords.x < 0 || coords.x > frame_static.viewport_width ) || ( coords.y < 0 || coords.y > frame_static.viewport_height ) ) {
				return;
			}

			// trap_SCR_DrawString( coords.x + shadowOffset, coords.y + shadowOffset,
			// 					 ALIGN_LEFT_MIDDLE, va( "%i", cent->current.number ), cgs.fontSystemSmall, colorBlack );
			// trap_SCR_DrawString( coords.x, coords.y,
			// 					 ALIGN_LEFT_MIDDLE, va( "%i", cent->current.number ), cgs.fontSystemSmall, colorWhite );
		}
	}
}

void CG_DrawDemocam2D() {
	char sfov[8], strack[8];

	if( !cgs.demoPlaying ) {
		return;
	}

	if( !democam_editing_mode ) {
		return;
	}

	// draw the numbers of every entity in the view
	CG_DrawEntityNumbers();

	// draw the cams info
	//int xpos = 8 * frame_static.viewport_height / 600;
	//int ypos = 100 * frame_static.viewport_height / 600;

	// if( *cgs.demoName ) {
		// trap_SCR_DrawString( xpos, ypos, ALIGN_LEFT_TOP, va( "Demo: %s", cgs.demoName ), cgs.fontSystemSmall, colorWhite );
		// ypos += trap_SCR_FontHeight( cgs.fontSystemSmall );
	// }

	// trap_SCR_DrawString( xpos, ypos, ALIGN_LEFT_TOP, va( "Play mode: %s%s%s", S_COLOR_ORANGE, CamIsFree ? "Free Fly" : "Preview", S_COLOR_WHITE ), cgs.fontSystemSmall, colorWhite );
	// ypos += trap_SCR_FontHeight( cgs.fontSystemSmall );

	// trap_SCR_DrawString( xpos, ypos, ALIGN_LEFT_TOP, va( "Time: %" PRIi64, demo_time ), cgs.fontSystemSmall, colorWhite );
	// ypos += trap_SCR_FontHeight( cgs.fontSystemSmall );

	//const char * cam_type_name = "none";
	//int64_t cam_timestamp = 0;

	if( currentcam ) {
		//cam_type_name = cam_TypeNames[currentcam->type];
		//cam_timestamp = currentcam->timeStamp;
		snprintf( strack, sizeof( strack ), "%i", currentcam->trackEnt );
		snprintf( sfov, sizeof( sfov ), "%i", currentcam->fov );
	} else {
		Q_strncpyz( strack, "NO", sizeof( strack ) );
		Q_strncpyz( sfov, "NO", sizeof( sfov ) );
	}

	// trap_SCR_DrawString( xpos, ypos, ALIGN_LEFT_TOP,
	// 	va( "Current cam: " S_COLOR_ORANGE "%s" S_COLOR_WHITE " Fov " S_COLOR_ORANGE "%s" S_COLOR_WHITE " Start %" PRIi64 " Tracking " S_COLOR_ORANGE "%s" S_COLOR_WHITE,
	// 													 cam_type_name, sfov, cam_timestamp, strack ),
	// 					 cgs.fontSystemSmall, colorWhite );
	// ypos += trap_SCR_FontHeight( cgs.fontSystemSmall );

	// if( currentcam ) {
		// trap_SCR_DrawString( xpos, ypos, ALIGN_LEFT_TOP,
		// 	va( "Pitch: " S_COLOR_ORANGE "%.2f" S_COLOR_WHITE " Yaw: " S_COLOR_ORANGE "%.2f" S_COLOR_WHITE " Roll: " S_COLOR_ORANGE "%.2f" S_COLOR_WHITE,
		// 													 currentcam->angles[PITCH], currentcam->angles[YAW], currentcam->angles[ROLL] ),
		// 					 cgs.fontSystemSmall, colorWhite );
	// }
	// ypos += trap_SCR_FontHeight( cgs.fontSystemSmall );

	//cam_type_name = "none";
	//cam_timestamp = 0;
	Q_strncpyz( sfov, "NO", sizeof( sfov ) );
	if( nextcam ) {
		//cam_type_name = cam_TypeNames[nextcam->type];
		//cam_timestamp = nextcam->timeStamp;
		snprintf( strack, sizeof( strack ), "%i", nextcam->trackEnt );
		snprintf( sfov, sizeof( sfov ), "%i", nextcam->fov );
	} else {
		Q_strncpyz( strack, "NO", sizeof( strack ) );
		Q_strncpyz( sfov, "NO", sizeof( sfov ) );
	}

	// trap_SCR_DrawString( xpos, ypos, ALIGN_LEFT_TOP,
	// 	va( "Next cam: " S_COLOR_ORANGE "%s" S_COLOR_WHITE " Fov " S_COLOR_ORANGE "%s" S_COLOR_WHITE " Start %" PRIi64 " Tracking " S_COLOR_ORANGE "%s" S_COLOR_WHITE,
	// 													 cam_type_name, sfov, cam_timestamp, strack ),
	// 					 cgs.fontSystemSmall, colorWhite );
	// ypos += trap_SCR_FontHeight( cgs.fontSystemSmall );

	// if( nextcam ) {
		// trap_SCR_DrawString( xpos, ypos, ALIGN_LEFT_TOP,
		// 	va( "Pitch: " S_COLOR_ORANGE "%.2f" S_COLOR_WHITE " Yaw: " S_COLOR_ORANGE "%.2f" S_COLOR_WHITE " Roll: " S_COLOR_ORANGE "%.2f" S_COLOR_WHITE,
		// 													 nextcam->angles[PITCH], nextcam->angles[YAW], nextcam->angles[ROLL] ),
		// 					 cgs.fontSystemSmall, colorWhite );
	// }
	// ypos += trap_SCR_FontHeight( cgs.fontSystemSmall );
}

//===================================================================

static bool CG_DemoCam_LookAt( int trackEnt, Vec3 vieworg, Vec3 * viewangles ) {
	if( trackEnt < 1 || trackEnt >= MAX_EDICTS ) {
		return false;
	}

	const centity_t * cent = &cg_entities[trackEnt];
	if( cent->serverFrame != cg.frame.serverFrame ) {
		return false;
	}

	// seems to be valid. Find the angles to look at this entity
	Vec3 origin = Lerp( cent->prev.origin, cg.lerpfrac, cent->current.origin );

	// if having a bounding box, look to its center
	const cmodel_t *cmodel = CG_CModelForEntity( trackEnt );
	if( cmodel != NULL ) {
		Vec3 mins, maxs;
		CM_InlineModelBounds( cl.map->cms, cmodel, &mins, &maxs );
		origin += mins + maxs;
	}

	Vec3 dir = Normalize( origin - vieworg );
	*viewangles = VecToAngles( dir );
	return true;
}

int CG_DemoCam_GetViewType() {
	return cam_viewtype;
}

bool CG_DemoCam_GetThirdPerson() {
	return ( cam_viewtype == VIEWDEF_PLAYERVIEW && cam_3dPerson );
}

void CG_DemoCam_GetViewDef( cg_viewdef_t *view ) {
	view->POVent = cam_POVent;
	view->thirdperson = cam_3dPerson;
	view->playerPrediction = false;
	view->drawWeapon = false;
	view->draw2D = false;
}

float CG_DemoCam_GetOrientation( Vec3 * origin, Vec3 * angles, Vec3 * velocity ) {
	*angles = cam_angles;
	*origin = cam_origin;
	*velocity = cam_velocity;

	if( !currentcam || !currentcam->fov ) {
		return FOV;
	}

	return cam_fov;
}

static short freecam_delta_angles[3];

int CG_DemoCam_FreeFly() {
	UserCommand cmd;
	const float SPEED = 500;

	if( cgs.demoPlaying && CamIsFree ) {
		Vec3 wishvel, forward, right, up, moveangles;
		float fmove, smove, upmove, wishspeed, maxspeed;

		maxspeed = 250;

		// run frame
		CL_GetUserCmd( CL_GetCurrentUserCmdNum() - 1, &cmd );
		cmd.msec = cls.realFrameTime;

		moveangles.x = SHORT2ANGLE( cmd.angles[ 0 ] ) + SHORT2ANGLE( freecam_delta_angles[ 0 ] );
		moveangles.y = SHORT2ANGLE( cmd.angles[ 1 ] ) + SHORT2ANGLE( freecam_delta_angles[ 1 ] );
		moveangles.z = SHORT2ANGLE( cmd.angles[ 2 ] ) + SHORT2ANGLE( freecam_delta_angles[ 2 ] );

		AngleVectors( moveangles, &forward, &right, &up );
		cam_angles = moveangles;

		fmove = cmd.forwardmove * SPEED / 127.0f;
		smove = cmd.sidemove * SPEED / 127.0f;
		upmove = cmd.upmove * SPEED / 127.0f;
		if( cmd.buttons & BUTTON_SPECIAL ) {
			maxspeed *= 2;
		}

		wishvel = forward * fmove + right * smove;
		wishvel.z += upmove;

		wishspeed = Length( wishvel );
		if( wishspeed > maxspeed ) {
			wishspeed = maxspeed / wishspeed;
			wishvel = wishvel * ( wishspeed );
			wishspeed = maxspeed;
		}

		cam_origin = cam_origin + wishvel * ( (float)cls.realFrameTime * 0.001f );

		cam_POVent = 0;
		cam_3dPerson = false;
		return VIEWDEF_DEMOCAM;
	}

	return VIEWDEF_PLAYERVIEW;
}

static void CG_Democam_SetCameraPositionFromView() {
	if( cg.view.type == VIEWDEF_PLAYERVIEW ) {
		cam_origin = cg.view.origin;
		cam_angles = cg.view.angles;
		cam_velocity = cg.view.velocity;
		cam_fov = cg.view.fov_y;
		cam_orbital_radius = 0;
	}

	if( !CamIsFree ) {
		UserCommand cmd;

		CL_GetUserCmd( CL_GetCurrentUserCmdNum() - 1, &cmd );

		freecam_delta_angles[ 0 ] = ANGLE2SHORT( cam_angles.x ) - cmd.angles[ 0 ];
		freecam_delta_angles[ 1 ] = ANGLE2SHORT( cam_angles.y ) - cmd.angles[ 1 ];
		freecam_delta_angles[ 2 ] = ANGLE2SHORT( cam_angles.z ) - cmd.angles[ 2 ];
	} else {
		cam_orbital_radius = 0;
	}
}

static int CG_Democam_CalcView() {
	int viewType;
	float lerpfrac;
	Vec3 v;

	viewType = VIEWDEF_PLAYERVIEW;
	cam_velocity = Vec3( 0.0f );

	if( currentcam ) {
		if( !nextcam ) {
			lerpfrac = 0;
		} else {
			lerpfrac = (float)( demo_time - currentcam->timeStamp ) / (float)( nextcam->timeStamp - currentcam->timeStamp );
		}

		switch( currentcam->type ) {
			case DEMOCAM_FIRSTPERSON:
				cam_origin = cg.view.origin;
				cam_angles = cg.view.angles;
				cam_velocity = cg.view.velocity;
				cam_fov = cg.view.fov_y;
				break;

			case DEMOCAM_THIRDPERSON:
				cam_origin = cg.view.origin;
				cam_angles = cg.view.angles;
				cam_velocity = cg.view.velocity;
				cam_fov = cg.view.fov_y;
				cam_3dPerson = true;
				break;

			case DEMOCAM_POSITIONAL:
				viewType = VIEWDEF_DEMOCAM;
				cam_POVent = 0;
				cam_origin = currentcam->origin;
				if( !CG_DemoCam_LookAt( currentcam->trackEnt, cam_origin, &cam_angles ) ) {
					cam_angles = currentcam->angles;
				}
				cam_fov = currentcam->fov;
				break;

			case DEMOCAM_PATH_LINEAR:
				viewType = VIEWDEF_DEMOCAM;
				cam_POVent = 0;
				v = cam_origin;

				if( !nextcam || nextcam->type == DEMOCAM_FIRSTPERSON || nextcam->type == DEMOCAM_THIRDPERSON ) {
					Com_Printf( "Warning: CG_DemoCam: path_linear cam without a valid next cam\n" );
					cam_origin = currentcam->origin;
					if( !CG_DemoCam_LookAt( currentcam->trackEnt, cam_origin, &cam_angles ) ) {
						cam_angles = currentcam->angles;
					}
					cam_fov = currentcam->fov;
				} else {
					cam_origin = Lerp( currentcam->origin, lerpfrac, nextcam->origin );
					if( !CG_DemoCam_LookAt( currentcam->trackEnt, cam_origin, &cam_angles ) ) {
						cam_angles = LerpAngles( currentcam->angles, lerpfrac, nextcam->angles );
					}
					cam_fov = (float)currentcam->fov + (float)( nextcam->fov - currentcam->fov ) * lerpfrac;
				}

				// set velocity
				cam_velocity = cam_origin - v;
				break;

			case DEMOCAM_PATH_SPLINE:
				viewType = VIEWDEF_DEMOCAM;
				cam_POVent = 0;
				lerpfrac = Clamp01( lerpfrac );
				v = cam_origin;

				if( !nextcam || nextcam->type == DEMOCAM_FIRSTPERSON || nextcam->type == DEMOCAM_THIRDPERSON ) {
					Com_Printf( "Warning: CG_DemoCam: path_spline cam without a valid next cam\n" );
					cam_origin = currentcam->origin;
					if( !CG_DemoCam_LookAt( currentcam->trackEnt, cam_origin, &cam_angles ) ) {
						cam_angles = currentcam->angles;
					}
					cam_fov = currentcam->fov;
				} else {  // valid spline path
#define VectorHermiteInterp( a, at, b, bt, c, v )  ( ( v ).x = ( 2 * powf( c, 3 ) - 3 * powf( c, 2 ) + 1 ) * a.x + ( powf( c, 3 ) - 2 * powf( c, 2 ) + c ) * 2 * at.x + ( -2 * powf( c, 3 ) + 3 * powf( c, 2 ) ) * b.x + ( powf( c, 3 ) - powf( c, 2 ) ) * 2 * bt.x, ( v ).y = ( 2 * powf( c, 3 ) - 3 * powf( c, 2 ) + 1 ) * a.y + ( powf( c, 3 ) - 2 * powf( c, 2 ) + c ) * 2 * at.y + ( -2 * powf( c, 3 ) + 3 * powf( c, 2 ) ) * b.y + ( powf( c, 3 ) - powf( c, 2 ) ) * 2 * bt.y, ( v ).z = ( 2 * powf( c, 3 ) - 3 * powf( c, 2 ) + 1 ) * a.z + ( powf( c, 3 ) - 2 * powf( c, 2 ) + c ) * 2 * at.z + ( -2 * powf( c, 3 ) + 3 * powf( c, 2 ) ) * b.z + ( powf( c, 3 ) - powf( c, 2 ) ) * 2 * bt.z )

					float lerpspline, A, B, C, n1, n2, n3;
					cg_democam_t *previouscam = NULL;
					cg_democam_t *secondnextcam = NULL;

					if( nextcam ) {
						secondnextcam = CG_Democam_FindNext( nextcam->timeStamp );
					}
					if( currentcam->timeStamp > 0 ) {
						previouscam = CG_Democam_FindCurrent( currentcam->timeStamp - 1 );
					}

					if( !previouscam && nextcam && !secondnextcam ) {
						lerpfrac = (float)( demo_time - currentcam->timeStamp ) / (float)( nextcam->timeStamp - currentcam->timeStamp );
						lerpspline = lerpfrac;
					} else if( !previouscam && nextcam && secondnextcam ) {
						n1 = nextcam->timeStamp - currentcam->timeStamp;
						n2 = secondnextcam->timeStamp - nextcam->timeStamp;
						A = n1 * ( n1 - n2 ) / ( powf( n1, 2 ) + n1 * n2 - n1 - n2 );
						B = ( 2 * n1 * n2 - n1 - n2 ) / ( powf( n1, 2 ) + n1 * n2 - n1 - n2 );
						lerpfrac = (float)( demo_time - currentcam->timeStamp ) / (float)( nextcam->timeStamp - currentcam->timeStamp );
						lerpspline = A * powf( lerpfrac, 2 ) + B * lerpfrac;
					} else if( previouscam && nextcam && !secondnextcam ) {
						n2 = currentcam->timeStamp - previouscam->timeStamp;
						n3 = nextcam->timeStamp - currentcam->timeStamp;
						A = n3 * ( n2 - n3 ) / ( -n2 - n3 + n2 * n3 + powf( n3, 2 ) );
						B = -1 / ( -n2 - n3 + n2 * n3 + powf( n3, 2 ) ) * ( n2 + n3 - 2 * powf( n3, 2 ) );
						lerpfrac = (float)( demo_time - currentcam->timeStamp ) / (float)( nextcam->timeStamp - currentcam->timeStamp );
						lerpspline = A * powf( lerpfrac, 2 ) + B * lerpfrac;
					} else if( previouscam && nextcam && secondnextcam ) {
						n1 = currentcam->timeStamp - previouscam->timeStamp;
						n2 = nextcam->timeStamp - currentcam->timeStamp;
						n3 = secondnextcam->timeStamp - nextcam->timeStamp;
						A = -2 * powf( n2, 2 ) * ( -powf( n2, 2 ) + n1 * n3 ) / ( 2 * n2 * n3 + powf( n2, 3 ) * n3 - 3 * powf( n2, 2 ) * n1 + n1 * powf( n2, 3 ) + 2 * n1 * n2 - 3 * powf( n2, 2 ) * n3 - 3 * powf( n2, 3 ) + 2 * powf( n2, 2 ) + powf( n2, 4 ) + n1 * powf( n2, 2 ) * n3 - 3 * n1 * n2 * n3 + 2 * n1 * n3 );
						B = powf( n2, 2 ) * ( -2 * n1 - 3 * powf( n2, 2 ) - n2 * n3 + 2 * n3 + 3 * n1 * n3 + n1 * n2 ) / ( 2 * n2 * n3 + powf( n2, 3 ) * n3 - 3 * powf( n2, 2 ) * n1 + n1 * powf( n2, 3 ) + 2 * n1 * n2 - 3 * powf( n2, 2 ) * n3 - 3 * powf( n2, 3 ) + 2 * powf( n2, 2 ) + powf( n2, 4 ) + n1 * powf( n2, 2 ) * n3 - 3 * n1 * n2 * n3 + 2 * n1 * n3 );
						C = -( powf( n2, 2 ) * n1 - 2 * n1 * n2 + 3 * n1 * n2 * n3 - 2 * n1 * n3 - 2 * powf( n2, 4 ) + 3 * powf( n2, 3 ) - 2 * powf( n2, 3 ) * n3 + 5 * powf( n2, 2 ) * n3 - 2 * powf( n2, 2 ) - 2 * n2 * n3 ) / ( 2 * n2 * n3 + powf( n2, 3 ) * n3 - 3 * powf( n2, 2 ) * n1 + n1 * powf( n2, 3 ) + 2 * n1 * n2 - 3 * powf( n2, 2 ) * n3 - 3 * powf( n2, 3 ) + 2 * powf( n2, 2 ) + powf( n2, 4 ) + n1 * powf( n2, 2 ) * n3 - 3 * n1 * n2 * n3 + 2 * n1 * n3 );
						lerpfrac = (float)( demo_time - currentcam->timeStamp ) / (float)( nextcam->timeStamp - currentcam->timeStamp );
						lerpspline = A * powf( lerpfrac, 3 ) + B * powf( lerpfrac, 2 ) + C * lerpfrac;
					} else {
						lerpfrac = 0;
						lerpspline = 0;
					}


					VectorHermiteInterp( currentcam->origin, currentcam->tangent, nextcam->origin, nextcam->tangent, lerpspline, cam_origin );
					if( !CG_DemoCam_LookAt( currentcam->trackEnt, cam_origin, &cam_angles ) ) {
						VectorHermiteInterp( currentcam->angles, currentcam->angles_tangent, nextcam->angles, nextcam->angles_tangent, lerpspline, cam_angles );
					}
					cam_fov = (float)currentcam->fov + (float)( nextcam->fov - currentcam->fov ) * lerpfrac;
#undef VectorHermiteInterp
				}

				// set velocity
				cam_velocity = cam_origin - v;
				break;

			case DEMOCAM_ORBITAL:
				viewType = VIEWDEF_DEMOCAM;
				cam_POVent = 0;
				cam_fov = currentcam->fov;
				v = cam_origin;

				if( !currentcam->trackEnt || currentcam->trackEnt >= MAX_EDICTS ) {
					Com_Printf( "Warning: CG_DemoCam: orbital cam needs a track entity set\n" );
					cam_origin = currentcam->origin;
					cam_angles = Vec3( 0.0f );
					cam_velocity = Vec3( 0.0f );
				} else {
					Vec3 center, forward;
					const cmodel_t *cmodel;
					const float ft = (float)cls.frametime * 0.001f;

					// find the trackEnt origin
					center = Lerp( cg_entities[currentcam->trackEnt].prev.origin, cg.lerpfrac, cg_entities[currentcam->trackEnt].current.origin );

					// if having a bounding box, look to its center
					if( ( cmodel = CG_CModelForEntity( currentcam->trackEnt ) ) != NULL ) {
						Vec3 mins, maxs;
						CM_InlineModelBounds( cl.map->cms, cmodel, &mins, &maxs );
						center += mins + maxs;
					}

					if( !cam_orbital_radius ) {
						// cam is just started, find distance from cam to trackEnt and keep it as radius
						forward = currentcam->origin - center;
						cam_orbital_radius = Length( forward );
						forward = Normalize( forward );
						cam_orbital_angles = VecToAngles( forward );
					}

					cam_orbital_angles += currentcam->angles * ft;
					for( int i = 0; i < 3; i++ ) {
						cam_orbital_angles[i] = AngleNormalize360( cam_orbital_angles[i] );
					}

					AngleVectors( cam_orbital_angles, &forward, NULL, NULL );
					cam_origin = center + forward * ( cam_orbital_radius );

					// lookat
					forward = forward * -1;
					cam_angles = VecToAngles( forward );
				}

				// set velocity
				cam_velocity = cam_origin - v;
				break;

			default:
				break;
		}

		if( currentcam->type != DEMOCAM_ORBITAL ) {
			cam_orbital_angles = Vec3( 0.0f );
			cam_orbital_radius = 0;
		}
	}

	return viewType;
}

bool CG_DemoCam_Update() {
	if( !cgs.demoPlaying ) {
		return false;
	}

	if( !demo_initial_timestamp && cg.frame.valid ) {
		demo_initial_timestamp = cl.serverTime;
	}

	CG_DemoCam_UpdateDemoTime();

	// see if we have any cams to be played
	currentcam = CG_Democam_FindCurrent( demo_time );
	nextcam = CG_Democam_FindNext( demo_time );

	cam_3dPerson = false;
	cam_viewtype = VIEWDEF_PLAYERVIEW;
	cam_POVent = cg.frame.playerState.POVnum;

	if( CamIsFree ) {
		cam_viewtype = CG_DemoCam_FreeFly();
	} else if( currentcam ) {
		cam_viewtype = CG_Democam_CalcView();
	}

	CG_Democam_SetCameraPositionFromView();

	return true;
}

bool CG_DemoCam_IsFree() {
	return CamIsFree;
}

static void CG_DemoFreeFly_Cmd_f() {
	if( Cmd_Argc() > 1 ) {
		if( !Q_stricmp( Cmd_Argv( 1 ), "on" ) ) {
			CamIsFree = true;
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "off" ) ) {
			CamIsFree = false;
		}
	} else {
		CamIsFree = !CamIsFree;
	}

	cam_velocity = Vec3( 0.0f );
	Com_Printf( "demo cam mode %s\n", CamIsFree ? "Free Fly" : "Preview" );
}

static void CG_DemoCamSwitch_Cmd_f() {

}

static void CG_AddCam_Cmd_f() {
	int type, i;

	CG_DemoCam_UpdateDemoTime();

	if( Cmd_Argc() == 2 ) {
		// type
		type = -1;
		for( i = 0; cam_TypeNames[i] != NULL; i++ ) {
			if( !Q_stricmp( cam_TypeNames[i], Cmd_Argv( 1 ) ) ) {
				type = i;
				break;
			}
		}

		if( type != -1 ) {
			// valid. Register and return
			if( CG_Democam_RegisterCam( type ) != NULL ) {
				Com_Printf( "cam added\n" );

				// update current cam
				CG_Democam_ExecutePathAnalysis();
				currentcam = CG_Democam_FindCurrent( demo_time );
				nextcam = CG_Democam_FindNext( demo_time );
				return;
			}
		}
	}

	// print help
	Com_Printf( " : Usage: AddCam <type>\n" );
	Com_Printf( " : Available types:\n" );
	for( i = 0; cam_TypeNames[i] != NULL; i++ )
		Com_Printf( " : %s\n", cam_TypeNames[i] );
}

static void CG_DeleteCam_Cmd_f() {
	if( !currentcam ) {
		Com_Printf( "DeleteCam: No current cam to delete\n" );
		return;
	}

	CG_DemoCam_UpdateDemoTime();
	currentcam = CG_Democam_FindCurrent( demo_time );

	CG_Democam_UnregisterCam( currentcam );

	// update pointer to new current cam
	CG_Democam_ExecutePathAnalysis();
	currentcam = CG_Democam_FindCurrent( demo_time );
	nextcam = CG_Democam_FindNext( demo_time );
	Com_Printf( "cam deleted\n" );
}

static void CG_EditCam_Cmd_f() {
	CG_DemoCam_UpdateDemoTime();

	currentcam = CG_Democam_FindCurrent( demo_time );
	if( !currentcam ) {
		Com_Printf( "Editcam: no current cam\n" );
		return;
	}

	if( Cmd_Argc() >= 2 && Q_stricmp( Cmd_Argv( 1 ), "help" ) ) {
		if( !Q_stricmp( Cmd_Argv( 1 ), "type" ) ) {
			int type, i;
			if( Cmd_Argc() < 3 ) { // not enough parameters, print help
				Com_Printf( "Usage: EditCam type <type name>\n" );
				return;
			}

			// type
			type = -1;
			for( i = 0; cam_TypeNames[i] != NULL; i++ ) {
				if( !Q_stricmp( cam_TypeNames[i], Cmd_Argv( 2 ) ) ) {
					type = i;
					break;
				}
			}

			if( type != -1 ) {
				// valid. Register and return
				currentcam->type = type;
				Com_Printf( "cam edited\n" );
				CG_Democam_ExecutePathAnalysis();
				return;
			} else {
				Com_Printf( "invalid type name\n" );
			}
		}
		if( !Q_stricmp( Cmd_Argv( 1 ), "track" ) ) {
			if( Cmd_Argc() < 3 ) {
				// not enough parameters, print help
				Com_Printf( "Usage: EditCam track <entity number> ( 0 for no tracking )\n" );
				return;
			}
			currentcam->trackEnt = atoi( Cmd_Argv( 2 ) );
			Com_Printf( "cam edited\n" );
			CG_Democam_ExecutePathAnalysis();
			return;
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "fov" ) ) {
			if( Cmd_Argc() < 3 ) {
				// not enough parameters, print help
				Com_Printf( "Usage: EditCam fov <value>\n" );
				return;
			}
			currentcam->fov = atoi( Cmd_Argv( 2 ) );
			Com_Printf( "cam edited\n" );
			CG_Democam_ExecutePathAnalysis();
			return;
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "timeOffset" ) ) {
			int64_t newtimestamp;
			if( Cmd_Argc() < 3 ) {
				// not enough parameters, print help
				Com_Printf( "Usage: EditCam timeOffset <value>\n" );
				return;
			}
			newtimestamp = currentcam->timeStamp += atoi( Cmd_Argv( 2 ) );
			if( newtimestamp + cl.serverTime <= demo_initial_timestamp ) {
				newtimestamp = 1;
			}
			currentcam->timeStamp = newtimestamp;
			currentcam = CG_Democam_FindCurrent( demo_time );
			nextcam = CG_Democam_FindNext( demo_time );
			Com_Printf( "cam edited\n" );
			CG_Democam_ExecutePathAnalysis();
			return;
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "origin" ) ) {
			currentcam->origin = cg.view.origin;
			cam_orbital_radius = 0;
			Com_Printf( "cam edited\n" );
			CG_Democam_ExecutePathAnalysis();
			return;
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "angles" ) ) {
			currentcam->angles = cg.view.angles;
			Com_Printf( "cam edited\n" );
			CG_Democam_ExecutePathAnalysis();
			return;
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "pitch" ) ) {
			if( Cmd_Argc() < 3 ) {
				// not enough parameters, print help
				Com_Printf( "Usage: EditCam pitch <value>\n" );
				return;
			}
			currentcam->angles.x = atof( Cmd_Argv( 2 ) );
			Com_Printf( "cam edited\n" );
			CG_Democam_ExecutePathAnalysis();
			return;
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "yaw" ) ) {
			if( Cmd_Argc() < 3 ) {
				// not enough parameters, print help
				Com_Printf( "Usage: EditCam yaw <value>\n" );
				return;
			}
			currentcam->angles.y = atof( Cmd_Argv( 2 ) );
			Com_Printf( "cam edited\n" );
			CG_Democam_ExecutePathAnalysis();
			return;
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "roll" ) ) {
			if( Cmd_Argc() < 3 ) {
				// not enough parameters, print help
				Com_Printf( "Usage: EditCam roll <value>\n" );
				return;
			}
			currentcam->angles.z = atof( Cmd_Argv( 2 ) );
			Com_Printf( "cam edited\n" );
			CG_Democam_ExecutePathAnalysis();
			return;
		}
	}

	// print help
	Com_Printf( " : Usage: EditCam <command>\n" );
	Com_Printf( " : Available commands:\n" );
	Com_Printf( " : type <type name>\n" );
	Com_Printf( " : track <entity number> ( 0 for no track )\n" );
	Com_Printf( " : fov <value> ( only for not player views )\n" );
	Com_Printf( " : timeOffset <value> ( + or - milliseconds to be added to camera timestamp )\n" );
	Com_Printf( " : origin ( changes cam to current origin )\n" );
	Com_Printf( " : angles ( changes cam to current angles )\n" );
	Com_Printf( " : pitch <value> ( assigns pitch angle to current cam )\n" );
	Com_Printf( " : yaw <value> ( assigns yaw angle to current cam )\n" );
	Com_Printf( " : roll <value> ( assigns roll angle to current cam )\n" );
}

void CG_SaveCam_Cmd_f() {
	if( !cgs.demoPlaying ) {
		return;
	}

	TempAllocator temp = cls.frame_arena.temp();
	char * path = temp( "{}/base/demos/{}.cam", HomeDirPath(), Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : cgs.demoName );

	CG_SaveRecamScriptFile( path );
}

void CG_Democam_ImportCams_f() {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: importcams <filename> (relative to demos directory)\n" );
		return;
	}

	TempAllocator temp = cls.frame_arena.temp();
	char * path = temp( "{}/base/demos/{}.cam", HomeDirPath(), Cmd_Argv( 1 ) );

	if( CG_LoadRecamScriptFile( path ) ) {
		Com_Printf( "cam script imported\n" );
	} else {
		Com_Printf( "CG_Democam_ImportCams_f: no valid file found\n" );
	}
}

void CG_DemoEditMode_RemoveCmds() {
	RemoveCommand( "addcam" );
	RemoveCommand( "deletecam" );
	RemoveCommand( "editcam" );
	RemoveCommand( "saverecam" );
	RemoveCommand( "clearcams" );
	RemoveCommand( "importcams" );
}

static void CG_DemoEditMode_Cmd_f() {
	if( !cgs.demoPlaying ) {
		return;
	}

	if( Cmd_Argc() > 1 ) {
		if( !Q_stricmp( Cmd_Argv( 1 ), "on" ) ) {
			democam_editing_mode = true;
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "off" ) ) {
			democam_editing_mode = false;
		}
	} else {
		democam_editing_mode = !democam_editing_mode;
	}

	Com_Printf( "demo cam editing mode %s\n", democam_editing_mode ? "on" : "off" );
	if( democam_editing_mode ) {
		AddCommand( "addcam", CG_AddCam_Cmd_f );
		AddCommand( "deletecam", CG_DeleteCam_Cmd_f );
		AddCommand( "editcam", CG_EditCam_Cmd_f );
		AddCommand( "saverecam", CG_SaveCam_Cmd_f );
		AddCommand( "clearcams", CG_Democam_FreeCams );
		AddCommand( "importcams", CG_Democam_ImportCams_f );
	} else {
		CG_DemoEditMode_RemoveCmds();
	}
}

void CG_DemocamInit() {
	democam_editing_mode = false;
	demo_time = 0;
	demo_initial_timestamp = 0;

	if( !cgs.demoPlaying ) {
		return;
	}

	if( !*cgs.demoName ) {
		Com_Error( "CG_DemocamInit: no demo name string\n" );
	}

	// see if there is any script for this demo, and load it
	TempAllocator temp = cls.frame_arena.temp();
	char * path = temp( "{}/base/demos/{}.cam", HomeDirPath(), cgs.demoName );

	Com_Printf( "cam: %s\n", path );

	// add console commands
	AddCommand( "demoEditMode", CG_DemoEditMode_Cmd_f );
	AddCommand( "demoFreeFly", CG_DemoFreeFly_Cmd_f );
	AddCommand( "democamswitch", CG_DemoCamSwitch_Cmd_f );

	if( CG_LoadRecamScriptFile( path ) ) {
		Com_Printf( "Loaded demo cam script\n" );
	}
}

void CG_DemocamShutdown() {
	if( !cgs.demoPlaying ) {
		return;
	}

	// remove console commands
	RemoveCommand( "demoEditMode" );
	RemoveCommand( "demoFreeFly" );
	RemoveCommand( "democamswitch" );
	if( democam_editing_mode ) {
		CG_DemoEditMode_RemoveCmds();
	}

	CG_Democam_FreeCams();
}

void CG_DemocamReset() {
	demo_time = 0;
	demo_initial_timestamp = 0;
}
