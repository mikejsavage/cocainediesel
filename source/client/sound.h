#pragma once

struct SoundAsset;

bool S_Init();
void S_Shutdown();

const SoundAsset * S_RegisterSound( const char * filename );

void S_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis );
void S_UpdateEntity( int ent_num, const vec3_t origin, const vec3_t velocity );

void S_SetWindowFocus( bool focused );

void S_StartFixedSound( const SoundAsset * sfx, const vec3_t origin, int channel, float volume, float attenuation );
void S_StartEntitySound( const SoundAsset * sfx, int ent_num, int channel, float volume, float attenuation );
void S_StartGlobalSound( const SoundAsset * sfx, int channel, float volume );
void S_StartLocalSound( const SoundAsset * sfx, int channel, float volume );
void S_ImmediateSound( const SoundAsset * sfx, int ent_num, float volume, float attenuation );
void S_StopAllSounds( bool stopMusic );

void S_StartMenuMusic();
void S_StopBackgroundTrack();
