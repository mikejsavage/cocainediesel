/*
Copyright (C) 2002-2003 Victor Luchits

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

#include "cg_local.h"

cgs_media_handle_t *sfx_headnode;

/*
* CG_RegisterMediaSfx
*/
static cgs_media_handle_t *CG_RegisterMediaSfx( const char *name ) {
	cgs_media_handle_t *mediasfx;

	for( mediasfx = sfx_headnode; mediasfx; mediasfx = mediasfx->next ) {
		if( !Q_stricmp( mediasfx->name, name ) ) {
			return mediasfx;
		}
	}

	mediasfx = ( cgs_media_handle_t * )CG_Malloc( sizeof( cgs_media_handle_t ) );
	mediasfx->name = CG_CopyString( name );
	mediasfx->next = sfx_headnode;
	sfx_headnode = mediasfx;

	mediasfx->data = ( void * )S_RegisterSound( mediasfx->name );

	return mediasfx;
}

/*
* CG_MediaSfx
*/
const SoundAsset *CG_MediaSfx( cgs_media_handle_t *mediasfx ) {
	if( !mediasfx->data ) {
		mediasfx->data = ( void * )S_RegisterSound( mediasfx->name );
	}
	return ( const SoundAsset * ) mediasfx->data;
}

/*
* CG_RegisterMediaSounds
*/
void CG_RegisterMediaSounds( void ) {
	int i;

	sfx_headnode = NULL;

	for( i = 0; i < 2; i++ )
		cgs.media.sfxRic[i] = CG_RegisterMediaSfx( va( "sounds/weapons/ric%i", i + 1 ) );

	// weapon
	for( i = 0; i < 4; i++ )
		cgs.media.sfxWeaponHit[i] = CG_RegisterMediaSfx( va( S_WEAPON_HITS, i ) );
	cgs.media.sfxWeaponKill = CG_RegisterMediaSfx( S_WEAPON_KILL );
	cgs.media.sfxWeaponHitTeam = CG_RegisterMediaSfx( S_WEAPON_HIT_TEAM );
	cgs.media.sfxWeaponUp = CG_RegisterMediaSfx( S_WEAPON_SWITCH );
	cgs.media.sfxWeaponUpNoAmmo = CG_RegisterMediaSfx( S_WEAPON_NOAMMO );

	cgs.media.sfxTeleportIn = CG_RegisterMediaSfx( S_TELEPORT );
	cgs.media.sfxTeleportOut = CG_RegisterMediaSfx( S_TELEPORT );

	//	cgs.media.sfxJumpPad = CG_RegisterMediaSfx ( S_JUMPPAD );

	// Gunblade sounds (weak is blade):
	for( i = 0; i < 3; i++ ) cgs.media.sfxGunbladeShot[i] = CG_RegisterMediaSfx( va( S_WEAPON_GUNBLADE_SHOT_1_to_3, i + 1 ) );
	for( i = 0; i < 3; i++ ) cgs.media.sfxBladeFleshHit[i] = CG_RegisterMediaSfx( va( S_WEAPON_GUNBLADE_HIT_FLESH_1_to_3, i + 1 ) );
	for( i = 0; i < 2; i++ ) cgs.media.sfxBladeWallHit[i] = CG_RegisterMediaSfx( va( S_WEAPON_GUNBLADE_HIT_WALL_1_to_2, i + 1 ) );

	// Riotgun sounds :
	cgs.media.sfxRiotgunHit = CG_RegisterMediaSfx( S_WEAPON_RIOTGUN_HIT );

	// Grenade launcher sounds :
	for( i = 0; i < 2; i++ )
		cgs.media.sfxGrenadeBounce[i] = CG_RegisterMediaSfx( va( S_WEAPON_GRENADE_BOUNCE_1_to_2, i + 1 ) );
	cgs.media.sfxGrenadeExplosion = CG_RegisterMediaSfx( S_WEAPON_GRENADE_HIT );

	// Rocket launcher sounds :
	cgs.media.sfxRocketLauncherHit = CG_RegisterMediaSfx( S_WEAPON_ROCKET_HIT );

	// Plasmagun sounds :
	cgs.media.sfxPlasmaHit = CG_RegisterMediaSfx( S_WEAPON_PLASMAGUN_HIT );

	// Lasergun sounds
	cgs.media.sfxLasergunHum = CG_RegisterMediaSfx( S_WEAPON_LASERGUN_HUM );
	cgs.media.sfxLasergunStop = CG_RegisterMediaSfx( S_WEAPON_LASERGUN_STOP );
	cgs.media.sfxLasergunHit[0] = CG_RegisterMediaSfx( S_WEAPON_LASERGUN_HIT_0 );
	cgs.media.sfxLasergunHit[1] = CG_RegisterMediaSfx( S_WEAPON_LASERGUN_HIT_1 );
	cgs.media.sfxLasergunHit[2] = CG_RegisterMediaSfx( S_WEAPON_LASERGUN_HIT_2 );

	cgs.media.sfxElectroboltHit = CG_RegisterMediaSfx( S_WEAPON_ELECTROBOLT_HIT );

	cgs.media.sfxSpikesArm = CG_RegisterMediaSfx( "sounds/spikes/arm" );
	cgs.media.sfxSpikesDeploy = CG_RegisterMediaSfx( "sounds/spikes/deploy" );
	cgs.media.sfxSpikesGlint = CG_RegisterMediaSfx( "sounds/spikes/glint" );
	cgs.media.sfxSpikesRetract = CG_RegisterMediaSfx( "sounds/spikes/retract" );

	// VSAY sounds
	cgs.media.sfxVSaySounds[VSAY_GENERIC] = CG_RegisterMediaSfx( S_VSAY_GOODGAME );
	cgs.media.sfxVSaySounds[VSAY_AFFIRMATIVE] = CG_RegisterMediaSfx( S_VSAY_AFFIRMATIVE );
	cgs.media.sfxVSaySounds[VSAY_NEGATIVE] = CG_RegisterMediaSfx( S_VSAY_NEGATIVE );
	cgs.media.sfxVSaySounds[VSAY_YES] = CG_RegisterMediaSfx( S_VSAY_YES );
	cgs.media.sfxVSaySounds[VSAY_NO] = CG_RegisterMediaSfx( S_VSAY_NO );
	cgs.media.sfxVSaySounds[VSAY_ONDEFENSE] = CG_RegisterMediaSfx( S_VSAY_ONDEFENSE );
	cgs.media.sfxVSaySounds[VSAY_ONOFFENSE] = CG_RegisterMediaSfx( S_VSAY_ONOFFENSE );
	cgs.media.sfxVSaySounds[VSAY_OOPS] = CG_RegisterMediaSfx( S_VSAY_OOPS );
	cgs.media.sfxVSaySounds[VSAY_SORRY] = CG_RegisterMediaSfx( S_VSAY_SORRY );
	cgs.media.sfxVSaySounds[VSAY_THANKS] = CG_RegisterMediaSfx( S_VSAY_THANKS );
	cgs.media.sfxVSaySounds[VSAY_NOPROBLEM] = CG_RegisterMediaSfx( S_VSAY_NOPROBLEM );
	cgs.media.sfxVSaySounds[VSAY_YEEHAA] = CG_RegisterMediaSfx( S_VSAY_YEEHAA );
	cgs.media.sfxVSaySounds[VSAY_GOODGAME] = CG_RegisterMediaSfx( S_VSAY_GOODGAME );
	cgs.media.sfxVSaySounds[VSAY_DEFEND] = CG_RegisterMediaSfx( S_VSAY_DEFEND );
	cgs.media.sfxVSaySounds[VSAY_ATTACK] = CG_RegisterMediaSfx( S_VSAY_ATTACK );
	cgs.media.sfxVSaySounds[VSAY_NEEDBACKUP] = CG_RegisterMediaSfx( S_VSAY_NEEDBACKUP );
	cgs.media.sfxVSaySounds[VSAY_BOOO] = CG_RegisterMediaSfx( S_VSAY_BOOO );
	cgs.media.sfxVSaySounds[VSAY_NEEDDEFENSE] = CG_RegisterMediaSfx( S_VSAY_NEEDDEFENSE );
	cgs.media.sfxVSaySounds[VSAY_NEEDOFFENSE] = CG_RegisterMediaSfx( S_VSAY_NEEDOFFENSE );
	cgs.media.sfxVSaySounds[VSAY_NEEDHELP] = CG_RegisterMediaSfx( S_VSAY_NEEDHELP );
	cgs.media.sfxVSaySounds[VSAY_ROGER] = CG_RegisterMediaSfx( S_VSAY_ROGER );
	cgs.media.sfxVSaySounds[VSAY_AREASECURED] = CG_RegisterMediaSfx( S_VSAY_AREASECURED );
	cgs.media.sfxVSaySounds[VSAY_BOOMSTICK] = CG_RegisterMediaSfx( S_VSAY_BOOMSTICK );
	cgs.media.sfxVSaySounds[VSAY_OK] = CG_RegisterMediaSfx( S_VSAY_OK );
	cgs.media.sfxVSaySounds[VSAY_SHUTUP] = CG_RegisterMediaSfx( S_VSAY_SHUTUP );
}

