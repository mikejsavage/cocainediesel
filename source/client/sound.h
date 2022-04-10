#pragma once

#include "qcommon/types.h"

struct PlayingSFXHandle {
	u64 handle;
};

enum SpatialisationMethod {
	SpatialisationMethod_None, // plays at max volume everywhere
	SpatialisationMethod_Position, // plays from some point in the world
	SpatialisationMethod_Entity, // moves with an entity
	SpatialisationMethod_LineSegment, // play sound from closest point on a line segment
};

struct PlaySFXConfig {
	SpatialisationMethod spatialisation;
	union {
		Vec3 position;
		int ent_num;
		struct { Vec3 start, end; } line_segment;
	};

	float volume;
	float pitch;

	u64 entropy;
	bool has_entropy;
};

extern Cvar * s_device;

bool InitSound();
void ShutdownSound();

Span< const char * > GetAudioDevices( Allocator * a );

void SoundFrame( Vec3 origin, Vec3 velocity, const mat3_t axis );

PlayingSFXHandle PlaySFX( StringHash name, const PlaySFXConfig & config );
PlayingSFXHandle PlayImmediateSFX( StringHash name, PlayingSFXHandle handle, const PlaySFXConfig & config );
void StopSFX( PlayingSFXHandle handle );

void StopAllSounds( bool stopMusic );

void StartMenuMusic();
void StopMenuMusic();

// TODO: legacy API, should probably stop using these
PlayingSFXHandle S_StartFixedSound( StringHash name, Vec3 position, float volume, float pitch );
PlayingSFXHandle S_StartEntitySound( StringHash name, int ent_num, float volume, float pitch );
PlayingSFXHandle S_StartEntitySound( StringHash name, int ent_num, float volume, float pitch, u64 entropy );
PlayingSFXHandle S_StartGlobalSound( StringHash name, float volume, float pitch );
PlayingSFXHandle S_StartGlobalSound( StringHash name, float volume, float pitch, u64 entropy );
PlayingSFXHandle S_StartLineSound( StringHash name, Vec3 start, Vec3 end, float volume, float pitch );

PlayingSFXHandle S_ImmediateEntitySound( StringHash name, int ent_num, float volume, float pitch, PlayingSFXHandle handle );
PlayingSFXHandle S_ImmediateEntitySound( StringHash name, int ent_num, float volume, float pitch, u64 entropy, PlayingSFXHandle handle );
PlayingSFXHandle S_ImmediateFixedSound( StringHash name, Vec3 pos, float volume, float pitch, PlayingSFXHandle handle );
PlayingSFXHandle S_ImmediateLineSound( StringHash name, Vec3 start, Vec3 end, float volume, float pitch, PlayingSFXHandle handle );
