#pragma once

#include "qcommon/types.h"

struct ImmediateSoundHandle {
	u64 x;
};

extern Cvar * s_device;

bool S_Init();
void S_Shutdown();

Span< const char * > GetAudioDevices( Allocator * a );

void S_Update( Vec3 origin, Vec3 velocity, const mat3_t axis );

void S_StartFixedSound( StringHash name, Vec3 origin, int channel, float volume, float pitch );
void S_StartEntitySound( StringHash name, int ent_num, int channel, float volume, float pitch );
void S_StartEntitySound( StringHash name, int ent_num, int channel, float volume, float pitch, u32 sfx_entropy );
void S_StartGlobalSound( StringHash name, int channel, float volume, float pitch );
void S_StartGlobalSound( StringHash name, int channel, float volume, float pitch, u32 sfx_entropy );
void S_StartLocalSound( StringHash name, int channel, float volume, float pitch );
void S_StartLineSound( StringHash name, Vec3 start, Vec3 end, int channel, float volume, float pitch );

ImmediateSoundHandle S_ImmediateEntitySound( StringHash name, int ent_num, float volume, float pitch, bool loop, ImmediateSoundHandle handle );
ImmediateSoundHandle S_ImmediateEntitySound( StringHash name, int ent_num, float volume, float pitch, bool loop, u32 sfx_entropy, ImmediateSoundHandle handle );
ImmediateSoundHandle S_ImmediateFixedSound( StringHash name, Vec3 pos, float volume, float pitch, ImmediateSoundHandle handle );
ImmediateSoundHandle S_ImmediateLineSound( StringHash name, Vec3 start, Vec3 end, float volume, float pitch, ImmediateSoundHandle handle );

void S_StopAllSounds( bool stopMusic );

void S_StartMenuMusic();
void S_StopBackgroundTrack();