//======================================================================

cgs_media_handle_t *model_headnode;

/*
* CG_RegisterModel
*/
struct model_s *CG_RegisterModel( const char *name ) {
	struct model_s *model;

	model = trap_R_RegisterModel( name );

	return model;
}

/*
* CG_RegisterMediaModel
*/
static cgs_media_handle_t *CG_RegisterMediaModel( const char *name ) {
	cgs_media_handle_t *mediamodel;

	for( mediamodel = model_headnode; mediamodel; mediamodel = mediamodel->next ) {
		if( !Q_stricmp( mediamodel->name, name ) ) {
			return mediamodel;
		}
	}

	mediamodel = ( cgs_media_handle_t * )CG_Malloc( sizeof( cgs_media_handle_t ) );
	mediamodel->name = CG_CopyString( name );
	mediamodel->next = model_headnode;
	model_headnode = mediamodel;

	mediamodel->data = ( void * )CG_RegisterModel( mediamodel->name );

	return mediamodel;
}

/*
* CG_MediaModel
*/
struct model_s *CG_MediaModel( cgs_media_handle_t *mediamodel ) {
	if( !mediamodel ) {
		return NULL;
	}

	if( !mediamodel->data ) {
		mediamodel->data = ( void * )CG_RegisterModel( mediamodel->name );
	}
	return ( struct model_s * )mediamodel->data;
}

