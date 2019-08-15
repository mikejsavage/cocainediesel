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

#if 0
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
	cgs.media.shaderBombIcon = CG_RegisterMediaShader( "gfx/bomb/carriericon" );
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

	cgs.media.shaderTick = CG_RegisterMediaShader( PATH_VSAY_YES_ICON );
}
#endif
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
