#pragma once

#include "qcommon/types.h"

struct ImmediateSoundHandle {
	u64 x;
};

extern cvar_t * s_device;

bool S_Init();
void S_Shutdown();

const char * GetAudioDevicesAsSequentialStrings();

void S_Update( Vec3 origin, Vec3 velocity, const mat3_t axis );
void S_UpdateEntity( int ent_num, Vec3 origin, Vec3 velocity );

void S_StartFixedSound( StringHash name, Vec3 origin, int channel, float volume );
void S_StartEntitySound( StringHash name, int ent_num, int channel, float volume );
void S_StartEntitySound( StringHash name, int ent_num, int channel, float volume, u32 sfx_entropy );
void S_StartGlobalSound( StringHash name, int channel, float volume );
void S_StartGlobalSound( StringHash name, int channel, float volume, u32 sfx_entropy );
void S_StartLocalSound( StringHash name, int channel, float volume );
void S_StartLineSound( StringHash name, Vec3 start, Vec3 end, int channel, float volume );

ImmediateSoundHandle S_ImmediateEntitySound( StringHash name, int ent_num, float volume, ImmediateSoundHandle handle );
ImmediateSoundHandle S_ImmediateFixedSound( StringHash name, Vec3 pos, float volume, ImmediateSoundHandle handle );
ImmediateSoundHandle S_ImmediateLineSound( StringHash name, Vec3 start, Vec3 end, float volume, ImmediateSoundHandle handle );

void S_StopAllSounds( bool stopMusic );

void S_StartMenuMusic();
void S_StopBackgroundTrack();
