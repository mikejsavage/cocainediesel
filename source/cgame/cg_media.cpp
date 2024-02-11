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
#include "client/renderer/text.h"

void CG_RegisterMedia() {
	TempAllocator temp = cls.frame_arena.temp();

	cgs.media.sfxVSaySounds[ Vsay_Sorry ] = "sounds/vsay/sorry";
	cgs.media.sfxVSaySounds[ Vsay_Thanks ] = "sounds/vsay/thanks";
	cgs.media.sfxVSaySounds[ Vsay_GoodGame ] = "sounds/vsay/goodgame";
	cgs.media.sfxVSaySounds[ Vsay_BoomStick ] = "sounds/vsay/boomstick";
	cgs.media.sfxVSaySounds[ Vsay_Acne ] = "sounds/vsay/acne";
	cgs.media.sfxVSaySounds[ Vsay_Valley ] = "sounds/vsay/valley";
	cgs.media.sfxVSaySounds[ Vsay_Fam ] = "sounds/vsay/fam";
	cgs.media.sfxVSaySounds[ Vsay_Mike ] = "sounds/vsay/mike";
	cgs.media.sfxVSaySounds[ Vsay_User ] = "sounds/vsay/user";
	cgs.media.sfxVSaySounds[ Vsay_Guyman ] = "sounds/vsay/guyman";
	cgs.media.sfxVSaySounds[ Vsay_Dodonga ] = "sounds/vsay/dodonga";
	cgs.media.sfxVSaySounds[ Vsay_Helena ] = "sounds/vsay/helena";
	cgs.media.sfxVSaySounds[ Vsay_Fart ] = "sounds/vsay/fart";
	cgs.media.sfxVSaySounds[ Vsay_Zombie ] = "sounds/vsay/zombie";
	cgs.media.sfxVSaySounds[ Vsay_Larp ] = "sounds/vsay/larp";

	for( WeaponType i = Weapon_None; i < Weapon_Count; i++ ) {
		cgs.media.shaderWeaponIcon[ i ] = StringHash( temp( "weapons/{}/icon", GS_GetWeaponDef( i )->short_name ) );
	}

	for( GadgetType i = Gadget_None; i < Gadget_Count; i++ ) {
		cgs.media.shaderGadgetIcon[ i ] = StringHash( temp( "gadgets/{}/icon", GetGadgetDef( i )->short_name ) );
	}

	for( PerkType i = Perk_None; i < Perk_Count; i++ ) {
		cgs.media.shaderPerkIcon[ i ] = StringHash( temp( "perks/{}/icon", GetPerkDef( i )->short_name ) );
	}

	cgs.fontNormal = RegisterFont( "fonts/Decalotype-Bold" );
	cgs.fontNormalBold = RegisterFont( "fonts/Decalotype-Black" );
	cgs.fontItalic = RegisterFont( "fonts/Decalotype-BoldItalic" );
	cgs.fontBoldItalic = RegisterFont( "fonts/Decalotype-BlackItalic" );
}