/*
* CG_RegisterMediaModels
*/
void CG_RegisterMediaModels( void ) {
	model_headnode = NULL;

	cgs.media.modPlasmaExplosion = CG_RegisterMediaModel( PATH_PLASMA_EXPLOSION_MODEL );

	cgs.media.modDash = CG_RegisterMediaModel( "models/effects/dash_burst.glb" );

	cgs.media.modBulletExplode = CG_RegisterMediaModel( PATH_BULLET_EXPLOSION_MODEL );
	cgs.media.modElectroBoltWallHit = CG_RegisterMediaModel( PATH_ELECTROBLAST_IMPACT_MODEL );
	cgs.media.modLasergunWallExplo = CG_RegisterMediaModel( PATH_LASERGUN_IMPACT_MODEL );
	cgs.media.modBladeWallHit = CG_RegisterMediaModel( PATH_GUNBLADEBLAST_IMPACT_MODEL );

	// gibs model
	cgs.media.modGib = CG_RegisterMediaModel( "models/objects/gibs/gib.glb" );
}

//======================================================================

cgs_media_handle_t *shader_headnode;

/*
* CG_RegisterMediaShader
*/
static cgs_media_handle_t *CG_RegisterMediaShader( const char *name ) {
	cgs_media_handle_t *mediashader;

	for( mediashader = shader_headnode; mediashader; mediashader = mediashader->next ) {
		if( !Q_stricmp( mediashader->name, name ) ) {
			return mediashader;
		}
	}

	mediashader = ( cgs_media_handle_t * )CG_Malloc( sizeof( cgs_media_handle_t ) );
	mediashader->name = CG_CopyString( name );
	mediashader->next = shader_headnode;
	shader_headnode = mediashader;

	mediashader->data = ( void * )trap_R_RegisterPic( mediashader->name );

	return mediashader;
}

/*
* CG_MediaShader
*/
struct shader_s *CG_MediaShader( cgs_media_handle_t *mediashader ) {
	if( !mediashader->data ) {
		mediashader->data = ( void * )trap_R_RegisterPic( mediashader->name );
	}
	return ( struct shader_s * )mediashader->data;
}

