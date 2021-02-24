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
	TempAllocator temp = cls.frame_arena.temp();

	for( int i = 0; i < 4; i++ ) {
		cgs.media.sfxWeaponHit[ i ] = StringHash( temp( "sounds/misc/hit_{}", i ) );
	}

	cgs.media.sfxVSaySounds[ Vsay_Sorry ] = "sounds/vsay/sorry";
	cgs.media.sfxVSaySounds[ Vsay_Thanks ] = "sounds/vsay/thanks";
	cgs.media.sfxVSaySounds[ Vsay_GoodGame ] = "sounds/vsay/goodgame";
	cgs.media.sfxVSaySounds[ Vsay_BoomStick ] = "sounds/vsay/boomstick";
	cgs.media.sfxVSaySounds[ Vsay_ShutUp ] = "sounds/vsay/mike/shutup";
	cgs.media.sfxVSaySounds[ Vsay_Bruh ] = "sounds/vsay/mike/bruh";
	cgs.media.sfxVSaySounds[ Vsay_Cya ] = "sounds/vsay/mike/cya";
	cgs.media.sfxVSaySounds[ Vsay_GetGood ] = "sounds/vsay/mike/getgood";
	cgs.media.sfxVSaySounds[ Vsay_HitTheShowers ] = "sounds/vsay/mike/hittheshowers";
	cgs.media.sfxVSaySounds[ Vsay_Lads ] = "sounds/vsay/mike/lads";
	cgs.media.sfxVSaySounds[ Vsay_SheDoesntEvenGoHere ] = "sounds/vsay/shedoesntevengohere";
	cgs.media.sfxVSaySounds[ Vsay_ShitSon ] = "sounds/vsay/mike/shitson";
	cgs.media.sfxVSaySounds[ Vsay_TrashSmash ] = "sounds/vsay/mike/trashsmash";
	cgs.media.sfxVSaySounds[ Vsay_WhatTheShit ] = "sounds/vsay/mike/whattheshit";
	cgs.media.sfxVSaySounds[ Vsay_WowYourTerrible ] = "sounds/vsay/mike/wowyourterrible";
	cgs.media.sfxVSaySounds[ Vsay_Acne ] = "sounds/vsay/acne";
	cgs.media.sfxVSaySounds[ Vsay_Valley ] = "sounds/vsay/valley";
	cgs.media.sfxVSaySounds[ Vsay_Mike ] = "sounds/vsay/mike";
	cgs.media.sfxVSaySounds[ Vsay_User ] = "sounds/vsay/user";
	cgs.media.sfxVSaySounds[ Vsay_Guyman ] = "sounds/vsay/guyman";
	cgs.media.sfxVSaySounds[ Vsay_Helena ] = "sounds/vsay/helena";
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

	cgs.media.shaderAlive = FindMaterial( "gfx/scoreboard/alive" );
	cgs.media.shaderDead = FindMaterial( "gfx/scoreboard/dead" );
	cgs.media.shaderReady = FindMaterial( "gfx/scoreboard/ready" );
}

void CG_RegisterFonts() {
	cgs.fontNormal = RegisterFont( "fonts/Decalotype-Bold" );
	cgs.fontNormalBold = RegisterFont( "fonts/Decalotype-Black" );
	cgs.fontNormalItalic = RegisterFont( "fonts/Decalotype-BoldItalic" );
	cgs.fontNormalBoldItalic = RegisterFont( "fonts/Decalotype-BlackItalic" );
}
