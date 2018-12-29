#include "qcommon/qcommon.h"
#include "qcommon/assets.h"
#include "qalgo/hashtable.h"
#include "snd_public.h"

#include "openal/al.h"
#include "openal/alc.h"
#include "openal/alext.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.h"

static cvar_t * s_volume;
static cvar_t * s_musicvolume;

static ALCdevice * alDevice;
static ALCcontext * alContext;

struct SoundAsset {
	int length_ms;
	ALuint buffer;
};

enum SoundType {
	SoundType_Global, // plays at max volume everywhere
	SoundType_Fixed, // plays from some point in the world
	SoundType_Attached, // moves with an entity
	SoundType_AttachedImmediate, // moves with an entity and loops so long as is gets touched every frame
};

struct PlayingSound {
	ALuint source;

	SoundType type;
	bool touched_since_last_update;
	int ent_num;
	int channel;
};

struct EntitySound {
	vec3_t origin;
	vec3_t velocity;
	PlayingSound * ps;
};

static SoundAsset sound_assets[ 4096 ];
static size_t num_sound_assets;

static Hashtable< 8192 > sound_assets_hashtable;

static PlayingSound playing_sounds[ 128 ];
static size_t num_playing_sounds;

static ALuint music_source;
static bool music_playing;

static bool window_focused;

static EntitySound entities[ MAX_EDICTS ];

const char *S_ErrorMessage( ALenum error ) {
	switch( error ) {
		case AL_NO_ERROR:
			return "No error";
		case AL_INVALID_NAME:
			return "Invalid name";
		case AL_INVALID_ENUM:
			return "Invalid enumerator";
		case AL_INVALID_VALUE:
			return "Invalid value";
		case AL_INVALID_OPERATION:
			return "Invalid operation";
		case AL_OUT_OF_MEMORY:
			return "Out of memory";
		default:
			return "Unknown error";
	}
}

static void S_ALAssert() {
	ALenum err = alGetError();
	if( err != AL_NO_ERROR ) {
		Sys_Error( "%s", S_ErrorMessage( err ) );
	}
}

static bool S_InitAL() {
	al_device = alcOpenDevice( NULL );
	if( al_device == NULL ) {
		Com_Printf( S_COLOR_RED "Failed to open device\n" );
		return false;
	}

	ALCint attrs[] = { ALC_HRTF_SOFT, ALC_HRTF_ENABLED_SOFT, 0 };
	al_context = alcCreateContext( al_device, attrs );
	if( al_context == NULL ) {
		alcCloseDevice( al_device );
		Com_Printf( S_COLOR_RED "Failed to create context\n" );
		return false;
	}
	alcMakeContextCurrent( al_context );

	alDopplerFactor( 1.0f );
	alDopplerVelocity( 10976.0f );
	alSpeedOfSound( 10976.0f );

	alDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );

	for( size_t i = 0; i < ARRAY_COUNT( playing_sounds ); i++ ) {
		alGenSources( 1, &playing_sounds[ i ].source );
	}
	alGenSources( 1, &music_source );

	if( alGetError() != AL_NO_ERROR ) {
		Com_Printf( S_COLOR_RED "Failed to allocate sound sources\n" );
		S_Shutdown();
		return false;
	}

	Com_Printf( "OpenAL initialized\n" );

	return true;
}

static void S_Register( const char * filename, bool allow_stereo ) {
	assert( num_sound_assets < ARRAY_COUNT( sound_assets ) );
	SoundAsset * sfx = &sound_assets[ num_sound_assets ];

	size_t compressed_len;
	const uint8_t * compressed_data = ( const uint8_t * ) Assets_Data( StringHash( filename ), &compressed_len );
	if( compressed_data == NULL ) {
		Com_Printf( S_COLOR_YELLOW "No such file: %s\n", filename );
		return;
	}

	int channels, sample_rate;
	int16_t * data;
	int num_samples = stb_vorbis_decode_memory( compressed_data, compressed_len, &channels, &sample_rate, &data );
	if( channels == -1 ) {
		Com_Printf( S_COLOR_RED "Couldn't decode sound %s\n", filename );
		return;
	}

	if( !allow_stereo && channels != 1 ) {
		Com_Printf( S_COLOR_RED "Couldn't load sound %s: needs to be a mono file!\n", filename );
		free( data );
		return;
	}

	sfx->length_ms = ( ( int64_t ) num_samples * 1000 ) / sample_rate;

	ALenum format = channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
	alGenBuffers( 1, &sfx->buffer );
	alBufferData( sfx->buffer, format, data, num_samples * channels * sizeof( int16_t ), sample_rate );
	S_ALAssert();

	free( data );

	sound_assets_hashtable.add( Hash64( filename, strlen( filename ) - strlen( ".ogg" ) ), num_sound_assets ); 
	num_sound_assets++;

	return;
}