/*
* CG_RegisterMediaShaders
*/
void CG_RegisterMediaShaders( void ) {
	shader_headnode = NULL;

	cgs.media.shaderParticle = CG_RegisterMediaShader( "particle" );

	cgs.media.shaderNet = CG_RegisterMediaShader( "gfx/hud/net" );

	cgs.media.shaderPlayerShadow = CG_RegisterMediaShader( "gfx/decals/shadow" );

	cgs.media.shaderWaterBubble = CG_RegisterMediaShader( "gfx/misc/waterBubble" );
	cgs.media.shaderSmokePuff = CG_RegisterMediaShader( "gfx/misc/smokepuff" );

	cgs.media.shaderSmokePuff1 = CG_RegisterMediaShader( "gfx/misc/smokepuff1" );
	cgs.media.shaderSmokePuff2 = CG_RegisterMediaShader( "gfx/misc/smokepuff2" );
	cgs.media.shaderSmokePuff3 = CG_RegisterMediaShader( "gfx/misc/smokepuff3" );

	cgs.media.shaderRocketFireTrailPuff = CG_RegisterMediaShader( "gfx/misc/strong_rocket_fire" );
	cgs.media.shaderTeleporterSmokePuff = CG_RegisterMediaShader( "TeleporterSmokePuff" );
	cgs.media.shaderGrenadeTrailSmokePuff = CG_RegisterMediaShader( "gfx/grenadetrail_smoke_puf" );
	cgs.media.shaderRocketTrailSmokePuff = CG_RegisterMediaShader( "gfx/misc/rocketsmokepuff" );
	cgs.media.shaderBloodTrailPuff = CG_RegisterMediaShader( "gfx/misc/bloodtrail_puff" );
	cgs.media.shaderBloodTrailLiquidPuff = CG_RegisterMediaShader( "gfx/misc/bloodtrailliquid_puff" );
	cgs.media.shaderBloodImpactPuff = CG_RegisterMediaShader( "gfx/misc/bloodimpact_puff" );
	cgs.media.shaderTeamMateIndicator = CG_RegisterMediaShader( "gfx/indicators/teammate_indicator" );
	cgs.media.shaderTeamCarrierIndicator = CG_RegisterMediaShader( "gfx/indicators/teamcarrier_indicator" );
	cgs.media.shaderBombIcon = CG_RegisterMediaShader( "gfx/bomb" );
	cgs.media.shaderTeleportShellGfx = CG_RegisterMediaShader( "gfx/misc/teleportshell" );

	cgs.media.shaderBladeMark = CG_RegisterMediaShader( "gfx/decals/d_blade_hit" );
	cgs.media.shaderBulletMark = CG_RegisterMediaShader( "gfx/decals/d_bullet_hit" );
	cgs.media.shaderExplosionMark = CG_RegisterMediaShader( "gfx/decals/d_explode_hit" );
	cgs.media.shaderPlasmaMark = CG_RegisterMediaShader( "gfx/decals/d_plasma_hit" );
	cgs.media.shaderEBImpact = CG_RegisterMediaShader( "gfx/decals/ebimpact" );

	cgs.media.shaderEBBeam = CG_RegisterMediaShader( "gfx/misc/ebbeam" );
	cgs.media.shaderLGBeam = CG_RegisterMediaShader( "gfx/misc/lgbeam" );
	cgs.media.shaderRocketExplosion = CG_RegisterMediaShader( PATH_ROCKET_EXPLOSION_SPRITE );
	cgs.media.shaderRocketExplosionRing = CG_RegisterMediaShader( PATH_ROCKET_EXPLOSION_RING_SPRITE );
	cgs.media.shaderGrenadeExplosion = CG_RegisterMediaShader( PATH_ROCKET_EXPLOSION_SPRITE );
	cgs.media.shaderGrenadeExplosionRing = CG_RegisterMediaShader( PATH_ROCKET_EXPLOSION_RING_SPRITE );

	cgs.media.shaderLaser = CG_RegisterMediaShader( "gfx/misc/laser" );

	cgs.media.shaderRaceGhostEffect = CG_RegisterMediaShader( "gfx/raceghost" );

	// Kurim : weapon icons
	cgs.media.shaderWeaponIcon[WEAP_GUNBLADE - 1] = CG_RegisterMediaShader( PATH_GUNBLADE_ICON );
	cgs.media.shaderWeaponIcon[WEAP_MACHINEGUN - 1] = CG_RegisterMediaShader( PATH_MACHINEGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_RIOTGUN - 1] = CG_RegisterMediaShader( PATH_RIOTGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_GRENADELAUNCHER - 1] = CG_RegisterMediaShader( PATH_GRENADELAUNCHER_ICON );
	cgs.media.shaderWeaponIcon[WEAP_ROCKETLAUNCHER - 1] = CG_RegisterMediaShader( PATH_ROCKETLAUNCHER_ICON );
	cgs.media.shaderWeaponIcon[WEAP_PLASMAGUN - 1] = CG_RegisterMediaShader( PATH_PLASMAGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_LASERGUN - 1] = CG_RegisterMediaShader( PATH_LASERGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_ELECTROBOLT - 1] = CG_RegisterMediaShader( PATH_ELECTROBOLT_ICON );

	// Kurim : keyicons
	cgs.media.shaderKeyIcon[KEYICON_FORWARD] = CG_RegisterMediaShader( PATH_KEYICON_FORWARD );
	cgs.media.shaderKeyIcon[KEYICON_BACKWARD] = CG_RegisterMediaShader( PATH_KEYICON_BACKWARD );
	cgs.media.shaderKeyIcon[KEYICON_LEFT] = CG_RegisterMediaShader( PATH_KEYICON_LEFT );
	cgs.media.shaderKeyIcon[KEYICON_RIGHT] = CG_RegisterMediaShader( PATH_KEYICON_RIGHT );
	cgs.media.shaderKeyIcon[KEYICON_FIRE] = CG_RegisterMediaShader( PATH_KEYICON_FIRE );
	cgs.media.shaderKeyIcon[KEYICON_JUMP] = CG_RegisterMediaShader( PATH_KEYICON_JUMP );
	cgs.media.shaderKeyIcon[KEYICON_CROUCH] = CG_RegisterMediaShader( PATH_KEYICON_CROUCH );
	cgs.media.shaderKeyIcon[KEYICON_SPECIAL] = CG_RegisterMediaShader( PATH_KEYICON_SPECIAL );

	cgs.media.shaderAlive = CG_RegisterMediaShader( "gfx/scoreboard/alive" );
	cgs.media.shaderDead = CG_RegisterMediaShader( "gfx/scoreboard/dead" );
	cgs.media.shaderReady = CG_RegisterMediaShader( "gfx/scoreboard/ready" );
}

