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

#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"
#include "client/renderer/text.h"

void CG_RegisterMediaSounds() {
	cgs.media.sfxBulletImpact = FindSoundEffect( "weapons/bullet_impact" );
	cgs.media.sfxBulletWhizz = FindSoundEffect( "weapons/bullet_whizz" );

	// weapon
	for( int i = 0; i < 4; i++ )
		cgs.media.sfxWeaponHit[ i ] = FindSoundEffect( va( S_WEAPON_HITS, i ) );
	cgs.media.sfxWeaponKill = FindSoundEffect( S_WEAPON_KILL );
	cgs.media.sfxWeaponHitTeam = FindSoundEffect( S_WEAPON_HIT_TEAM );
	cgs.media.sfxWeaponNoAmmo = FindSoundEffect( "weapons/noammo" );

	cgs.media.sfxTeleportIn = FindSoundEffect( S_TELEPORT );
	cgs.media.sfxTeleportOut = FindSoundEffect( S_TELEPORT );

	// Gunblade sounds
	cgs.media.sfxBladeFleshHit = FindSoundEffect( "weapons/gb/hit_player" );
	cgs.media.sfxBladeWallHit = FindSoundEffect( "weapons/gb/hit_wall" );

	// Riotgun sounds :
	cgs.media.sfxRiotgunHit = FindSoundEffect( "weapons/rg/hit" );

	// Grenade launcher sounds :
	cgs.media.sfxGrenadeBounce = FindSoundEffect( "weapons/gl/bounce" );
	cgs.media.sfxGrenadeExplosion = FindSoundEffect( "weapons/gl/explode" );

	// Rocket launcher sounds :
	cgs.media.sfxRocketLauncherHit = FindSoundEffect( "weapons/rl/explode" );

	// Plasmagun sounds :
	cgs.media.sfxPlasmaHit = FindSoundEffect( "weapons/pg/explode" );
	cgs.media.sfxBubbleHit = FindSoundEffect( "weapons/bg/explode" );

	// Lasergun sounds
	cgs.media.sfxLasergunHum = FindSoundEffect( "weapons/lg/hum" );
	cgs.media.sfxLasergunBeam = FindSoundEffect( "weapons/lg/beam" );
	cgs.media.sfxLasergunStop = FindSoundEffect( "weapons/lg/stop" );
	cgs.media.sfxLasergunHit = FindSoundEffect( "weapons/lg/hit" );

	cgs.media.sfxElectroboltHit = FindSoundEffect( "weapons/rg/hit" );

	cgs.media.sfxSpikesArm = FindSoundEffect( "sounds/spikes/arm" );
	cgs.media.sfxSpikesDeploy = FindSoundEffect( "sounds/spikes/deploy" );
	cgs.media.sfxSpikesGlint = FindSoundEffect( "sounds/spikes/glint" );
	cgs.media.sfxSpikesRetract = FindSoundEffect( "sounds/spikes/retract" );

	cgs.media.sfxFall = FindSoundEffect( "players/fall" );

	cgs.media.sfxTbag = FindSoundEffect( "sounds/tbag/tbag" );
	cgs.media.sfxSpray = FindSoundEffect( "sounds/spray/spray" );

	cgs.media.sfxHeadshot = FindSoundEffect( "sounds/headshot/headshot" );

	// VSAY sounds
	cgs.media.sfxVSaySounds[ Vsay_Sorry ] = FindSoundEffect( "sounds/vsay/sorry" );
	cgs.media.sfxVSaySounds[ Vsay_Thanks ] = FindSoundEffect( "sounds/vsay/thanks" );
	cgs.media.sfxVSaySounds[ Vsay_GoodGame ] = FindSoundEffect( "sounds/vsay/goodgame" );
	cgs.media.sfxVSaySounds[ Vsay_BoomStick ] = FindSoundEffect( "sounds/vsay/boomstick" );
	cgs.media.sfxVSaySounds[ Vsay_ShutUp ] = FindSoundEffect( "sounds/vsay/mike/shutup" );
	cgs.media.sfxVSaySounds[ Vsay_Bruh ] = FindSoundEffect( "sounds/vsay/mike/bruh" );
	cgs.media.sfxVSaySounds[ Vsay_Cya ] = FindSoundEffect( "sounds/vsay/mike/cya" );
	cgs.media.sfxVSaySounds[ Vsay_GetGood ] = FindSoundEffect( "sounds/vsay/mike/getgood" );
	cgs.media.sfxVSaySounds[ Vsay_HitTheShowers ] = FindSoundEffect( "sounds/vsay/mike/hittheshowers" );
	cgs.media.sfxVSaySounds[ Vsay_Lads ] = FindSoundEffect( "sounds/vsay/mike/lads" );
	cgs.media.sfxVSaySounds[ Vsay_SheDoesntEvenGoHere ] = FindSoundEffect( "sounds/vsay/shedoesntevengohere" );
	cgs.media.sfxVSaySounds[ Vsay_ShitSon ] = FindSoundEffect( "sounds/vsay/mike/shitson" );
	cgs.media.sfxVSaySounds[ Vsay_TrashSmash ] = FindSoundEffect( "sounds/vsay/mike/trashsmash" );
	cgs.media.sfxVSaySounds[ Vsay_WhatTheShit ] = FindSoundEffect( "sounds/vsay/mike/whattheshit" );
	cgs.media.sfxVSaySounds[ Vsay_WowYourTerrible ] = FindSoundEffect( "sounds/vsay/mike/wowyourterrible" );
	cgs.media.sfxVSaySounds[ Vsay_Acne ] = FindSoundEffect( "sounds/vsay/acne" );
	cgs.media.sfxVSaySounds[ Vsay_Valley ] = FindSoundEffect( "sounds/vsay/valley" );
	cgs.media.sfxVSaySounds[ Vsay_Mike ] = FindSoundEffect( "sounds/vsay/mike" );
}

