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

// helper functions for common PlaySFX patterns
PlaySFXConfig PlaySFXConfigGlobal( float volume = 1.0f );
PlaySFXConfig PlaySFXConfigPosition( Vec3 position, float volume = 1.0f );
PlaySFXConfig PlaySFXConfigEntity( int ent_num, float volume = 1.0f );
PlaySFXConfig PlaySFXConfigLineSegment( Vec3 start, Vec3 end, float volume = 1.0f );

PlayingSFXHandle PlaySFX( StringHash name, const PlaySFXConfig & config = PlaySFXConfigGlobal() );
PlayingSFXHandle PlayImmediateSFX( StringHash name, PlayingSFXHandle handle, const PlaySFXConfig & config );
void StopSFX( PlayingSFXHandle handle );

void StopAllSounds( bool stopMusic );

void StartMenuMusic();
void StopMenuMusic();