/*
* CG_RegisterFonts
*/
void CG_RegisterFonts( void ) {
	float scale = ( float )( cgs.vidHeight ) / 600.0f;

	cgs.fontSystemTinySize = ceilf( SYSTEM_FONT_TINY_SIZE * scale );
	cgs.fontSystemTiny = trap_SCR_RegisterFont( SYSTEM_FONT_FAMILY, QFONT_STYLE_NONE, cgs.fontSystemTinySize );
	if( !cgs.fontSystemTiny ) {
		CG_Error( "Couldn't load default font \"%s\"", SYSTEM_FONT_FAMILY );
	}

	cgs.fontSystemSmallSize = ceilf( SYSTEM_FONT_SMALL_SIZE * scale );
	cgs.fontSystemSmall = trap_SCR_RegisterFont( SYSTEM_FONT_FAMILY, QFONT_STYLE_NONE, cgs.fontSystemSmallSize );

	cgs.fontSystemMediumSize = ceilf( SYSTEM_FONT_MEDIUM_SIZE * scale );
	cgs.fontSystemMedium = trap_SCR_RegisterFont( SYSTEM_FONT_FAMILY, QFONT_STYLE_NONE, cgs.fontSystemMediumSize );

	cgs.fontSystemBigSize = ceilf( SYSTEM_FONT_BIG_SIZE * scale );
	cgs.fontSystemBig = trap_SCR_RegisterFont( SYSTEM_FONT_FAMILY, QFONT_STYLE_NONE, cgs.fontSystemBigSize );

	cgs.fontMontserrat = RegisterFont( "fonts/Montserrat-SemiBold" );
	cgs.fontMontserratBold = RegisterFont( "fonts/Montserrat-Bold" );
	cgs.fontMontserratItalic = RegisterFont( "fonts/Montserrat-SemiBoldItalic" );
	cgs.fontMontserratBoldItalic = RegisterFont( "fonts/Montserrat-BoldItalic" );

	scale *= 1.3f;
	cgs.textSizeTiny = SYSTEM_FONT_TINY_SIZE * scale;;
	cgs.textSizeSmall = SYSTEM_FONT_SMALL_SIZE * scale;;
	cgs.textSizeMedium = SYSTEM_FONT_MEDIUM_SIZE * scale;;
	cgs.textSizeBig = SYSTEM_FONT_BIG_SIZE * scale;;
}
