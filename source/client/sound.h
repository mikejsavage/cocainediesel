#pragma once

#include "qcommon/types.h"

#define AL_LIBTYPE_STATIC
#include "openal/al.h"


struct ImmediateSoundHandle {
	u64 x;
};

struct Sound {
	ALuint buf;
	bool mono;
};

struct SoundEffect {
	struct PlaybackConfig {
		StringHash sounds[ 128 ];
		u8 num_random_sounds;

		float delay;
		float volume;
		float pitch;
		float pitch_random;
		float attenuation;
	};

	PlaybackConfig sounds[ 8 ];
	u8 num_sounds;
};

enum PlayingSoundType {
	PlayingSoundType_Global, // plays at max volume everywhere
	PlayingSoundType_Position, // plays from some point in the world
	PlayingSoundType_Entity, // moves with an entity
	PlayingSoundType_Line, // play sound from closest point on a line
};

struct PlayingSound {
	PlayingSoundType type;
	const SoundEffect * sfx;
	s64 start_time;
	int ent_num;
	int channel;
	float volume;
	float pitch;

	u32 entropy;
	bool has_entropy;

	ImmediateSoundHandle immediate_handle;
	bool touched_since_last_update;

	Vec3 origin;
	Vec3 end;

	ALuint sources[ ARRAY_COUNT( &SoundEffect::sounds ) ];
	bool started[ ARRAY_COUNT( &SoundEffect::sounds ) ];
	bool stopped[ ARRAY_COUNT( &SoundEffect::sounds ) ];
};

struct EntitySound {
	Vec3 origin;
	Vec3 velocity;
};

extern cvar_t * s_device;

bool S_Init();
void S_Shutdown();

const char * GetAudioDevicesAsSequentialStrings();

void S_Update( Vec3 origin, Vec3 velocity, const mat3_t axis );
void S_UpdateEntity( int ent_num, Vec3 origin, Vec3 velocity );

PlayingSound * S_StartFixedSound( StringHash name, Vec3 origin, int channel, float volume, float pitch );
PlayingSound * S_StartEntitySound( StringHash name, int ent_num, int channel, float volume, float pitch );
PlayingSound * S_StartEntitySound( StringHash name, int ent_num, int channel, float volume, float pitch, u32 sfx_entropy );
PlayingSound * S_StartGlobalSound( StringHash name, int channel, float volume, float pitch );
PlayingSound * S_StartGlobalSound( StringHash name, int channel, float volume, float pitch, u32 sfx_entropy );
PlayingSound * S_StartLocalSound( StringHash name, int channel, float volume, float pitch );
PlayingSound * S_StartLineSound( StringHash name, Vec3 start, Vec3 end, int channel, float volume, float pitch );

ImmediateSoundHandle S_ImmediateEntitySound( StringHash name, int ent_num, float volume, float pitch, ImmediateSoundHandle handle );
ImmediateSoundHandle S_ImmediateFixedSound( StringHash name, Vec3 pos, float volume, float pitch, ImmediateSoundHandle handle );
ImmediateSoundHandle S_ImmediateLineSound( StringHash name, Vec3 start, Vec3 end, float volume, float pitch, ImmediateSoundHandle handle );

void S_StopAllSounds( bool stopMusic );

void S_StartMenuMusic();
void S_StopBackgroundTrack();
