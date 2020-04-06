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
#include "qcommon/assets.h"

static WeaponModelMetadata cg_pWeaponModelInfos[ Weapon_Count ];

/*
* CG_vWeap_ParseAnimationScript
*
* script:
* 0 = first frame
* 1 = lastframe/number of frames
* 2 = looping frames
* 3 = frame time
*/
static bool CG_vWeap_ParseAnimationScript( WeaponModelMetadata *weaponinfo, const char *filename ) {
	int i;
	bool debug = cg_debugWeaponModels->integer != 0;
	int anim_data[4][VWEAP_MAXANIMS] = { };

	int rounder = 0;
	int counter = 1; // reserve 0 for 'no animation'

	Span< const char > contents = AssetString( filename );
	if( contents.ptr == NULL )
		return false;

	if( debug ) {
		Com_Printf( "%sLoading weapon animation script:%s%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE );
	}

	//proceed
	const char * ptr = contents.ptr;
	while( ptr ) {
		const char * token = COM_ParseExt( &ptr, true );
		if( !token[0] ) {
			break;
		}

		//see if it is keyword or number
		if( *token < '0' || *token > '9' ) {
			if( !Q_stricmp( token, "handOffset" ) ) {
				if( debug ) {
					Com_Printf( "%sScript: handPosition:%s", S_COLOR_YELLOW, S_COLOR_WHITE );
				}

				weaponinfo->handpositionOrigin[FORWARD] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionOrigin[RIGHT] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionOrigin[UP] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionAngles[PITCH] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionAngles[YAW] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionAngles[ROLL] = atof( COM_ParseExt( &ptr, false ) );

				if( debug ) {
					Com_Printf( "%s%f %f %f %f %f %f%s\n", S_COLOR_YELLOW,
							   weaponinfo->handpositionOrigin[0], weaponinfo->handpositionOrigin[1], weaponinfo->handpositionOrigin[2],
							   weaponinfo->handpositionAngles[0], weaponinfo->handpositionAngles[1], weaponinfo->handpositionAngles[2],
							   S_COLOR_WHITE );
				}

			} else if( token[0] && debug ) {
				Com_Printf( "%signored: %s%s\n", S_COLOR_YELLOW, token, S_COLOR_WHITE );
			}
		} else {
			//frame & animation values
			i = (int)atoi( token );
			if( debug ) {
				if( rounder == 0 ) {
					Com_Printf( "%sScript: %s", S_COLOR_YELLOW, S_COLOR_WHITE );
				}
				Com_Printf( "%s%i - %s", S_COLOR_YELLOW, i, S_COLOR_WHITE );
			}
			anim_data[rounder][counter] = i;
			rounder++;
			if( rounder > 3 ) {
				rounder = 0;
				if( debug ) {
					Com_Printf( "%s anim: %i%s\n", S_COLOR_YELLOW, counter, S_COLOR_WHITE );
				}
				counter++;
				if( counter == VWEAP_MAXANIMS ) {
					break;
				}
			}
		}
	}

	if( counter < VWEAP_MAXANIMS ) {
		Com_Printf( "%sERROR: incomplete WEAPON script: %s - Using default%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE );
		return false;
	}

	//reorganize to make my life easier
	for( i = 0; i < VWEAP_MAXANIMS; i++ ) {
		weaponinfo->firstframe[i] = anim_data[0][i];
		weaponinfo->lastframe[i] = anim_data[1][i];
		weaponinfo->loopingframes[i] = anim_data[2][i];

		if( anim_data[3][i] < 10 ) { //never allow less than 10 fps
			anim_data[3][i] = 10;
		}

		weaponinfo->frametime[i] = 1000 / anim_data[3][i];
	}

	return true;
}

/*
* CG_CreateHandDefaultAnimations
*/
static void CG_CreateHandDefaultAnimations( WeaponModelMetadata *weaponinfo ) {
	float defaultfps = 15.0f;

	// default wsw hand
	weaponinfo->firstframe[WEAPANIM_STANDBY] = 0;
	weaponinfo->lastframe[WEAPANIM_STANDBY] = 0;
	weaponinfo->loopingframes[WEAPANIM_STANDBY] = 1;
	weaponinfo->frametime[WEAPANIM_STANDBY] = 1000 / defaultfps;

	weaponinfo->firstframe[WEAPANIM_ATTACK] = 0;
	weaponinfo->lastframe[WEAPANIM_ATTACK] = 0;
	weaponinfo->loopingframes[WEAPANIM_ATTACK] = 1;
	weaponinfo->frametime[WEAPANIM_ATTACK] = 1000 / defaultfps;

	weaponinfo->firstframe[WEAPANIM_WEAPDOWN] = 0;
	weaponinfo->lastframe[WEAPANIM_WEAPDOWN] = 0;
	weaponinfo->loopingframes[WEAPANIM_WEAPDOWN] = 1;
	weaponinfo->frametime[WEAPANIM_WEAPDOWN] = 1000 / defaultfps;

	weaponinfo->firstframe[WEAPANIM_WEAPONUP] = 6; // flipout animation (6-10)
	weaponinfo->lastframe[WEAPANIM_WEAPONUP] = 10;
	weaponinfo->loopingframes[WEAPANIM_WEAPONUP] = 1;
	weaponinfo->frametime[WEAPANIM_WEAPONUP] = 1000 / defaultfps;
}

/*
* CG_WeaponModelUpdateRegistration
*/
static void CG_WeaponModelUpdateRegistration( WeaponModelMetadata *weaponinfo, const char *filename ) {
	TempAllocator temp = cls.frame_arena.temp();

	if( cg_debugWeaponModels->integer ) {
		Com_Printf( "%sWEAPmodel: Loading %s%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE );
	}

	weaponinfo->model = FindModel( temp( "weapons/{}/model", filename ) );
	if( weaponinfo->model == NULL )
		return;

	// load animation script for the hand model
	if( !CG_vWeap_ParseAnimationScript( weaponinfo, temp( "weapons/{}/model.cfg", filename ) ) ) {
		CG_CreateHandDefaultAnimations( weaponinfo );
	}

	weaponinfo->fire_sound = FindSoundEffect( temp( "weapons/{}/fire", filename ) );
	weaponinfo->up_sound = FindSoundEffect( temp( "weapons/{}/up", filename ) );
	weaponinfo->zoom_in_sound = FindSoundEffect( temp( "weapons/{}/zoom_in", filename ) );
	weaponinfo->zoom_out_sound = FindSoundEffect( temp( "weapons/{}/zoom_out", filename ) );

	VectorSet( weaponinfo->tag_projectionsource.origin, 16, 0, 8 );
	Matrix3_Identity( weaponinfo->tag_projectionsource.axis );

	if( cg_debugWeaponModels->integer ) {
		Com_Printf( "%sWEAPmodel: Loaded successful%s\n", S_COLOR_YELLOW, S_COLOR_WHITE );
	}
}

/*
* CG_RegisterWeaponModel
*/
WeaponModelMetadata *CG_RegisterWeaponModel( const char *cgs_name, WeaponType weaponTag ) {
	char filename[MAX_QPATH];
	Q_strncpyz( filename, cgs_name, sizeof( filename ) );
	COM_StripExtension( filename );

	WeaponModelMetadata * weaponinfo = &cg_pWeaponModelInfos[ weaponTag ];
	if( weaponinfo->inuse ) {
		return weaponinfo;
	}

	CG_WeaponModelUpdateRegistration( weaponinfo, filename );
	weaponinfo->inuse = true;

	return weaponinfo;
}

/*
* CG_CreateWeaponZeroModel
*
* we can't allow NULL weaponmodels to be passed to the viewweapon.
* They will produce crashes because the lack of animation script.
* We need to have at least one weaponinfo with a script to be used
* as a replacement, so, weapon 0 will have the animation script
* even if the registration failed
*/
WeaponModelMetadata * CG_CreateWeaponZeroModel() {
	WeaponModelMetadata * weaponinfo = &cg_pWeaponModelInfos[ Weapon_None ];
	if( weaponinfo->inuse ) {
		return weaponinfo;
	}

	if( cg_debugWeaponModels->integer ) {
		Com_Printf( "%sWEAPmodel: Failed to load generic weapon. Creating a fake one%s\n", S_COLOR_YELLOW, S_COLOR_WHITE );
	}

	CG_CreateHandDefaultAnimations( weaponinfo );
	weaponinfo->inuse = true;

	return weaponinfo; //no checks
}

void CG_WModelsInit() {
	memset( &cg_pWeaponModelInfos, 0, sizeof( cg_pWeaponModelInfos ) );
}