void CG_RegisterMediaModels() {
	cgs.media.modPlasmaExplosion = FindModel( "weapons/pg/impact" );

	cgs.media.modDash = FindModel( "models/effects/dash_burst" );

	cgs.media.modBulletExplode = FindModel( "weapons/mg/impact" );
	cgs.media.modElectroBoltWallHit = FindModel( "weapons/eb/impact" );
	cgs.media.modLasergunWallExplo = FindModel( "weapons/lg/impact" );
	cgs.media.modBladeWallHit = FindModel( "weapons/gb/impact" );

	cgs.media.modGib = FindModel( "models/objects/gibs/gib" );
}

void CG_RegisterMediaShaders() {
	TempAllocator temp = cls.frame_arena.temp();

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

	cgs.media.shaderEBBeam = FindMaterial( "weapons/eb/beam" );
	cgs.media.shaderLGBeam = FindMaterial( "weapons/lg/beam" );
	cgs.media.shaderTracer = FindMaterial( "weapons/tracer" );
	cgs.media.shaderRocketExplosion = FindMaterial( "weapons/rl/explosion" );
	cgs.media.shaderRocketExplosionRing = FindMaterial( "weapons/rl/explosion_ring" );
	cgs.media.shaderGrenadeExplosion = FindMaterial( "weapons/gl/explosion" );
	cgs.media.shaderGrenadeExplosionRing = FindMaterial( "weapons/gl/explosion_ring" );

	cgs.media.shaderLaser = FindMaterial( "gfx/misc/laser" );

	for( WeaponType i = 0; i < Weapon_Count; i++ ) {
		cgs.media.shaderWeaponIcon[ i ] = FindMaterial( temp( "weapons/{}/icon", GS_GetWeaponDef( i )->short_name ) );
	}

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
	cgs.fontMontserrat = RegisterFont( "fonts/Montserrat-SemiBold" );
	cgs.fontMontserratBold = RegisterFont( "fonts/Montserrat-Bold" );
	cgs.fontMontserratItalic = RegisterFont( "fonts/Montserrat-SemiBoldItalic" );
	cgs.fontMontserratBoldItalic = RegisterFont( "fonts/Montserrat-BoldItalic" );
}