bool S_Init() {
	num_sound_assets = 0;
	num_playing_sounds = 0;
	music_playing = false;
	window_focused = true;
	initialized = false;

	memset( entities, 0, sizeof( entities ) );

	s_volume = Cvar_Get( "s_volume", "0.8", CVAR_ARCHIVE );
	s_musicvolume = Cvar_Get( "s_musicvolume", "1", CVAR_ARCHIVE );
	s_muteinbackground = Cvar_Get( "s_muteinbackground", "1", CVAR_ARCHIVE );
	s_muteinbackground->modified = true;

	return S_InitAL();
}

void S_LoadSoundAssets() {
	for( const char * name : Assets_Names() ) {
		const char * ext = COM_FileExtension( name );
		if( ext == NULL || strcmp( ext, ".ogg" ) != 0 )
			continue;

		bool stereo = strcmp( name, "sounds/music/menu_1.ogg" ) == 0;
		S_Register( name, stereo );
	}
}

void S_Shutdown() {
	if( !initialized )
		return;

	S_StopAllSounds( true );

	for( size_t i = 0; i < ARRAY_COUNT( playing_sounds ); i++ ) {
		alDeleteSources( 1, &playing_sounds[ i ].source );
	}
	alDeleteSources( 1, &music_source );

	for( size_t i = 0; i < num_sound_assets; i++ ) {
		alDeleteBuffers( 1, &sound_assets[ i ].buffer );
	}

	S_ALAssert();

	alcDestroyContext( alContext );
	alcCloseDevice( alDevice );
}

static void swap( PlayingSound * a, PlayingSound * b ) {
	PlayingSound t = *a;
	*a = *b;
	*b = t;
}

void S_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis ) {
	if( !initialized )
		return;

	if( s_muteinbackground->modified ) {
		alListenerf( AL_GAIN, window_focused || s_muteinbackground->integer == 0 ? 1 : 0 );
		s_muteinbackground->modified = false;
	}

	float orientation[ 6 ];
	VectorCopy( &axis[ AXIS_FORWARD ], &orientation[ 0 ] );
	VectorCopy( &axis[ AXIS_UP ], &orientation[ 3 ] );

	alListenerfv( AL_POSITION, origin );
	alListenerfv( AL_VELOCITY, velocity );
	alListenerfv( AL_ORIENTATION, orientation );

	for( size_t i = 0; i < num_playing_sounds; i++ ) {
		PlayingSound * ps = &playing_sounds[ i ];

		ALint state;
		alGetSourcei( ps->source, AL_SOURCE_STATE, &state );
		bool not_touched = ps->type == SoundType_AttachedImmediate && !ps->touched_since_last_update;
		ps->touched_since_last_update = false;

		if( not_touched || state == AL_STOPPED ) {
			// stop the current sound
			alSourceStop( ps->source );
			if( ps->type == SoundType_AttachedImmediate && ps->ent_num >= 0 )
				entities[ ps->ent_num ].ps = NULL;

			// remove-swap it from the playing sounds array
			num_playing_sounds--;
			swap( ps, &playing_sounds[ num_playing_sounds ] );

			// fix up the ent->ps pointer for the sound that got swapped in
			if( ps->type == SoundType_AttachedImmediate && ps->ent_num >= 0 )
				entities[ ps->ent_num ].ps = ps;

			i--;
			continue;
		}

		if( s_volume->modified )
			alSourcef( ps->source, AL_GAIN, s_volume->value );

		if( ps->type == SoundType_Attached || ps->type == SoundType_AttachedImmediate ) {
			alSourcefv( ps->source, AL_POSITION, entities[ ps->ent_num ].origin );
			alSourcefv( ps->source, AL_VELOCITY, entities[ ps->ent_num ].velocity );
		}
	}

	if( ( s_volume->modified || s_musicvolume->modified ) && music_playing )
		alSourcef( music_source, AL_GAIN, s_volume->value * s_musicvolume->value );

	s_volume->modified = false;
	s_musicvolume->modified = false;

	S_ALAssert();
}

void S_UpdateEntity( int ent_num, const vec3_t origin, const vec3_t velocity ) {
	if( !initialized )
		return;

	VectorCopy( origin, entities[ ent_num ].origin );
	VectorCopy( velocity, entities[ ent_num ].velocity );
}

void S_SetWindowFocus( bool focused ) {
	if( !initialized )
		return;

	window_focused = focused;
	alListenerf( AL_GAIN, window_focused || s_muteinbackground->integer == 0 ? 1 : 0 );
}

static PlayingSound * S_FindEmptyPlayingSound( int ent_num, int channel ) {
	for( size_t i = 0; i < num_playing_sounds; i++ ) {
		PlayingSound * ps = &playing_sounds[ i ];
		if( channel && ps->ent_num == ent_num && ps->channel == channel ) {
			ALint state;
			alGetSourcei( ps->source, AL_SOURCE_STATE, &state );
			if( state != AL_INITIAL )
				alSourceStop( ps->source );
			return ps;
		}
	}

	if( num_playing_sounds == ARRAY_COUNT( playing_sounds ) )
		return NULL;

	num_playing_sounds++;
	return &playing_sounds[ num_playing_sounds - 1 ];
}

static const SoundAsset * S_HashToAsset( StringHash sound ) {
	uint64_t idx;
	if( !sound_assets_hashtable.get( sound.hash, &idx ) )
		return NULL;
	return &sound_assets[ idx ];
}

