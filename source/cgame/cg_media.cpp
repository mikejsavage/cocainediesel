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

void CG_RegisterMediaSounds() {
	for( int i = 0; i < 2; i++ )
		cgs.media.sfxRic[i] = S_RegisterSound( va( "sounds/weapons/ric%i", i + 1 ) );

	// weapon
	for( int i = 0; i < 4; i++ )
		cgs.media.sfxWeaponHit[i] = S_RegisterSound( va( S_WEAPON_HITS, i ) );
	cgs.media.sfxWeaponKill = S_RegisterSound( S_WEAPON_KILL );
	cgs.media.sfxWeaponHitTeam = S_RegisterSound( S_WEAPON_HIT_TEAM );
	cgs.media.sfxWeaponUp = S_RegisterSound( S_WEAPON_SWITCH );
	cgs.media.sfxWeaponUpNoAmmo = S_RegisterSound( S_WEAPON_NOAMMO );

	cgs.media.sfxTeleportIn = S_RegisterSound( S_TELEPORT );
	cgs.media.sfxTeleportOut = S_RegisterSound( S_TELEPORT );

	// Gunblade sounds
	for( int i = 0; i < 3; i++ ) cgs.media.sfxBladeFleshHit[i] = S_RegisterSound( va( S_WEAPON_GUNBLADE_HIT_FLESH_1_to_3, i + 1 ) );
	for( int i = 0; i < 2; i++ ) cgs.media.sfxBladeWallHit[i] = S_RegisterSound( va( S_WEAPON_GUNBLADE_HIT_WALL_1_to_2, i + 1 ) );

	// Riotgun sounds :
	cgs.media.sfxRiotgunHit = S_RegisterSound( S_WEAPON_RIOTGUN_HIT );

	// Grenade launcher sounds :
	for( int i = 0; i < 2; i++ )
		cgs.media.sfxGrenadeBounce[i] = S_RegisterSound( va( S_WEAPON_GRENADE_BOUNCE_1_to_2, i + 1 ) );
	cgs.media.sfxGrenadeExplosion = S_RegisterSound( S_WEAPON_GRENADE_HIT );

	// Rocket launcher sounds :
	cgs.media.sfxRocketLauncherHit = S_RegisterSound( S_WEAPON_ROCKET_HIT );

	// Plasmagun sounds :
	cgs.media.sfxPlasmaHit = S_RegisterSound( S_WEAPON_PLASMAGUN_HIT );

	// Lasergun sounds
	cgs.media.sfxLasergunHum = S_RegisterSound( S_WEAPON_LASERGUN_HUM );
	cgs.media.sfxLasergunStop = S_RegisterSound( S_WEAPON_LASERGUN_STOP );
	cgs.media.sfxLasergunHit[0] = S_RegisterSound( S_WEAPON_LASERGUN_HIT_0 );
	cgs.media.sfxLasergunHit[1] = S_RegisterSound( S_WEAPON_LASERGUN_HIT_1 );
	cgs.media.sfxLasergunHit[2] = S_RegisterSound( S_WEAPON_LASERGUN_HIT_2 );

	cgs.media.sfxElectroboltHit = S_RegisterSound( S_WEAPON_ELECTROBOLT_HIT );

	cgs.media.sfxSpikesArm = S_RegisterSound( "sounds/spikes/arm" );
	cgs.media.sfxSpikesDeploy = S_RegisterSound( "sounds/spikes/deploy" );
	cgs.media.sfxSpikesGlint = S_RegisterSound( "sounds/spikes/glint" );
	cgs.media.sfxSpikesRetract = S_RegisterSound( "sounds/spikes/retract" );

	// VSAY sounds
	cgs.media.sfxVSaySounds[VSAY_GENERIC] = S_RegisterSound( S_VSAY_GOODGAME );
	cgs.media.sfxVSaySounds[VSAY_AFFIRMATIVE] = S_RegisterSound( S_VSAY_AFFIRMATIVE );
	cgs.media.sfxVSaySounds[VSAY_NEGATIVE] = S_RegisterSound( S_VSAY_NEGATIVE );
	cgs.media.sfxVSaySounds[VSAY_YES] = S_RegisterSound( S_VSAY_YES );
	cgs.media.sfxVSaySounds[VSAY_NO] = S_RegisterSound( S_VSAY_NO );
	cgs.media.sfxVSaySounds[VSAY_ONDEFENSE] = S_RegisterSound( S_VSAY_ONDEFENSE );
	cgs.media.sfxVSaySounds[VSAY_ONOFFENSE] = S_RegisterSound( S_VSAY_ONOFFENSE );
	cgs.media.sfxVSaySounds[VSAY_OOPS] = S_RegisterSound( S_VSAY_OOPS );
	cgs.media.sfxVSaySounds[VSAY_SORRY] = S_RegisterSound( S_VSAY_SORRY );
	cgs.media.sfxVSaySounds[VSAY_THANKS] = S_RegisterSound( S_VSAY_THANKS );
	cgs.media.sfxVSaySounds[VSAY_NOPROBLEM] = S_RegisterSound( S_VSAY_NOPROBLEM );
	cgs.media.sfxVSaySounds[VSAY_YEEHAA] = S_RegisterSound( S_VSAY_YEEHAA );
	cgs.media.sfxVSaySounds[VSAY_GOODGAME] = S_RegisterSound( S_VSAY_GOODGAME );
	cgs.media.sfxVSaySounds[VSAY_DEFEND] = S_RegisterSound( S_VSAY_DEFEND );
	cgs.media.sfxVSaySounds[VSAY_ATTACK] = S_RegisterSound( S_VSAY_ATTACK );
	cgs.media.sfxVSaySounds[VSAY_NEEDBACKUP] = S_RegisterSound( S_VSAY_NEEDBACKUP );
	cgs.media.sfxVSaySounds[VSAY_BOOO] = S_RegisterSound( S_VSAY_BOOO );
	cgs.media.sfxVSaySounds[VSAY_NEEDDEFENSE] = S_RegisterSound( S_VSAY_NEEDDEFENSE );
	cgs.media.sfxVSaySounds[VSAY_NEEDOFFENSE] = S_RegisterSound( S_VSAY_NEEDOFFENSE );
	cgs.media.sfxVSaySounds[VSAY_NEEDHELP] = S_RegisterSound( S_VSAY_NEEDHELP );
	cgs.media.sfxVSaySounds[VSAY_ROGER] = S_RegisterSound( S_VSAY_ROGER );
	cgs.media.sfxVSaySounds[VSAY_AREASECURED] = S_RegisterSound( S_VSAY_AREASECURED );
	cgs.media.sfxVSaySounds[VSAY_BOOMSTICK] = S_RegisterSound( S_VSAY_BOOMSTICK );
	cgs.media.sfxVSaySounds[VSAY_OK] = S_RegisterSound( S_VSAY_OK );
	cgs.media.sfxVSaySounds[VSAY_SHUTUP] = S_RegisterSound( S_VSAY_SHUTUP );
}

