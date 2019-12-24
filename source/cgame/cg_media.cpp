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
	cgs.media.sfxBulletImpact = FindSoundEffect( "sounds/weapons/bullet_impact" );

	// weapon
	for( int i = 0; i < 4; i++ )
		cgs.media.sfxWeaponHit[i] = FindSoundEffect( va( S_WEAPON_HITS, i ) );
	cgs.media.sfxWeaponKill = FindSoundEffect( S_WEAPON_KILL );
	cgs.media.sfxWeaponHitTeam = FindSoundEffect( S_WEAPON_HIT_TEAM );
	cgs.media.sfxWeaponUp = FindSoundEffect( S_WEAPON_SWITCH );
	cgs.media.sfxWeaponUpNoAmmo = FindSoundEffect( S_WEAPON_NOAMMO );

	cgs.media.sfxTeleportIn = FindSoundEffect( S_TELEPORT );
	cgs.media.sfxTeleportOut = FindSoundEffect( S_TELEPORT );

	// Gunblade sounds
	cgs.media.sfxBladeFleshHit = FindSoundEffect( S_WEAPON_GUNBLADE_HIT_FLESH );
	cgs.media.sfxBladeWallHit = FindSoundEffect( S_WEAPON_GUNBLADE_HIT_WALL );

	// Riotgun sounds :
	cgs.media.sfxRiotgunHit = FindSoundEffect( S_WEAPON_RIOTGUN_HIT );

	// Grenade launcher sounds :
	cgs.media.sfxGrenadeBounce = FindSoundEffect( S_WEAPON_GRENADE_BOUNCE );
	cgs.media.sfxGrenadeExplosion = FindSoundEffect( S_WEAPON_GRENADE_HIT );

	// Rocket launcher sounds :
	cgs.media.sfxRocketLauncherHit = FindSoundEffect( S_WEAPON_ROCKET_HIT );

	// Plasmagun sounds :
	cgs.media.sfxPlasmaHit = FindSoundEffect( S_WEAPON_PLASMAGUN_HIT );

	// Lasergun sounds
	cgs.media.sfxLasergunHum = FindSoundEffect( S_WEAPON_LASERGUN_HUM );
	cgs.media.sfxLasergunStop = FindSoundEffect( S_WEAPON_LASERGUN_STOP );
	cgs.media.sfxLasergunHit = FindSoundEffect( S_WEAPON_LASERGUN_HIT );

	cgs.media.sfxElectroboltHit = FindSoundEffect( S_WEAPON_ELECTROBOLT_HIT );

	cgs.media.sfxSpikesArm = FindSoundEffect( "sounds/spikes/arm" );
	cgs.media.sfxSpikesDeploy = FindSoundEffect( "sounds/spikes/deploy" );
	cgs.media.sfxSpikesGlint = FindSoundEffect( "sounds/spikes/glint" );
	cgs.media.sfxSpikesRetract = FindSoundEffect( "sounds/spikes/retract" );

	cgs.media.sfxFall = FindSoundEffect( "sounds/players/fall" );

	// VSAY sounds
	cgs.media.sfxVSaySounds[VSAY_GENERIC] = FindSoundEffect( S_VSAY_GOODGAME );
	cgs.media.sfxVSaySounds[VSAY_AFFIRMATIVE] = FindSoundEffect( S_VSAY_AFFIRMATIVE );
	cgs.media.sfxVSaySounds[VSAY_NEGATIVE] = FindSoundEffect( S_VSAY_NEGATIVE );
	cgs.media.sfxVSaySounds[VSAY_YES] = FindSoundEffect( S_VSAY_YES );
	cgs.media.sfxVSaySounds[VSAY_NO] = FindSoundEffect( S_VSAY_NO );
	cgs.media.sfxVSaySounds[VSAY_ONDEFENSE] = FindSoundEffect( S_VSAY_ONDEFENSE );
	cgs.media.sfxVSaySounds[VSAY_ONOFFENSE] = FindSoundEffect( S_VSAY_ONOFFENSE );
	cgs.media.sfxVSaySounds[VSAY_OOPS] = FindSoundEffect( S_VSAY_OOPS );
	cgs.media.sfxVSaySounds[VSAY_SORRY] = FindSoundEffect( S_VSAY_SORRY );
	cgs.media.sfxVSaySounds[VSAY_THANKS] = FindSoundEffect( S_VSAY_THANKS );
	cgs.media.sfxVSaySounds[VSAY_NOPROBLEM] = FindSoundEffect( S_VSAY_NOPROBLEM );
	cgs.media.sfxVSaySounds[VSAY_YEEHAA] = FindSoundEffect( S_VSAY_YEEHAA );
	cgs.media.sfxVSaySounds[VSAY_GOODGAME] = FindSoundEffect( S_VSAY_GOODGAME );
	cgs.media.sfxVSaySounds[VSAY_DEFEND] = FindSoundEffect( S_VSAY_DEFEND );
	cgs.media.sfxVSaySounds[VSAY_ATTACK] = FindSoundEffect( S_VSAY_ATTACK );
	cgs.media.sfxVSaySounds[VSAY_NEEDBACKUP] = FindSoundEffect( S_VSAY_NEEDBACKUP );
	cgs.media.sfxVSaySounds[VSAY_BOOO] = FindSoundEffect( S_VSAY_BOOO );
	cgs.media.sfxVSaySounds[VSAY_NEEDDEFENSE] = FindSoundEffect( S_VSAY_NEEDDEFENSE );
	cgs.media.sfxVSaySounds[VSAY_NEEDOFFENSE] = FindSoundEffect( S_VSAY_NEEDOFFENSE );
	cgs.media.sfxVSaySounds[VSAY_NEEDHELP] = FindSoundEffect( S_VSAY_NEEDHELP );
	cgs.media.sfxVSaySounds[VSAY_ROGER] = FindSoundEffect( S_VSAY_ROGER );
	cgs.media.sfxVSaySounds[VSAY_AREASECURED] = FindSoundEffect( S_VSAY_AREASECURED );
	cgs.media.sfxVSaySounds[VSAY_BOOMSTICK] = FindSoundEffect( S_VSAY_BOOMSTICK );
	cgs.media.sfxVSaySounds[VSAY_OK] = FindSoundEffect( S_VSAY_OK );
	cgs.media.sfxVSaySounds[VSAY_SHUTUP] = FindSoundEffect( S_VSAY_SHUTUP );
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
	cgs.media.shaderBombIcon = FindMaterial( "gfx/bomb" );
	cgs.media.shaderTeleportShellGfx = FindMaterial( "gfx/misc/teleportshell" );

	cgs.media.shaderBladeMark = FindMaterial( "gfx/decals/d_blade_hit" );
	cgs.media.shaderBulletMark = FindMaterial( "gfx/decals/d_bullet_hit" );
	cgs.media.shaderExplosionMark = FindMaterial( "gfx/decals/d_explode_hit" );
	cgs.media.shaderPlasmaMark = FindMaterial( "gfx/decals/d_plasma_hit" );
	cgs.media.shaderEBImpact = FindMaterial( "gfx/decals/ebimpact" );

	cgs.media.shaderEBBeam = FindMaterial( "gfx/misc/ebbeam" );
	cgs.media.shaderLGBeam = FindMaterial( "gfx/misc/lgbeam" );
	cgs.media.shaderSMGtrail = FindMaterial( "weapons/SMG/SMGtrail" );
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
	cgs.fontSystemSmallSize = ceilf( SYSTEM_FONT_SMALL_SIZE * scale );
	cgs.fontSystemMediumSize = ceilf( SYSTEM_FONT_MEDIUM_SIZE * scale );
	cgs.fontSystemBigSize = ceilf( SYSTEM_FONT_BIG_SIZE * scale );

	cgs.fontMontserrat = RegisterFont( "fonts/Montserrat-SemiBold" );
	cgs.fontMontserratBold = RegisterFont( "fonts/Montserrat-Bold" );
	cgs.fontMontserratItalic = RegisterFont( "fonts/Montserrat-SemiBoldItalic" );
	cgs.fontMontserratBoldItalic = RegisterFont( "fonts/Montserrat-BoldItalic" );

	scale *= 1.3f;
	cgs.textSizeTiny = SYSTEM_FONT_TINY_SIZE * scale;
	cgs.textSizeSmall = SYSTEM_FONT_SMALL_SIZE * scale;
	cgs.textSizeMedium = SYSTEM_FONT_MEDIUM_SIZE * scale;
	cgs.textSizeBig = SYSTEM_FONT_BIG_SIZE * scale;
}
