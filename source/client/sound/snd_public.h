#pragma once

bool S_Init();
void S_Shutdown();

void S_LoadSoundAssets();

void S_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis );
void S_UpdateEntity( int ent_num, const vec3_t origin, const vec3_t velocity );

void S_SetWindowFocus( bool focused );

void S_StartFixedSound( StringHash sound, const vec3_t origin, int channel, float volume, float attenuation );
void S_StartEntitySound( StringHash sound, int ent_num, int channel, float volume, float attenuation );
void S_StartGlobalSound( StringHash sound, int channel, float volume );
void S_StartLocalSound( StringHash sound, int channel, float volume );
void S_ImmediateSound( StringHash sound, int ent_num, float volume, float attenuation );
void S_StopAllSounds( bool stopMusic );

void S_StartMenuMusic();
void S_StopBackgroundTrack();

void S_BeginAviDemo();
void S_StopAviDemo();
