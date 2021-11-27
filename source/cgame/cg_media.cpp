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

void CG_RegisterMedia() {
	TempAllocator temp = cls.frame_arena.temp();

	for( int i = 0; i < 4; i++ ) {
		cgs.media.sfxWeaponHit[ i ] = StringHash( temp( "sounds/misc/hit_{}", i ) );
	}

	cgs.media.sfxVSaySounds[ Vsay_Sorry ] = "sounds/vsay/sorry";
	cgs.media.sfxVSaySounds[ Vsay_Thanks ] = "sounds/vsay/thanks";
	cgs.media.sfxVSaySounds[ Vsay_GoodGame ] = "sounds/vsay/goodgame";
	cgs.media.sfxVSaySounds[ Vsay_BoomStick ] = "sounds/vsay/boomstick";
	cgs.media.sfxVSaySounds[ Vsay_Acne ] = "sounds/vsay/acne";
	cgs.media.sfxVSaySounds[ Vsay_Valley ] = "sounds/vsay/valley";
	cgs.media.sfxVSaySounds[ Vsay_Mike ] = "sounds/vsay/mike";
	cgs.media.sfxVSaySounds[ Vsay_User ] = "sounds/vsay/user";
	cgs.media.sfxVSaySounds[ Vsay_Guyman ] = "sounds/vsay/guyman";
	cgs.media.sfxVSaySounds[ Vsay_Helena ] = "sounds/vsay/helena";

	for( WeaponType i = 0; i < Weapon_Count; i++ ) {
		cgs.media.shaderWeaponIcon[ i ] = FindMaterial( temp( "weapons/{}/icon", GS_GetWeaponDef( i )->short_name ) );
	}

	for( u8 i = 0; i < Gadget_Count; i++ ) {
		cgs.media.shaderGadgetIcon[ i ] = FindMaterial( temp( "weapons/{}/icon", GetGadgetDef( GadgetType( i ) )->short_name ) );
	}

	cgs.fontNormal = RegisterFont( "fonts/Decalotype-Bold" );
	cgs.fontNormalBold = RegisterFont( "fonts/Decalotype-Black" );
	cgs.fontItalic = RegisterFont( "fonts/Decalotype-BoldItalic" );
	cgs.fontBoldItalic = RegisterFont( "fonts/Decalotype-BlackItalic" );
}
