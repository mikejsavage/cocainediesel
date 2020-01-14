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

static weaponinfo_t cg_pWeaponModelInfos[ Weapon_Count + 1 ];

static const char *wmPartSufix[] = { "", "_flash", "_hand", "_barrel", NULL };

/*
* CG_vWeap_ParseAnimationScript
*
* script:
* 0 = first frame
* 1 = lastframe/number of frames
* 2 = looping frames
* 3 = frame time
*
* keywords:
* "islastframe":Will read the second value of each animation as lastframe (usually means numframes)
* "rotationscale": value witch will scale the barrel rotation speed
* "ammoCounter": digital ammo counter parameters: font_family font_size digit_width digit_height digit_alpha icon_size icon_alpha
*/
static bool CG_vWeap_ParseAnimationScript( weaponinfo_t *weaponinfo, const char *filename ) {
	uint8_t *buf;
	char *ptr, *token;
	int rounder, counter, i;
	bool debug = true;
	int anim_data[4][VWEAP_MAXANIMS];
	int length, filenum;

	rounder = 0;
	counter = 1; // reserve 0 for 'no animation'

	// set some defaults
	weaponinfo->barrelSpeed = 0;
	weaponinfo->flashFade = true;

	if( !cg_debugWeaponModels->integer ) {
		debug = false;
	}

	// load the file
	length = FS_FOpenFile( filename, &filenum, FS_READ );
	if( length == -1 ) {
		return false;
	}
	if( !length ) {
		FS_FCloseFile( filenum );
		return false;
	}
	buf = ( uint8_t * )CG_Malloc( length + 1 );
	FS_Read( buf, length, filenum );
	FS_FCloseFile( filenum );

	if( !buf ) {
		CG_Free( buf );
		return false;
	}

	if( debug ) {
		Com_Printf( "%sLoading weapon animation script:%s%s\n", S_COLOR_BLUE, filename, S_COLOR_WHITE );
	}

	memset( anim_data, 0, sizeof( anim_data ) );

	//proceed
	ptr = ( char * )buf;
	while( ptr ) {
		token = COM_ParseExt( &ptr, true );
		if( !token[0] ) {
			break;
		}

		//see if it is keyword or number
		if( *token < '0' || *token > '9' ) {
			if( !Q_stricmp( token, "barrel" ) ) {
				if( debug ) {
					Com_Printf( "%sScript: barrel:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				// time
				i = atoi( COM_ParseExt( &ptr, false ) );
				weaponinfo->barrelTime = (unsigned int)( i > 0 ? i : 0 );

				// speed
				weaponinfo->barrelSpeed = atof( COM_ParseExt( &ptr, false ) );

				if( debug ) {
					Com_Printf( "%s time:%" PRIi64 ", speed:%.2f%s\n", S_COLOR_BLUE, weaponinfo->barrelTime, weaponinfo->barrelSpeed, S_COLOR_WHITE );
				}
			} else if( !Q_stricmp( token, "flash" ) ) {
				if( debug ) {
					Com_Printf( "%sScript: flash:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				// time
				i = atoi( COM_ParseExt( &ptr, false ) );
				weaponinfo->flashTime = (unsigned int)( i > 0 ? i : 0 );

				// radius
				i = atoi( COM_ParseExt( &ptr, false ) );
				weaponinfo->flashRadius = (float)( i > 0 ? i : 0 );

				// fade
				token = COM_ParseExt( &ptr, false );
				if( !Q_stricmp( token, "no" ) ) {
					weaponinfo->flashFade = false;
				}

				if( debug ) {
					Com_Printf( "%s time:%i, radius:%i, fade:%s%s\n", S_COLOR_BLUE, (int)weaponinfo->flashTime, (int)weaponinfo->flashRadius, weaponinfo->flashFade ? "YES" : "NO", S_COLOR_WHITE );
				}
			} else if( !Q_stricmp( token, "flashColor" ) ) {
				if( debug ) {
					Com_Printf( "%sScript: flashColor:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				weaponinfo->flashColor[0] = atof( token = COM_ParseExt( &ptr, false ) );
				weaponinfo->flashColor[1] = atof( token = COM_ParseExt( &ptr, false ) );
				weaponinfo->flashColor[2] = atof( token = COM_ParseExt( &ptr, false ) );

				if( debug ) {
					Com_Printf( "%s%f %f %f%s\n", S_COLOR_BLUE,
							   weaponinfo->flashColor[0], weaponinfo->flashColor[1], weaponinfo->flashColor[2],
							   S_COLOR_WHITE );
				}
			} else if( !Q_stricmp( token, "handOffset" ) ) {
				if( debug ) {
					Com_Printf( "%sScript: handPosition:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				weaponinfo->handpositionOrigin[FORWARD] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionOrigin[RIGHT] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionOrigin[UP] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionAngles[PITCH] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionAngles[YAW] = atof( COM_ParseExt( &ptr, false ) );
				weaponinfo->handpositionAngles[ROLL] = atof( COM_ParseExt( &ptr, false ) );

				if( debug ) {
					Com_Printf( "%s%f %f %f %f %f %f%s\n", S_COLOR_BLUE,
							   weaponinfo->handpositionOrigin[0], weaponinfo->handpositionOrigin[1], weaponinfo->handpositionOrigin[2],
							   weaponinfo->handpositionAngles[0], weaponinfo->handpositionAngles[1], weaponinfo->handpositionAngles[2],
							   S_COLOR_WHITE );
				}

			} else if( !Q_stricmp( token, "firesound" ) ) {
				if( debug ) {
					Com_Printf( "%sScript: firesound:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				}

				token = COM_ParseExt( &ptr, false );
				weaponinfo->sound_fire = FindSoundEffect( token );
				if( debug ) {
					Com_Printf( "%s%s%s\n", S_COLOR_BLUE, token, S_COLOR_WHITE );
				}
			} else if( token[0] && debug ) {
				Com_Printf( "%signored: %s%s\n", S_COLOR_YELLOW, token, S_COLOR_WHITE );
			}
		} else {
			//frame & animation values
			i = (int)atoi( token );
			if( debug ) {
				if( rounder == 0 ) {
					Com_Printf( "%sScript: %s", S_COLOR_BLUE, S_COLOR_WHITE );
				}
				Com_Printf( "%s%i - %s", S_COLOR_BLUE, i, S_COLOR_WHITE );
			}
			anim_data[rounder][counter] = i;
			rounder++;
			if( rounder > 3 ) {
				rounder = 0;
				if( debug ) {
					Com_Printf( "%s anim: %i%s\n", S_COLOR_BLUE, counter, S_COLOR_WHITE );
				}
				counter++;
				if( counter == VWEAP_MAXANIMS ) {
					break;
				}
			}
		}
	}

	CG_Free( buf );

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
static void CG_CreateHandDefaultAnimations( weaponinfo_t *weaponinfo ) {
	float defaultfps = 15.0f;

	weaponinfo->barrelSpeed = 0;

	// default wsw hand
	weaponinfo->firstframe[WEAPANIM_STANDBY] = 0;
	weaponinfo->lastframe[WEAPANIM_STANDBY] = 0;
	weaponinfo->loopingframes[WEAPANIM_STANDBY] = 1;
	weaponinfo->frametime[WEAPANIM_STANDBY] = 1000 / defaultfps;

	weaponinfo->firstframe[WEAPANIM_ATTACK_WEAK] = 1; // attack animation (1-5)
	weaponinfo->lastframe[WEAPANIM_ATTACK_WEAK] = 5;
	weaponinfo->loopingframes[WEAPANIM_ATTACK_WEAK] = 0;
	weaponinfo->frametime[WEAPANIM_ATTACK_WEAK] = 1000 / defaultfps;

	weaponinfo->firstframe[WEAPANIM_ATTACK_STRONG] = 0;
	weaponinfo->lastframe[WEAPANIM_ATTACK_STRONG] = 0;
	weaponinfo->loopingframes[WEAPANIM_ATTACK_STRONG] = 1;
	weaponinfo->frametime[WEAPANIM_ATTACK_STRONG] = 1000 / defaultfps;

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
* CG_ComputeWeaponInfoTags
*
* Store the orientation_t closer to the tag_flash we can create,
* or create one using an offset we consider acceptable.
*
* NOTE: This tag will ignore weapon models animations. You'd have to
* do it in realtime to use it with animations. Or be careful on not
* moving the weapon too much
*/
static void CG_ComputeWeaponInfoTags( weaponinfo_t *weaponinfo ) {
	VectorSet( weaponinfo->tag_projectionsource.origin, 16, 0, 8 );
	Matrix3_Identity( weaponinfo->tag_projectionsource.axis );

	if( !weaponinfo->model[WEAPMODEL_WEAPON] ) {
		return;
	}

	// try getting the tag_flash from the weapon model
	entity_t ent = { };
	ent.scale = 1.0f;
	ent.model = weaponinfo->model[WEAPMODEL_WEAPON];

	CG_GrabTag( &weaponinfo->tag_projectionsource, &ent, "tag_flash" );
}

/*
* CG_WeaponModelUpdateRegistration
*/
static void CG_WeaponModelUpdateRegistration( weaponinfo_t *weaponinfo, char *filename ) {
	for( int p = 0; p < VWEAP_MAXPARTS; p++ ) {
		if( !weaponinfo->model[p] ) {
			char scratch[MAX_QPATH];
			snprintf( scratch, sizeof( scratch ), "models/weapons/%s%s.md3", filename, wmPartSufix[p] );
			weaponinfo->model[p] = FindModel( scratch );
		}
	}

	// load animation script for the hand model
	char scratch[MAX_QPATH];
	snprintf( scratch, sizeof( scratch ), "models/weapons/%s.cfg", filename );
	if( !CG_vWeap_ParseAnimationScript( weaponinfo, scratch ) ) {
		CG_CreateHandDefaultAnimations( weaponinfo );
	}

	// load failed
	if( !weaponinfo->model[WEAPMODEL_HAND] ) {
		weaponinfo->name[0] = 0;
		for( int p = 0; p < VWEAP_MAXPARTS; p++ )
			weaponinfo->model[p] = NULL;
		return;
	}

	// create a tag_projection from tag_flash, to position fire effects
	CG_ComputeWeaponInfoTags( weaponinfo );

	if( cg_debugWeaponModels->integer ) {
		Com_Printf( "%sWEAPmodel: Loaded successful%s\n", S_COLOR_BLUE, S_COLOR_WHITE );
	}

	Q_strncpyz( weaponinfo->name, filename, sizeof( weaponinfo->name ) );
}

/*
* CG_RegisterWeaponModel
*/
struct weaponinfo_s *CG_RegisterWeaponModel( char *cgs_name, WeaponType weaponTag ) {
	char filename[MAX_QPATH];
	Q_strncpyz( filename, cgs_name, sizeof( filename ) );
	COM_StripExtension( filename );

	weaponinfo_t * weaponinfo = &cg_pWeaponModelInfos[ weaponTag ];
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
struct weaponinfo_s *CG_CreateWeaponZeroModel() {
	weaponinfo_t * weaponinfo = &cg_pWeaponModelInfos[ Weapon_Count ];
	if( weaponinfo->inuse ) {
		return weaponinfo;
	}

	if( cg_debugWeaponModels->integer ) {
		Com_Printf( "%sWEAPmodel: Failed to load generic weapon. Creating a fake one%s\n", S_COLOR_YELLOW, S_COLOR_WHITE );
	}

	CG_CreateHandDefaultAnimations( weaponinfo );
	weaponinfo->inuse = true;

	Q_strncpyz( weaponinfo->name, "", sizeof( weaponinfo->name ) );

	return weaponinfo; //no checks
}


//======================================================================
//							weapons
//======================================================================

/*
* CG_AddWeaponFlashOnTag
*/
static void CG_AddWeaponFlashOnTag( entity_t *weapon, const weaponinfo_t *weaponInfo,
	const char *tag_flash, int effects, int64_t flash_time ) {
	uint8_t c;
	orientation_t tag;
	float intensity;

	if( flash_time < cl.serverTime ) {
		return;
	}
	if( !weaponInfo->model[WEAPMODEL_FLASH] ) {
		return;
	}
	if( !CG_GrabTag( &tag, weapon, tag_flash ) ) {
		return;
	}

	if( weaponInfo->flashFade ) {
		intensity = (float)( flash_time - cl.serverTime ) / (float)weaponInfo->flashTime;
		c = ( uint8_t )( 255 * intensity );
	} else {
		intensity = 1.0f;
		c = 255;
	}

	entity_t flash = { };
	flash.color = RGBA8( 255, 255, 255, c );
	flash.model = weaponInfo->model[WEAPMODEL_FLASH];
	flash.scale = weapon->scale;

	CG_PlaceModelOnTag( &flash, weapon, &tag );

	if( !( effects & EF_RACEGHOST ) ) {
		CG_AddEntityToScene( &flash );
	}

	// CG_AddLightToScene( flash.origin, weaponInfo->flashRadius * intensity,
	// 	weaponInfo->flashColor[0], weaponInfo->flashColor[1], weaponInfo->flashColor[2] );
}

/*
* CG_AddWeaponBarrelOnTag
*/
static void CG_AddWeaponBarrelOnTag( entity_t *weapon, const weaponinfo_t *weaponInfo,
	const char *tag_barrel, int effects, int64_t barrel_time ) {
	orientation_t tag;
	vec3_t rotangles = { 90, 0, 0 }; // hack to fix gb blade orientation

	if( !weaponInfo->model[WEAPMODEL_BARREL] ) {
		return;
	}
	if( !CG_GrabTag( &tag, weapon, tag_barrel ) ) {
		return;
	}

	entity_t barrel = { };
	barrel.color = rgba8_white;
	barrel.model = weaponInfo->model[WEAPMODEL_BARREL];
	barrel.scale = weapon->scale;

	// rotation
	if( barrel_time > cl.serverTime ) {
		float intensity;

		intensity =  (float)( barrel_time - cl.serverTime ) / (float)weaponInfo->barrelTime;
		rotangles[2] = 360.0f * weaponInfo->barrelSpeed * intensity * intensity;
	}

	AnglesToAxis( rotangles, barrel.axis );

	// barrel requires special tagging
	CG_PlaceRotatedModelOnTag( &barrel, weapon, &tag );

	CG_AddOutline( &barrel, effects, RGBA8( 0, 0, 0, weapon->color.a ) );

	if( !( effects & EF_RACEGHOST ) )
		CG_AddEntityToScene( &barrel );
}

/*
* CG_AddWeaponOnTag
*
* Add weapon model(s) positioned at the tag
*/
void CG_AddWeaponOnTag( entity_t *ent, const orientation_t *tag, int weaponid, int effects,
	orientation_t *projectionSource, int64_t flash_time, int64_t barrel_time ) {
	const weaponinfo_t * weaponInfo = cgs.weaponInfos[ weaponid ];

	entity_t weapon = { };
	weapon.color = rgba8_white;
	weapon.scale = ent->scale;
	weapon.model = weaponInfo->model[WEAPMODEL_WEAPON];

	CG_PlaceModelOnTag( &weapon, ent, tag );

	CG_AddOutline( &weapon, effects, rgba8_black );

	if( !( effects & EF_RACEGHOST ) ) {
		CG_AddEntityToScene( &weapon );
	}

	if( !weapon.model ) {
		return;
	}

	// update projection source
	if( projectionSource != NULL ) {
		VectorCopy( vec3_origin, projectionSource->origin );
		Matrix3_Copy( axis_identity, projectionSource->axis );
		CG_MoveToTag( projectionSource->origin, projectionSource->axis,
					  weapon.origin, weapon.axis,
					  weaponInfo->tag_projectionsource.origin,
					  weaponInfo->tag_projectionsource.axis );
	}

	CG_AddWeaponBarrelOnTag( &weapon, weaponInfo, "tag_barrel", effects, barrel_time );
	CG_AddWeaponFlashOnTag( &weapon, weaponInfo, "tag_flash", effects, flash_time );
}

void CG_WModelsInit() {
	memset( &cg_pWeaponModelInfos, 0, sizeof( cg_pWeaponModelInfos ) );
}
