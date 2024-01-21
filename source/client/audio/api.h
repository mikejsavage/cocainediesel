#pragma once

#include "qcommon/types.h"
#include "client/audio/types.h"

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

	Optional< u64 > entropy;
};

struct Cvar;
extern Cvar * s_device;

void InitSound();
void ShutdownSound();

Span< const char * > GetAudioDeviceNames( Allocator * a );

void SoundFrame( Vec3 origin, Vec3 velocity, Vec3 forward, Vec3 up );

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
