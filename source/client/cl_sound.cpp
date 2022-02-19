#include <algorithm> // std::sort

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/hash.h"
#include "qcommon/array.h"
#include "qcommon/hashtable.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/sound.h"
#include "client/threadpool.h"
#include "cgame/cg_local.h"
#include "gameshared/gs_public.h"

#define AL_LIBTYPE_STATIC
#include "openal/al.h"
#include "openal/alc.h"
#include "openal/alext.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.h"

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
	StringHash hash;
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
	bool loop;

	Vec3 origin;
	Vec3 end;

	ALuint sources[ ARRAY_COUNT( &SoundEffect::sounds ) ];
	bool started[ ARRAY_COUNT( &SoundEffect::sounds ) ];
	bool stopped[ ARRAY_COUNT( &SoundEffect::sounds ) ];
};

static ALCdevice * al_device;
static ALCcontext * al_context;

// so we don't crash when some other application is running in exclusive playback mode (WASAPI/JACK/etc)
static bool initialized;

Cvar * s_device;
static Cvar * s_volume;
static Cvar * s_musicvolume;
static Cvar * s_muteinbackground;

constexpr u32 MAX_SOUND_ASSETS = 4096;
constexpr u32 MAX_SOUND_EFFECTS = 4096;
constexpr u32 MAX_PLAYING_SOUNDS = 256;

static Sound sounds[ MAX_SOUND_ASSETS ];
static u32 num_sounds;
static Hashtable< MAX_SOUND_ASSETS * 2 > sounds_hashtable;

static SoundEffect sound_effects[ MAX_SOUND_EFFECTS ];
static u32 num_sound_effects;
static Hashtable< MAX_SOUND_EFFECTS * 2 > sound_effects_hashtable;

static ALuint free_sound_sources[ MAX_PLAYING_SOUNDS ];
static u32 num_free_sound_sources;

static PlayingSound playing_sound_effects[ MAX_PLAYING_SOUNDS ];
static u32 num_playing_sound_effects;

static Hashtable< MAX_PLAYING_SOUNDS * 2 > immediate_sounds_hashtable;
static u64 immediate_sounds_autoinc;

static ALuint music_source;
static bool music_playing;

constexpr float MusicIsWayTooLoud = 0.25f;

