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
uint64 modelBombModel;
uint64 modelBombModelActive;
uint64 modelBombBackpack;
uint64 modelIndicator;

// SPRITES ETC
uint64 imgBombDecal;

// SOUNDS
uint64 sndBeep;
uint64 sndPlantStart;
uint64 sndGoodGame;
uint64 sndBombTaken;
uint64 sndBongo;

uint64[] sndAnnouncementsOff( Announcement_Count );
uint64[] sndAnnouncementsDef( Announcement_Count );

enum Announcement {
	Announcement_Started,
	Announcement_Armed,
	Announcement_Defused,
	Announcement_Hurry,

	Announcement_Count,
}

void announce( Announcement announcement ) {
	announceOff( announcement );
	announceDef( announcement );
}

void announceOff( Announcement announcement ) {
	G_AnnouncerSound( null, sndAnnouncementsOff[ announcement ], attackingTeam, true, null );
}

void announceDef( Announcement announcement ) {
	G_AnnouncerSound( null, sndAnnouncementsDef[ announcement ], defendingTeam, true, null );
}

void mediaInit() {
	modelBombModel = Hash64( "models/objects/misc/bomb_centered" );
	modelBombModelActive = Hash64( "models/objects/misc/bomb_centered_active" );
	modelBombBackpack = Hash64( "models/objects/misc/bomb" );

	sndBeep = Hash64( "sounds/bomb/bombtimer" );
	sndPlantStart = Hash64( "sounds/misc/timer_bip_bip" );
	sndGoodGame = Hash64( "sounds/vsay/goodgame" );
	sndBombTaken = Hash64( "sounds/announcer/bomb/offense/taken" );
	sndBongo = Hash64( "sounds/announcer/bomb/bongo" );

	sndAnnouncementsOff[ Announcement_Started ] = Hash64( "sounds/announcer/bomb/offense/start" );
	sndAnnouncementsOff[ Announcement_Armed ] = Hash64( "sounds/announcer/bomb/offense/planted" );
	sndAnnouncementsOff[ Announcement_Defused ] = Hash64( "sounds/announcer/bomb/offense/defused" );
	sndAnnouncementsOff[ Announcement_Hurry ] = Hash64( "sounds/misc/timer_bip_bip" );

	sndAnnouncementsDef[ Announcement_Started ] = Hash64( "sounds/announcer/bomb/defense/start" );
	sndAnnouncementsDef[ Announcement_Armed ] = Hash64( "sounds/announcer/bomb/defense/planted" );
	sndAnnouncementsDef[ Announcement_Defused ] = Hash64( "sounds/announcer/bomb/defense/defused" );
	sndAnnouncementsDef[ Announcement_Hurry ] = Hash64( "sounds/misc/timer_bip_bip" );
}