void CG_RegisterMediaModels() {
	cgs.media.modPlasmaExplosion = FindModel( PATH_PLASMA_EXPLOSION_MODEL );

	cgs.media.modDash = FindModel( "models/effects/dash_burst" );

	cgs.media.modBulletExplode = FindModel( PATH_BULLET_EXPLOSION_MODEL );
	cgs.media.modElectroBoltWallHit = FindModel( PATH_ELECTROBLAST_IMPACT_MODEL );
	cgs.media.modLasergunWallExplo = FindModel( PATH_LASERGUN_IMPACT_MODEL );
	cgs.media.modBladeWallHit = FindModel( PATH_GUNBLADEBLAST_IMPACT_MODEL );

	cgs.media.modGib = FindModel( "models/objects/gibs/gib" );
}

void CG_RegisterMediaShaders() {
	cgs.media.shaderParticle = FindMaterial( "particle" );

	cgs.media.shaderNet = FindMaterial( "gfx/hud/net" );

	cgs.media.shaderPlayerShadow = FindMaterial( "gfx/decals/shadow" );

	cgs.media.shaderWaterBubble = FindMaterial( "gfx/misc/waterBubble" );
	cgs.media.shaderSmokePuff = FindMaterial( "gfx/misc/smokepuff" );

	cgs.media.shaderSmokePuff1 = FindMaterial( "gfx/misc/smokepuff1" );
	cgs.media.shaderSmokePuff2 = FindMaterial( "gfx/misc/smokepuff2" );
	cgs.media.shaderSmokePuff3 = FindMaterial( "gfx/misc/smokepuff3" );

	cgs.media.shaderRocketFireTrailPuff = FindMaterial( "gfx/misc/strong_rocket_fire" );
	cgs.media.shaderTeleporterSmokePuff = FindMaterial( "TeleporterSmokePuff" );
	cgs.media.shaderGrenadeTrailSmokePuff = FindMaterial( "gfx/grenadetrail_smoke_puf" );
	cgs.media.shaderRocketTrailSmokePuff = FindMaterial( "gfx/misc/rocketsmokepuff" );
	cgs.media.shaderBloodTrailPuff = FindMaterial( "gfx/misc/bloodtrail_puff" );
	cgs.media.shaderBloodTrailLiquidPuff = FindMaterial( "gfx/misc/bloodtrailliquid_puff" );
	cgs.media.shaderBloodImpactPuff = FindMaterial( "gfx/misc/bloodimpact_puff" );
	cgs.media.shaderTeamMateIndicator = FindMaterial( "gfx/indicators/teammate_indicator" );
	cgs.media.shaderTeamCarrierIndicator = FindMaterial( "gfx/indicators/teamcarrier_indicator" );
	cgs.media.shaderBombIcon = FindMaterial( "gfx/bomb" );
	cgs.media.shaderTeleportShellGfx = FindMaterial( "gfx/misc/teleportshell" );

	cgs.media.shaderBladeMark = FindMaterial( "gfx/decals/d_blade_hit" );
	cgs.media.shaderBulletMark = FindMaterial( "gfx/decals/d_bullet_hit" );
	cgs.media.shaderExplosionMark = FindMaterial( "gfx/decals/d_explode_hit" );
	cgs.media.shaderPlasmaMark = FindMaterial( "gfx/decals/d_plasma_hit" );
	cgs.media.shaderEBImpact = FindMaterial( "gfx/decals/ebimpact" );

	cgs.media.shaderEBBeam = FindMaterial( "gfx/misc/ebbeam" );
	cgs.media.shaderLGBeam = FindMaterial( "gfx/misc/lgbeam" );
	cgs.media.shaderRocketExplosion = FindMaterial( PATH_ROCKET_EXPLOSION_SPRITE );
	cgs.media.shaderRocketExplosionRing = FindMaterial( PATH_ROCKET_EXPLOSION_RING_SPRITE );
	cgs.media.shaderGrenadeExplosion = FindMaterial( PATH_ROCKET_EXPLOSION_SPRITE );
	cgs.media.shaderGrenadeExplosionRing = FindMaterial( PATH_ROCKET_EXPLOSION_RING_SPRITE );

	cgs.media.shaderLaser = FindMaterial( "gfx/misc/laser" );

	cgs.media.shaderRaceGhostEffect = FindMaterial( "gfx/raceghost" );

	cgs.media.shaderWeaponIcon[WEAP_GUNBLADE - 1] = FindMaterial( PATH_GUNBLADE_ICON );
	cgs.media.shaderWeaponIcon[WEAP_MACHINEGUN - 1] = FindMaterial( PATH_MACHINEGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_RIOTGUN - 1] = FindMaterial( PATH_RIOTGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_GRENADELAUNCHER - 1] = FindMaterial( PATH_GRENADELAUNCHER_ICON );
	cgs.media.shaderWeaponIcon[WEAP_ROCKETLAUNCHER - 1] = FindMaterial( PATH_ROCKETLAUNCHER_ICON );
	cgs.media.shaderWeaponIcon[WEAP_PLASMAGUN - 1] = FindMaterial( PATH_PLASMAGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_LASERGUN - 1] = FindMaterial( PATH_LASERGUN_ICON );
	cgs.media.shaderWeaponIcon[WEAP_ELECTROBOLT - 1] = FindMaterial( PATH_ELECTROBOLT_ICON );

	cgs.media.shaderKeyIcon[KEYICON_FORWARD] = FindMaterial( PATH_KEYICON_FORWARD );
	cgs.media.shaderKeyIcon[KEYICON_BACKWARD] = FindMaterial( PATH_KEYICON_BACKWARD );
	cgs.media.shaderKeyIcon[KEYICON_LEFT] = FindMaterial( PATH_KEYICON_LEFT );
	cgs.media.shaderKeyIcon[KEYICON_RIGHT] = FindMaterial( PATH_KEYICON_RIGHT );
	cgs.media.shaderKeyIcon[KEYICON_FIRE] = FindMaterial( PATH_KEYICON_FIRE );
	cgs.media.shaderKeyIcon[KEYICON_JUMP] = FindMaterial( PATH_KEYICON_JUMP );
	cgs.media.shaderKeyIcon[KEYICON_CROUCH] = FindMaterial( PATH_KEYICON_CROUCH );
	cgs.media.shaderKeyIcon[KEYICON_SPECIAL] = FindMaterial( PATH_KEYICON_SPECIAL );

	cgs.media.shaderAlive = FindMaterial( "gfx/scoreboard/alive" );
	cgs.media.shaderDead = FindMaterial( "gfx/scoreboard/dead" );
	cgs.media.shaderReady = FindMaterial( "gfx/scoreboard/ready" );
}

void CG_RegisterFonts() {
	float scale = ( float )( frame_static.viewport_height ) / 600.0f;

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
