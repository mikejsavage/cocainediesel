
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
uint64 sndAce;
uint64 sndHurry;

uint64 snd1v1;
uint64 snd1vx;
uint64 sndxv1;

enum Announcement {
	Announcement_RoundStarted,
	Announcement_Planted,
	Announcement_Defused,

	Announcement_Count,
}

uint64[] sndAnnouncementsOff( Announcement_Count );
uint64[] sndAnnouncementsDef( Announcement_Count );

void announce( Announcement announcement ) {
	G_AnnouncerSound( null, sndAnnouncementsOff[ announcement ], attackingTeam, true, null );
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
	sndAce = Hash64( "sounds/announcer/bomb/bongo" );
	sndHurry = Hash64( "sounds/misc/timer_bip_bip" );

	snd1v1 = Hash64( "sounds/announcer/bomb/1v1" );
	snd1vx = Hash64( "sounds/announcer/bomb/1vx" );
	sndxv1 = Hash64( "sounds/announcer/bomb/xv1" );

	sndAnnouncementsOff[ Announcement_RoundStarted ] = Hash64( "sounds/announcer/bomb/offense/start" );
	sndAnnouncementsOff[ Announcement_Planted ] = Hash64( "sounds/announcer/bomb/offense/planted" );
	sndAnnouncementsOff[ Announcement_Defused ] = Hash64( "sounds/announcer/bomb/offense/defused" );

	sndAnnouncementsDef[ Announcement_RoundStarted ] = Hash64( "sounds/announcer/bomb/defense/start" );
	sndAnnouncementsDef[ Announcement_Planted ] = Hash64( "sounds/announcer/bomb/defense/planted" );
	sndAnnouncementsDef[ Announcement_Defused ] = Hash64( "sounds/announcer/bomb/defense/defused" );
}
