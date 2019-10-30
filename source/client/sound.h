#pragma once

struct Sound;

struct SoundEffect {
	struct PlaybackConfig {
		StringHash sounds[ 4 ];
		u8 num_random_sounds;

		float delay;
		float volume;
		float attenuation;
	};

	PlaybackConfig sounds[ 8 ];
	u8 num_sounds;
};

bool S_Init();
void S_Shutdown();

const SoundEffect * FindSoundEffect( StringHash name );
const SoundEffect * FindSoundEffect( const char * name );

void S_Update( Vec3 origin, Vec3 velocity, const mat3_t axis );
void S_UpdateEntity( int ent_num, Vec3 origin, Vec3 velocity );

void S_SetWindowFocus( bool focused );

void S_StartFixedSound( const SoundEffect * sfx, Vec3 origin, int channel, float volume, float attenuation );
void S_StartEntitySound( const SoundEffect * sfx, int ent_num, int channel, float volume, float attenuation );
void S_StartGlobalSound( const SoundEffect * sfx, int channel, float volume );
void S_StartLocalSound( const SoundEffect * sfx, int channel, float volume );
void S_ImmediateSound( const SoundEffect * sfx, int ent_num, float volume, float attenuation );
void S_StopAllSounds( bool stopMusic );

void S_StartMenuMusic();
void S_StopBackgroundTrack();