const char * ALErrorMessage( ALenum error ) {
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

template< typename... Rest >
static void CheckALErrors( const char * fmt, const Rest & ... rest ) {
	ALenum err = alGetError();
	if( err != AL_NO_ERROR ) {
		char buf[ 1024 ];
		ggformat( buf, sizeof( buf ), fmt, rest... );

		if( is_public_build ) {
			Com_Printf( S_COLOR_RED "AL error: %s\n", buf );
		}
		else {
			Fatal( "AL error: %s", buf );
		}
	}
}

static void CheckedALListener( ALenum param, float x ) {
	alListenerf( param, x );
	CheckALErrors( "alListenerf( {}, {} )", param, x );
}

static void CheckedALListener( ALenum param, Vec3 v ) {
	alListenerfv( param, v.ptr() );
	CheckALErrors( "alListenerfv( {}, {} )", param, v );
}

static void CheckedALListener( ALenum param, const mat3_t m ) {
	float forward_and_up[ 6 ];
	forward_and_up[ 0 ] = m[ AXIS_FORWARD ];
	forward_and_up[ 1 ] = m[ AXIS_FORWARD + 1 ];
	forward_and_up[ 2 ] = m[ AXIS_FORWARD + 2 ];
	forward_and_up[ 3 ] = m[ AXIS_UP ];
	forward_and_up[ 4 ] = m[ AXIS_UP + 1 ];
	forward_and_up[ 5 ] = m[ AXIS_UP + 2 ];
	alListenerfv( param, forward_and_up );
	CheckALErrors( "alListenerfv( {}, {}/{} )", param, FromQFAxis( m, AXIS_FORWARD ), FromQFAxis( m, AXIS_UP ) );
}

static ALint CheckedALGetSource( ALuint source, ALenum param ) {
	ALint res;
	alGetSourcei( source, param, &res );
	CheckALErrors( "alGetSourcei( {}, {} )", source, param );
	return res;
}

static void CheckedALSource( ALuint source, ALenum param, ALint x ) {
	alSourcei( source, param, x );
	CheckALErrors( "alSourcei( {}, {}, {} )", source, param, x );
}

static void CheckedALSource( ALuint source, ALenum param, ALuint x ) {
	alSourcei( source, param, x );
	CheckALErrors( "alSourcei( {}, {}, {} )", source, param, x );
}

static void CheckedALSource( ALuint source, ALenum param, float x ) {
	alSourcef( source, param, x );
	CheckALErrors( "alSourcef( {}, {}, {} )", source, param, x );
}

static void CheckedALSource( ALuint source, ALenum param, Vec3 v ) {
	alSourcefv( source, param, v.ptr() );
	CheckALErrors( "alSourcefv( {}, {}, {} )", source, param, v );
}

static void CheckedALSourcePlay( ALuint source ) {
	alSourcePlay( source );
	CheckALErrors( "alSourcePlay( {} )", source );
}

static void CheckedALSourceStop( ALuint source ) {
	alSourceStop( source );
	CheckALErrors( "alSourceStop( {} )", source );
}

static bool S_InitAL() {
	TracyZoneScoped;

	al_device = NULL;

	if( !StrEqual( s_device->value, "" ) ) {
		al_device = alcOpenDevice( s_device->value );
		if( al_device == NULL ) {
			Com_Printf( S_COLOR_YELLOW "Failed to open sound device %s, trying default\n", s_device->value );
		}
	}

	if( al_device == NULL ) {
		al_device = alcOpenDevice( NULL );

		if( al_device == NULL ) {
			Com_Printf( S_COLOR_RED "Failed to open device\n" );
			return false;
		}
	}

	ALCint attrs[] = {
		ALC_HRTF_SOFT, ALC_HRTF_ENABLED_SOFT,
		ALC_MONO_SOURCES, 256,
		ALC_STEREO_SOURCES, 16,
		0
	};
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

	alGenSources( ARRAY_COUNT( free_sound_sources ), free_sound_sources );
	alGenSources( 1, &music_source );
	num_free_sound_sources = ARRAY_COUNT( free_sound_sources );

	if( alGetError() != AL_NO_ERROR ) {
		Com_Printf( S_COLOR_RED "Failed to allocate sound sources\n" );
		S_Shutdown();
		return false;
	}

	return true;
}

struct DecodeSoundJob {
	struct {
		const char * path;
		Span< const u8 > ogg;
	} in;

	struct {
		int channels;
		int sample_rate;
		int num_samples;
		s16 * samples;
	} out;
};

static void AddSound( const char * path, int num_samples, int channels, int sample_rate, s16 * samples ) {
	TracyZoneScoped;
	TracyZoneText( path, strlen( path ) );

	if( num_samples == -1 ) {
		Com_Printf( S_COLOR_RED "Couldn't decode sound %s\n", path );
		return;
	}

	u64 hash = Hash64( path, strlen( path ) - strlen( ".ogg" ) );

	bool restart_music = false;

	u64 idx = num_sounds;
	if( !sounds_hashtable.get( hash, &idx ) ) {
		assert( num_sounds < ARRAY_COUNT( sounds ) );
		assert( num_sound_effects < ARRAY_COUNT( sound_effects ) );

		sounds_hashtable.add( hash, num_sounds );
		num_sounds++;

		// add simple sound effect
		SoundEffect sfx = { };
		sfx.sounds[ 0 ].sounds[ 0 ] = StringHash( hash );
		sfx.sounds[ 0 ].volume = 1;
		sfx.sounds[ 0 ].pitch = 1.0f;
		sfx.sounds[ 0 ].pitch_random = 0.0f;
		sfx.sounds[ 0 ].attenuation = ATTN_NORM;
		sfx.sounds[ 0 ].num_random_sounds = 1;
		sfx.num_sounds = 1;

		sound_effects[ num_sound_effects ] = sfx;
		sound_effects_hashtable.add( hash, num_sound_effects );
		num_sound_effects++;
	}
	else {
		restart_music = music_playing;
		S_StopAllSounds( true );
		alDeleteBuffers( 1, &sounds[ idx ].buf );
	}

	ALenum format = channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
	alGenBuffers( 1, &sounds[ idx ].buf );
	alBufferData( sounds[ idx ].buf, format, samples, num_samples * channels * sizeof( s16 ), sample_rate );
	CheckALErrors( "AddSound" );

	sounds[ idx ].mono = channels == 1;

	if( restart_music ) {
		S_StartMenuMusic();
	}

	free( samples );
}

static void LoadSounds() {
	TracyZoneScoped;

	DynamicArray< DecodeSoundJob > jobs( sys_allocator );
	{
		TracyZoneScopedN( "Build job list" );

		for( const char * path : AssetPaths() ) {
			if( FileExtension( path ) == ".ogg" ) {
				DecodeSoundJob job;
				job.in.path = path;
				job.in.ogg = AssetBinary( path );

				jobs.add( job );
			}
		}

		std::sort( jobs.begin(), jobs.end(), []( const DecodeSoundJob & a, const DecodeSoundJob & b ) {
			return a.in.ogg.n > b.in.ogg.n;
		} );
	}

	ParallelFor( jobs.span(), []( TempAllocator * temp, void * data ) {
		DecodeSoundJob * job = ( DecodeSoundJob * ) data;

		TracyZoneScopedN( "stb_vorbis_decode_memory" );
		TracyZoneText( job->in.path, strlen( job->in.path ) );

		job->out.num_samples = stb_vorbis_decode_memory( job->in.ogg.ptr, job->in.ogg.num_bytes(), &job->out.channels, &job->out.sample_rate, &job->out.samples );
	} );

	for( DecodeSoundJob job : jobs ) {
		AddSound( job.in.path, job.out.num_samples, job.out.channels, job.out.sample_rate, job.out.samples );
	}
}

static void HotloadSounds() {
	TracyZoneScoped;

	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".ogg" ) {
			Span< const u8 > ogg = AssetBinary( path );

			int num_samples, channels, sample_rate;
			s16 * samples;
			{
				TracyZoneScopedN( "stb_vorbis_decode_memory" );
				TracyZoneText( path, strlen( path ) );
				num_samples = stb_vorbis_decode_memory( ogg.ptr, ogg.num_bytes(), &channels, &sample_rate, &samples );
			}

			AddSound( path, num_samples, channels, sample_rate, samples );
		}
	}
}

