/*
Copyright (C) 2009-2010 Chasseur de bots

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

// MODELS
int modelBombModel;
int modelBombModelActive;
int modelBombBackpack;
int modelIndicator;

// SPRITES ETC
int imgBombDecal;

// SOUNDS
int sndBeep;
int sndPlantStart;
int sndGoodGame;
int sndBombTaken;
int sndBongo;

int[] sndAnnouncementsOff( Announcement_Count );
int[] sndAnnouncementsDef( Announcement_Count );

enum Announcement {
	Announcement_Started,
	Announcement_Armed,
	Announcement_Defused,
	Announcement_Hurry,

	Announcement_Count,
}

const uint MSG_ALIVE_ALPHA = CS_GENERAL;
const uint MSG_ALIVE_BETA = CS_GENERAL + 1;
const uint MSG_TOTAL_ALPHA = CS_GENERAL + 2;
const uint MSG_TOTAL_BETA = CS_GENERAL + 3;

void announce( Announcement announcement ) {
	announceOff( announcement );
	announceDef( announcement );
}

void announceOff( Announcement announcement ) {
	if( sndAnnouncementsOff[announcement] != 0 ) {
		G_AnnouncerSound( null, sndAnnouncementsOff[announcement], attackingTeam, true, null );
	}
}

void announceDef( Announcement announcement ) {
	if( sndAnnouncementsDef[announcement] != 0 ) {
		G_AnnouncerSound( null, sndAnnouncementsDef[announcement], defendingTeam, true, null );
	}
}

void mediaInit() {
	modelBombModel = G_ModelIndex( "models/objects/misc/bomb_centered", true );
	modelBombModelActive = G_ModelIndex( "models/objects/misc/bomb_centered_active", true );
	modelBombBackpack = G_ModelIndex( "models/objects/misc/bomb", true );

	imgBombDecal = G_ImageIndex( "gfx/indicators/radar_decal" );

	sndBeep = G_SoundIndex( "sounds/bomb/bombtimer", false );
	sndPlantStart = G_SoundIndex( "sounds/misc/timer_bip_bip", false );
	sndGoodGame = G_SoundIndex( "sounds/vsay/goodgame", false );
	sndBombTaken = G_SoundIndex( "sounds/announcer/bomb/offense/taken", false );
	sndBongo = G_SoundIndex( "sounds/announcer/bomb/bongo", false );

	sndAnnouncementsOff[ Announcement_Started ] = G_SoundIndex( "sounds/announcer/bomb/offense/start", false );
	sndAnnouncementsOff[ Announcement_Armed ] = G_SoundIndex( "sounds/announcer/bomb/offense/planted", false );
	sndAnnouncementsOff[ Announcement_Defused ] = G_SoundIndex( "sounds/announcer/bomb/offense/defused", false );
	sndAnnouncementsOff[ Announcement_Hurry ] = G_SoundIndex( "sounds/misc/timer_bip_bip", false );

	sndAnnouncementsDef[ Announcement_Started ] = G_SoundIndex( "sounds/announcer/bomb/defense/start", false );
	sndAnnouncementsDef[ Announcement_Armed ] = G_SoundIndex( "sounds/announcer/bomb/defense/planted", false );
	sndAnnouncementsDef[ Announcement_Defused ] = G_SoundIndex( "sounds/announcer/bomb/defense/defused", false );
	sndAnnouncementsDef[ Announcement_Hurry ] = G_SoundIndex( "sounds/misc/timer_bip_bip", false );
}