static bool S_StartSound( StringHash sound, const vec3_t origin, int ent_num, int channel, float volume, float attenuation, SoundType type ) {
	const SoundAsset * asset = S_HashToAsset( sound );
	if( asset == NULL ) {
		printf( "Can't find sound %s %lu\n", sound.str, sound.hash );
		return false;
	}

	PlayingSound * ps = S_FindEmptyPlayingSound( ent_num, channel );
	if( ps == NULL ) {
		Com_Printf( S_COLOR_RED "Too many playing sounds!" );
		return false;
	}

	alSourcei( ps->source, AL_BUFFER, asset->buffer );
	alSourcef( ps->source, AL_GAIN, volume * s_volume->value );
	alSourcef( ps->source, AL_REFERENCE_DISTANCE, S_DEFAULT_ATTENUATION_REFDISTANCE );
	alSourcef( ps->source, AL_MAX_DISTANCE, S_DEFAULT_ATTENUATION_MAXDISTANCE );
	alSourcef( ps->source, AL_ROLLOFF_FACTOR, attenuation );

	ps->type = type;
	ps->ent_num = ent_num;
	ps->channel = channel;

	switch( type ) {
		case SoundType_Global:
			alSourcefv( ps->source, AL_POSITION, vec3_origin );
			alSourcefv( ps->source, AL_VELOCITY, vec3_origin );
			alSourcei( ps->source, AL_LOOPING, AL_FALSE );
			alSourcei( ps->source, AL_SOURCE_RELATIVE, AL_TRUE );
			break;

		case SoundType_Fixed:
			alSourcefv( ps->source, AL_POSITION, origin );
			alSourcefv( ps->source, AL_VELOCITY, vec3_origin );
			alSourcei( ps->source, AL_LOOPING, AL_FALSE );
			alSourcei( ps->source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;

		case SoundType_Attached:
			alSourcefv( ps->source, AL_POSITION, entities[ ent_num ].origin );
			alSourcefv( ps->source, AL_VELOCITY, entities[ ent_num ].velocity );
			alSourcei( ps->source, AL_LOOPING, AL_FALSE );
			alSourcei( ps->source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;

		case SoundType_AttachedImmediate:
			entities[ ent_num ].ps = ps;
			alSourcefv( ps->source, AL_POSITION, entities[ ent_num ].origin );
			alSourcefv( ps->source, AL_VELOCITY, entities[ ent_num ].velocity );
			alSourcei( ps->source, AL_LOOPING, AL_TRUE );
			alSourcei( ps->source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;
	}

	alSourcePlay( ps->source );

	S_ALAssert();

	return true;
}

void S_StartFixedSound( StringHash sound, const vec3_t origin, int channel, float volume, float attenuation ) {
	S_StartSound( sound, origin, 0, channel, volume, attenuation, SoundType_Fixed );
}

void S_StartEntitySound( StringHash sound, int ent_num, int channel, float volume, float attenuation ) {
	S_StartSound( sound, NULL, ent_num, channel, volume, attenuation, SoundType_Attached );
}

void S_StartGlobalSound( StringHash sound, int channel, float volume ) {
	S_StartSound( sound, NULL, 0, channel, volume, 0, SoundType_Global );
}

void S_StartLocalSound( StringHash sound, int channel, float volume ) {
	S_StartSound( sound, NULL, -1, channel, volume, 0, SoundType_Global );
}

void S_ImmediateSound( StringHash sound, int ent_num, float volume, float attenuation ) {
	// TODO: replace old immediate sound if sfx changed
	if( entities[ ent_num ].ps == NULL ) {
		bool started = S_StartSound( sound, NULL, ent_num, -1, volume, attenuation, SoundType_AttachedImmediate );
		if( !started )
			return;
	}
	entities[ ent_num ].ps->touched_since_last_update = true;
}

void S_StopAllSounds( bool stop_music ) {
	if( !initialized )
		return;

	for( size_t i = 0; i < num_playing_sounds; i++ ) {
		alSourceStop( playing_sounds[ i ].source );
	}
	num_playing_sounds = 0;

	if( stop_music )
		S_StopBackgroundTrack();
}

void S_StartMenuMusic() {
	const SoundAsset * asset = S_HashToAsset( "sounds/music/menu_1.ogg" );
	if( !initialized || asset == NULL )
		return;

	alSourcefv( music_source, AL_POSITION, vec3_origin );
	alSourcefv( music_source, AL_VELOCITY, vec3_origin );
	alSourcef( music_source, AL_GAIN, s_musicvolume->value );
	alSourcei( music_source, AL_SOURCE_RELATIVE, AL_FALSE );
	alSourcei( music_source, AL_LOOPING, AL_TRUE );
	alSourcei( music_source, AL_BUFFER, asset->buffer );

	alSourcePlay( music_source );

	music_playing = true;
}

void S_StopBackgroundTrack() {
	if( initialized && music_playing )
		alSourceStop( music_source );
	music_playing = false;
}

void S_BeginAviDemo() {
	// TODO
}

void S_StopAviDemo() {
	// TODO
}