static bool ParseSoundEffect( SoundEffect * sfx, Span< const char > * data, u64 base_hash ) {
	if( sfx->num_sounds == ARRAY_COUNT( sfx->sounds ) ) {
		Com_Printf( S_COLOR_YELLOW "SFX with too many sections\n" );
		return false;
	}

	while( true ) {
		Span< const char > opening_brace = ParseToken( data, Parse_DontStopOnNewLine );
		if( opening_brace == "" )
			break;

		if( opening_brace != "{" ) {
			Com_Printf( S_COLOR_YELLOW "Expected {\n" );
			return false;
		}


		SoundEffect::PlaybackConfig * config = &sfx->sounds[ sfx->num_sounds ];
		config->volume = 1.0f;
		config->pitch = 1.0f;
		config->pitch_random = 0.0f;
		config->attenuation = ATTN_NORM;

		while( true ) {
			Span< const char > key = ParseToken( data, Parse_DontStopOnNewLine );
			Span< const char > value = ParseToken( data, Parse_StopOnNewLine );

			if( key == "}" ) {
				break;
			}

			if( key == "" || value == "" ) {
				Com_Printf( S_COLOR_YELLOW "Missing key/value" );
				return false;
			}

			if( key == "sound" ) {
				if( config->num_random_sounds == ARRAY_COUNT( config->sounds ) ) {
					Com_Printf( S_COLOR_YELLOW "SFX with too many random sounds\n" );
					return false;
				}
				if( value[ 0 ] == '.' ) {
					value++;
					config->sounds[ config->num_random_sounds ] = StringHash( Hash64( value.ptr, value.n, base_hash ) );
				}
				else {
					config->sounds[ config->num_random_sounds ] = StringHash( Hash64( value.ptr, value.n ) );
				}
				config->num_random_sounds++;
			}
			else if( key == "delay" ) {
				if( !TrySpanToFloat( value, &config->delay ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to delay should be a number\n" );
					return false;
				}
			}
			else if( key == "volume" ) {
				if( !TrySpanToFloat( value, &config->volume ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to volume should be a number\n" );
					return false;
				}
			}
			else if( key == "pitch" ) {
				if( !TrySpanToFloat( value, &config->pitch ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to pitch should be a number\n" );
					return false;
				}
			}
			else if( key == "pitch_random" ) {
				if( !TrySpanToFloat( value, &config->pitch_random ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to pitch_random should be a number\n" );
					return false;
				}
			}
			else if( key == "attenuation" ) {
				if( value == "none" ) {
					config->attenuation = ATTN_NONE;
				}
				else if( value == "distant" ) {
					config->attenuation = ATTN_DISTANT;
				}
				else if( value == "norm" ) {
					config->attenuation = ATTN_NORM;
				}
				else if( value == "idle" ) {
					config->attenuation = ATTN_IDLE;
				}
				else if( value == "static" ) {
					config->attenuation = ATTN_STATIC;
				}
				else {
					Com_Printf( S_COLOR_YELLOW "Bad attenuation value\n" );
					return false;
				}
			}
			else {
				Com_Printf( S_COLOR_YELLOW "Bad key\n" );
				return false;
			}
		}

		if( config->num_random_sounds == 0 ) {
			Com_Printf( S_COLOR_YELLOW "Section with no sounds\n" );
			return false;
		}

		sfx->num_sounds++;
	}

	return true;
}

static void LoadSoundEffect( const char * path ) {
	TracyZoneScoped;
	TracyZoneText( path, strlen( path ) );

	Span< const char > data = AssetString( path );
	u64 base_hash = Hash64( BasePath( path ) );

	SoundEffect sfx = { };

	if( !ParseSoundEffect( &sfx, &data, base_hash ) ) {
		Com_Printf( S_COLOR_YELLOW "Couldn't load %s\n", path );
		return;
	}

	u64 hash = Hash64( path, strlen( path ) - strlen( ".cdsfx" ) );

	u64 idx = num_sound_effects;
	if( !sound_effects_hashtable.get( hash, &idx ) ) {
		sound_effects_hashtable.add( hash, num_sound_effects );
		num_sound_effects++;
	}

	sound_effects[ idx ] = sfx;
}

static void LoadSoundEffects() {
	TracyZoneScoped;

	for( const char * path : AssetPaths() ) {
		if( FileExtension( path ) == ".cdsfx" ) {
			LoadSoundEffect( path );
		}
	}
}

static void HotloadSoundEffects() {
	TracyZoneScoped;

	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".cdsfx" ) {
			LoadSoundEffect( path );
		}
	}
}

bool S_Init() {
	TracyZoneScoped;

	num_sounds = 0;
	num_sound_effects = 0;
	num_playing_sound_effects = 0;
	immediate_sounds_autoinc = 1;
	sounds_hashtable.clear();
	sound_effects_hashtable.clear();
	immediate_sounds_hashtable.clear();
	music_playing = false;
	initialized = false;

	s_device = NewCvar( "s_device", "", CvarFlag_Archive );
	s_device->modified = false;
	s_volume = NewCvar( "s_volume", "1", CvarFlag_Archive );
	s_musicvolume = NewCvar( "s_musicvolume", "1", CvarFlag_Archive );
	s_muteinbackground = NewCvar( "s_muteinbackground", "1", CvarFlag_Archive );

	if( !S_InitAL() )
		return false;

	LoadSounds();
	LoadSoundEffects();

	initialized = true;

	return true;
}

void S_Shutdown() {
	TracyZoneScoped;

	if( !initialized )
		return;

	S_StopAllSounds( true );

	alDeleteSources( ARRAY_COUNT( free_sound_sources ), free_sound_sources );
	alDeleteSources( 1, &music_source );

	for( u32 i = 0; i < num_sounds; i++ ) {
		alDeleteBuffers( 1, &sounds[ i ].buf );
	}

	CheckALErrors( "S_Shutdown" );

	alcDestroyContext( al_context );
	alcCloseDevice( al_device );
}

Span< const char * > GetAudioDevices( Allocator * a ) {
	NonRAIIDynamicArray< const char * > devices( a );

	const char * cursor = alcGetString( NULL, ALC_ALL_DEVICES_SPECIFIER );
	while( !StrEqual( cursor, "" ) ) {
		devices.add( cursor );
		cursor += strlen( cursor ) + 1;
	}

	return devices.span();
}

static bool FindSound( StringHash name, Sound * sound ) {
	u64 idx;
	if( !initialized || !sounds_hashtable.get( name.hash, &idx ) )
		return false;
	*sound = sounds[ idx ];
	return true;
}

static const SoundEffect * FindSoundEffect( StringHash name ) {
	u64 idx;
	if( !initialized || !sound_effects_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &sound_effects[ idx ];
}

static bool StartSound( PlayingSound * ps, u8 i ) {
	SoundEffect::PlaybackConfig config = ps->sfx->sounds[ i ];

	int idx;
	if( !ps->has_entropy ) {
		idx = RandomUniform( &cls.rng, 0, config.num_random_sounds );
	}
	else {
		RNG rng = NewRNG( ps->entropy, 0 );
		idx = RandomUniform( &rng, 0, config.num_random_sounds );
	}

	Sound sound;
	if( !FindSound( config.sounds[ idx ], &sound ) )
		return false;

	if( num_free_sound_sources == 0 ) {
		Com_Printf( S_COLOR_YELLOW "Too many playing sounds!\n" );
		return false;
	}

	if( !sound.mono && ps->type != PlayingSoundType_Global ) {
		Com_Printf( S_COLOR_YELLOW "Positioned sounds must be mono!\n" );
		return false;
	}

	num_free_sound_sources--;
	ALuint source = free_sound_sources[ num_free_sound_sources ];
	ps->sources[ i ] = source;

	CheckedALSource( source, AL_BUFFER, sound.buf );
	CheckedALSource( source, AL_GAIN, ps->volume * config.volume * s_volume->number );
	CheckedALSource( source, AL_PITCH, ps->pitch * config.pitch + ( RandomFloat11( &cls.rng ) * config.pitch_random * config.pitch * ps->pitch ) );
	CheckedALSource( source, AL_REFERENCE_DISTANCE, S_DEFAULT_ATTENUATION_REFDISTANCE );
	CheckedALSource( source, AL_MAX_DISTANCE, S_DEFAULT_ATTENUATION_MAXDISTANCE );
	CheckedALSource( source, AL_ROLLOFF_FACTOR, config.attenuation );

	switch( ps->type ) {
		case PlayingSoundType_Global:
			CheckedALSource( source, AL_POSITION, Vec3( 0.0f ) );
			CheckedALSource( source, AL_VELOCITY, Vec3( 0.0f ) );
			CheckedALSource( source, AL_SOURCE_RELATIVE, AL_TRUE );
			break;

		case PlayingSoundType_Position:
			CheckedALSource( source, AL_POSITION, ps->origin );
			CheckedALSource( source, AL_VELOCITY, Vec3( 0.0f ) );
			CheckedALSource( source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;

		case PlayingSoundType_Entity:
			CheckedALSource( source, AL_POSITION, cg_entities[ ps->ent_num ].interpolated.origin );
			CheckedALSource( source, AL_VELOCITY, cg_entities[ ps->ent_num ].velocity );
			CheckedALSource( source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;

		case PlayingSoundType_Line:
			CheckedALSource( source, AL_POSITION, ps->origin );
			CheckedALSource( source, AL_VELOCITY, Vec3( 0.0f ) ); // TODO
			CheckedALSource( source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;
	}

	CheckedALSource( source, AL_LOOPING, ps->immediate_handle.x != 0 && ps->loop ? AL_TRUE : AL_FALSE );
	CheckedALSourcePlay( source );

	return true;
}

static void StopSound( PlayingSound * ps, u8 i ) {
	CheckedALSourceStop( ps->sources[ i ] );
	CheckedALSource( ps->sources[ i ], AL_BUFFER, 0 );
	free_sound_sources[ num_free_sound_sources ] = ps->sources[ i ];
	num_free_sound_sources++;
	ps->stopped[ i ] = true;
}

static void UpdateSound( PlayingSound * ps, float volume, float pitch ) {
	ps->volume = volume;
	ps->pitch = pitch;

	for( size_t i = 0; i < ps->sfx->num_sounds; i++ ) {
		if( ps->started[ i ] ) {
			const SoundEffect::PlaybackConfig * config = &ps->sfx->sounds[ i ];
			CheckedALSource( ps->sources[ i ], AL_GAIN, ps->volume * config->volume * s_volume->number );
			CheckedALSource( ps->sources[ i ], AL_PITCH, ps->pitch * config->pitch + ( RandomFloat11( &cls.rng ) * config->pitch_random * config->pitch * ps->pitch ) );
		}
	}
}

void S_Update( Vec3 origin, Vec3 velocity, const mat3_t axis ) {
	TracyZoneScoped;

	if( !initialized )
		return;

	if( s_device->modified ) {
		S_Shutdown();
		S_Init();
		s_device->modified = false;
	}

	HotloadSounds();
	HotloadSoundEffects();

	CheckedALListener( AL_GAIN, IsWindowFocused() || s_muteinbackground->integer == 0 ? 1 : 0 );

	CheckedALListener( AL_POSITION, origin );
	CheckedALListener( AL_VELOCITY, velocity );
	CheckedALListener( AL_ORIENTATION, axis );

	for( size_t i = 0; i < num_playing_sound_effects; i++ ) {
		PlayingSound * ps = &playing_sound_effects[ i ];
		float t = ( cls.monotonicTime - ps->start_time ) * 0.001f;
		bool all_stopped = true;

		bool not_touched = ps->immediate_handle.x != 0 && !ps->touched_since_last_update;
		ps->touched_since_last_update = false;

		for( u8 j = 0; j < ps->sfx->num_sounds; j++ ) {
			if( ps->started[ j ] ) {
				if( ps->stopped[ j ] )
					continue;

				ALint state = CheckedALGetSource( ps->sources[ j ], AL_SOURCE_STATE );
				if( not_touched || state == AL_STOPPED ) {
					StopSound( ps, j );
				}
				else {
					all_stopped = false;
				}
				continue;
			}

			all_stopped = false;

			if( t >= ps->sfx->sounds[ j ].delay ) {
				ps->started[ j ] = true;
				if( !StartSound( ps, j ) ) {
					ps->stopped[ j ] = true;
				}
			}
		}

		if( all_stopped ) {
			if( ps->immediate_handle.x != 0 ) {
				bool ok = immediate_sounds_hashtable.remove( ps->immediate_handle.x );
				assert( ok );
			}

			// remove-swap it from playing_sound_effects
			num_playing_sound_effects--;
			if( ps != &playing_sound_effects[ num_playing_sound_effects ] ) {
				Swap2( ps, &playing_sound_effects[ num_playing_sound_effects ] );

				if( ps->immediate_handle.x != 0 ) {
					bool ok = immediate_sounds_hashtable.update( ps->immediate_handle.x, ps - playing_sound_effects );
					assert( ok );
				}
			}

			i--;
			continue;
		}

		for( u8 j = 0; j < ps->sfx->num_sounds; j++ ) {
			if( !ps->started[ j ] || ps->stopped[ j ] )
				continue;

			if( s_volume->modified ) {
				CheckedALSource( ps->sources[ j ], AL_GAIN, ps->volume * ps->sfx->sounds[ j ].volume * s_volume->number );
			}

			if( ps->type == PlayingSoundType_Entity ) {
				CheckedALSource( ps->sources[ j ], AL_POSITION, cg_entities[ ps->ent_num ].interpolated.origin );
				CheckedALSource( ps->sources[ j ], AL_VELOCITY, cg_entities[ ps->ent_num ].velocity );
			}
			else if( ps->type == PlayingSoundType_Position ) {
				CheckedALSource( ps->sources[ j ], AL_POSITION, ps->origin );
			}
			else if( ps->type == PlayingSoundType_Line ) {
				Vec3 p = ClosestPointOnSegment( ps->origin, ps->end, origin );
				CheckedALSource( ps->sources[ j ], AL_POSITION, p );
			}
		}
	}

	if( ( s_volume->modified || s_musicvolume->modified ) && music_playing ) {
		CheckedALSource( music_source, AL_GAIN, s_volume->number * s_musicvolume->number * MusicIsWayTooLoud );
	}

	s_volume->modified = false;
	s_musicvolume->modified = false;
}

static PlayingSound * FindEmptyPlayingSound( int ent_num, int channel ) {
	if( channel != 0 ) {
		for( u32 i = 0; i < num_playing_sound_effects; i++ ) {
			PlayingSound * ps = &playing_sound_effects[ i ];
			if( ps->ent_num == ent_num && ps->channel == channel ) {
				for( u8 j = 0; j < ps->sfx->num_sounds; j++ ) {
					if( ps->started[ j ] && !ps->stopped[ j ] ) {
						StopSound( ps, j );
					}
				}
				return ps;
			}
		}
	}

	if( num_playing_sound_effects == ARRAY_COUNT( playing_sound_effects ) )
		return NULL;

	num_playing_sound_effects++;
	return &playing_sound_effects[ num_playing_sound_effects - 1 ];
}

static PlayingSound * StartSoundEffect( StringHash name, int ent_num, int channel, float volume, float pitch, PlayingSoundType type ) {
	if( !initialized )
		return NULL;

	const SoundEffect * sfx = FindSoundEffect( name );
	if( sfx == NULL )
		return NULL;

	PlayingSound * ps = FindEmptyPlayingSound( ent_num, channel );
	if( ps == NULL ) {
		Com_Printf( S_COLOR_YELLOW "Too many playing sound effects!\n" );
		return NULL;
	}

	*ps = { };
	ps->type = type;
	ps->hash = name;
	ps->sfx = sfx;
	ps->start_time = cls.monotonicTime;
	ps->ent_num = ent_num;
	ps->channel = channel;
	ps->volume = volume;
	ps->pitch = pitch;
	ps->touched_since_last_update = true;

	return ps;
}

void S_StartFixedSound( StringHash name, Vec3 origin, int channel, float volume, float pitch ) {
	PlayingSound * ps = StartSoundEffect( name, 0, channel, volume, pitch, PlayingSoundType_Position );
	if( ps == NULL )
		return;
	ps->origin = origin;
}

void S_StartEntitySound( StringHash name, int ent_num, int channel, float volume, float pitch ) {
	StartSoundEffect( name, ent_num, channel, volume, pitch, PlayingSoundType_Entity );
}

void S_StartEntitySound( StringHash name, int ent_num, int channel, float volume, float pitch, u32 sfx_entropy ) {
	PlayingSound * ps = StartSoundEffect( name, ent_num, channel, volume, pitch, PlayingSoundType_Entity );
	if( ps == NULL )
		return;
	ps->entropy = sfx_entropy;
	ps->has_entropy = true;
}

void S_StartGlobalSound( StringHash name, int channel, float volume, float pitch ) {
	StartSoundEffect( name, 0, channel, volume, pitch, PlayingSoundType_Global );
}

void S_StartGlobalSound( StringHash name, int channel, float volume, float pitch, u32 sfx_entropy ) {
	PlayingSound * ps = StartSoundEffect( name, 0, channel, volume, pitch, PlayingSoundType_Global );
	if( ps == NULL )
		return;
	ps->entropy = sfx_entropy;
	ps->has_entropy = true;
}

void S_StartLocalSound( StringHash name, int channel, float volume, float pitch ) {
	StartSoundEffect( name, -1, channel, volume, pitch, PlayingSoundType_Global );
}

void S_StartLineSound( StringHash name, Vec3 start, Vec3 end, int channel, float volume, float pitch) {
	PlayingSound * ps = StartSoundEffect( name, -1, channel, volume, pitch, PlayingSoundType_Line );
	if( ps == NULL )
		return;
	ps->origin = start;
	ps->end = end;
}

static ImmediateSoundHandle StartImmediateSound( StringHash name, int ent_num, float volume, float pitch, bool loop, u32 sfx_entropy, PlayingSoundType type, ImmediateSoundHandle handle ) {
	if( name == EMPTY_HASH ) {
		return { 0 };
	}

	u64 idx;
	bool found = immediate_sounds_hashtable.get( handle.x, &idx );
	if( handle.x != 0 && found && playing_sound_effects[ idx ].hash == name ) {
		if( playing_sound_effects[ idx ].volume != volume || playing_sound_effects[ idx ].pitch != pitch ) {
			UpdateSound( &playing_sound_effects[ idx ], volume, pitch );
		}
		playing_sound_effects[ idx ].touched_since_last_update = true;
	}
	else {
		PlayingSound * ps = StartSoundEffect( name, ent_num, CHAN_AUTO, volume, pitch, type );
		if( ps == NULL )
			return { 0 };

		handle = { immediate_sounds_autoinc };

		immediate_sounds_autoinc++;
		if( immediate_sounds_autoinc == 0 )
			immediate_sounds_autoinc++;

		ps->immediate_handle = handle;
		ps->loop = loop;
		ps->entropy = sfx_entropy;
		idx = ps - playing_sound_effects;

		immediate_sounds_hashtable.add( handle.x, idx );
	}

	return handle;
}

ImmediateSoundHandle S_ImmediateEntitySound( StringHash name, int ent_num, float volume, float pitch, bool loop, ImmediateSoundHandle handle ) {
	return StartImmediateSound( name, ent_num, volume, pitch, loop, 0, PlayingSoundType_Entity, handle );
}

ImmediateSoundHandle S_ImmediateEntitySound( StringHash name, int ent_num, float volume, float pitch, bool loop, u32 sfx_entropy, ImmediateSoundHandle handle ) {
	return StartImmediateSound( name, ent_num, volume, pitch, loop, sfx_entropy, PlayingSoundType_Entity, handle );
}

ImmediateSoundHandle S_ImmediateFixedSound( StringHash name, Vec3 origin, float volume, float pitch, ImmediateSoundHandle handle ) {
	handle = StartImmediateSound( name, -1, volume, pitch, true, 0, PlayingSoundType_Position, handle );
	if( handle.x == 0 )
		return handle;

	u64 idx;
	bool ok = immediate_sounds_hashtable.get( handle.x, &idx );
	assert( ok );

	PlayingSound * ps = &playing_sound_effects[ idx ];
	ps->origin = origin;

	return handle;
}

ImmediateSoundHandle S_ImmediateLineSound( StringHash name, Vec3 start, Vec3 end, float volume, float pitch, ImmediateSoundHandle handle ) {
	handle = StartImmediateSound( name, -1, volume, pitch, true, 0, PlayingSoundType_Line, handle );
	if( handle.x == 0 )
		return handle;

	u64 idx;
	bool ok = immediate_sounds_hashtable.get( handle.x, &idx );
	assert( ok );

	PlayingSound * ps = &playing_sound_effects[ idx ];
	ps->origin = start;
	ps->end = end;

	return handle;
}

void S_StopAllSounds( bool stop_music ) {
	if( !initialized )
		return;

	for( u32 i = 0; i < num_playing_sound_effects; i++ ) {
		PlayingSound * ps = &playing_sound_effects[ i ];
		for( u8 j = 0; j < ps->sfx->num_sounds; j++ ) {
			if( ps->started[ j ] && !ps->stopped[ j ] ) {
				StopSound( ps, j );
			}
		}
	}

	num_playing_sound_effects = 0;

	if( stop_music ) {
		S_StopBackgroundTrack();
	}

	immediate_sounds_hashtable.clear();
}

void S_StartMenuMusic() {
	if( !initialized )
		return;

	Sound sound;
	if( !FindSound( "sounds/music/menu_1", &sound ) )
		return;

	if( music_playing )
		return;

	CheckedALSource( music_source, AL_GAIN, s_volume->number * s_musicvolume->number * MusicIsWayTooLoud );
	CheckedALSource( music_source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE );
	CheckedALSource( music_source, AL_LOOPING, AL_TRUE );
	CheckedALSource( music_source, AL_BUFFER, sound.buf );

	CheckedALSourcePlay( music_source );

	music_playing = true;
}

void S_StopBackgroundTrack() {
	if( initialized && music_playing ) {
		CheckedALSourceStop( music_source );
		CheckedALSource( music_source, AL_BUFFER, 0 );
	}
	music_playing = false;
}
