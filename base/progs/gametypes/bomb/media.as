uint64 modelBomb;

uint64 sndPlant;
uint64 sndFuse;
uint64 sndFuseExtinguished;
uint64 sndComedy;
uint64 sndBombTaken;
uint64 sndAce;

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
	modelBomb = Hash64( "models/bomb/bomb" );

	sndPlant = Hash64( "models/bomb/plant" );
	sndFuse = Hash64( "models/bomb/fuse" );
	sndFuseExtinguished = Hash64( "models/bomb/tss" );
	sndComedy = Hash64( "sounds/vsay/comedy" );
	sndBombTaken = Hash64( "sounds/announcer/bomb/offense/taken" );
	sndAce = Hash64( "sounds/announcer/bomb/ace" );

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
