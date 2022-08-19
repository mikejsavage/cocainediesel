#include <algorithm> // std::sort

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/time.h"
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

		Time delay;
		float volume;
		float pitch;
		float pitch_random;
		float attenuation;
	};

	PlaybackConfig sounds[ 8 ];
	u8 num_sounds;
};

struct PlayingSFX {
	PlayingSFXHandle handle;
	PlaySFXConfig config;

	StringHash hash;
	const SoundEffect * sfx;
	Time start_time;

	bool immediate;
	bool keep_playing_immediate;

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

static PlayingSFX playing_sound_effects[ MAX_PLAYING_SOUNDS ];
static u32 num_playing_sound_effects;
static Hashtable< MAX_PLAYING_SOUNDS * 2 > playing_sounds_hashtable;
static u64 playing_sounds_autoinc;

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

static bool InitOpenAL() {
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
		ALC_MONO_SOURCES, MAX_PLAYING_SOUNDS,
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
		ShutdownSound();
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
		StopAllSounds( true );
		alDeleteBuffers( 1, &sounds[ idx ].buf );
	}

	ALenum format = channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
	alGenBuffers( 1, &sounds[ idx ].buf );
	alBufferData( sounds[ idx ].buf, format, samples, num_samples * channels * sizeof( s16 ), sample_rate );
	CheckALErrors( "AddSound" );

	sounds[ idx ].mono = channels == 1;

	if( restart_music ) {
		StartMenuMusic();
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

static bool ParseSoundEffect( SoundEffect * sfx, Span< const char > * data, Span< const char > base_path ) {
	TracyZoneScoped;

	if( sfx->num_sounds == ARRAY_COUNT( sfx->sounds ) ) {
		Com_Printf( S_COLOR_YELLOW "SFX with too many sections\n" );
		return false;
	}

	TempAllocator temp = cls.frame_arena.temp();

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
				if( StartsWith( value, "." ) ) {
					config->sounds[ config->num_random_sounds ] = StringHash( temp( "{}{}", base_path, value + 1 ) );
				}
				else {
					config->sounds[ config->num_random_sounds ] = StringHash( value );
				}
				config->num_random_sounds++;
			}
			else if( key == "find_sounds" ) {
				TracyZoneScopedN( "find_sounds" );

				Span< const char > prefix = value;
				if( StartsWith( value, "." ) ) {
					prefix = MakeSpan( temp( "{}{}", base_path, value + 1 ) );
				}

				for( const char * path : AssetPaths() ) {
					if( FileExtension( path ) == ".ogg" && StartsWith( MakeSpan( path ), prefix ) ) {
						if( config->num_random_sounds == ARRAY_COUNT( config->sounds ) ) {
							Com_Printf( S_COLOR_YELLOW "SFX with too many random sounds\n" );
							return false;
						}

						config->sounds[ config->num_random_sounds ] = StringHash( StripExtension( path ) );
						config->num_random_sounds++;
					}
				}
			}
			else if( key == "delay" ) {
				float delay;
				if( !TrySpanToFloat( value, &delay ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to delay should be a number\n" );
					return false;
				}
				config->delay = Seconds( double( delay ) );
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

	SoundEffect sfx = { };

	if( !ParseSoundEffect( &sfx, &data, BasePath( path ) ) ) {
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

bool InitSound() {
	TracyZoneScoped;

	num_sounds = 0;
	num_sound_effects = 0;
	num_playing_sound_effects = 0;
	playing_sounds_autoinc = 1;
	sounds_hashtable.clear();
	sound_effects_hashtable.clear();
	playing_sounds_hashtable.clear();
	music_playing = false;
	initialized = false;

	s_device = NewCvar( "s_device", "", CvarFlag_Archive );
	s_device->modified = false;
	s_volume = NewCvar( "s_volume", "1", CvarFlag_Archive );
	s_musicvolume = NewCvar( "s_musicvolume", "1", CvarFlag_Archive );
	s_muteinbackground = NewCvar( "s_muteinbackground", "1", CvarFlag_Archive );

	if( !InitOpenAL() )
		return false;

	LoadSounds();
	LoadSoundEffects();

	initialized = true;

	return true;
}

void ShutdownSound() {
	TracyZoneScoped;

	if( !initialized )
		return;

	StopAllSounds( true );

	alDeleteSources( ARRAY_COUNT( free_sound_sources ), free_sound_sources );
	alDeleteSources( 1, &music_source );

	for( u32 i = 0; i < num_sounds; i++ ) {
		alDeleteBuffers( 1, &sounds[ i ].buf );
	}

	CheckALErrors( "ShutdownSound" );

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

static bool StartSound( PlayingSFX * ps, u8 i ) {
	SoundEffect::PlaybackConfig config = ps->sfx->sounds[ i ];

	int idx;
	if( !ps->config.entropy.exists ) {
		idx = RandomUniform( &cls.rng, 0, config.num_random_sounds );
	}
	else {
		RNG rng = NewRNG( ps->config.entropy.value, 0 );
		idx = RandomUniform( &rng, 0, config.num_random_sounds );
	}

	Sound sound;
	if( !FindSound( config.sounds[ idx ], &sound ) )
		return false;

	if( num_free_sound_sources == 0 ) {
		Com_Printf( S_COLOR_YELLOW "Too many playing sounds!\n" );
		return false;
	}

	if( !sound.mono && ps->config.spatialisation != SpatialisationMethod_None ) {
		Com_Printf( S_COLOR_YELLOW "Positioned sounds must be mono!\n" );
		return false;
	}

	num_free_sound_sources--;
	ALuint source = free_sound_sources[ num_free_sound_sources ];
	ps->sources[ i ] = source;

	CheckedALSource( source, AL_BUFFER, sound.buf );
	CheckedALSource( source, AL_GAIN, ps->config.volume * config.volume * s_volume->number );
	CheckedALSource( source, AL_PITCH, ps->config.pitch * config.pitch + ( RandomFloat11( &cls.rng ) * config.pitch_random * config.pitch * ps->config.pitch ) );
	CheckedALSource( source, AL_REFERENCE_DISTANCE, S_DEFAULT_ATTENUATION_REFDISTANCE );
	CheckedALSource( source, AL_MAX_DISTANCE, S_DEFAULT_ATTENUATION_MAXDISTANCE );
	CheckedALSource( source, AL_ROLLOFF_FACTOR, config.attenuation );

	switch( ps->config.spatialisation ) {
		case SpatialisationMethod_None:
			CheckedALSource( source, AL_POSITION, Vec3( 0.0f ) );
			CheckedALSource( source, AL_VELOCITY, Vec3( 0.0f ) );
			CheckedALSource( source, AL_SOURCE_RELATIVE, AL_TRUE );
			break;

		case SpatialisationMethod_Position:
			CheckedALSource( source, AL_POSITION, ps->config.position );
			CheckedALSource( source, AL_VELOCITY, Vec3( 0.0f ) );
			CheckedALSource( source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;

		case SpatialisationMethod_Entity:
			CheckedALSource( source, AL_POSITION, cg_entities[ ps->config.ent_num ].interpolated.origin );
			CheckedALSource( source, AL_VELOCITY, cg_entities[ ps->config.ent_num ].velocity );
			CheckedALSource( source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;

		case SpatialisationMethod_LineSegment:
			CheckedALSource( source, AL_POSITION, ps->config.line_segment.start );
			CheckedALSource( source, AL_VELOCITY, ps->config.line_segment.end - ps->config.line_segment.start );
			CheckedALSource( source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;
	}

	CheckedALSource( source, AL_LOOPING, ps->immediate ? AL_TRUE : AL_FALSE );
	CheckedALSourcePlay( source );

	return true;
}

static void StopSound( PlayingSFX * ps, u8 i ) {
	CheckedALSourceStop( ps->sources[ i ] );
	CheckedALSource( ps->sources[ i ], AL_BUFFER, 0 );
	free_sound_sources[ num_free_sound_sources ] = ps->sources[ i ];
	num_free_sound_sources++;
	ps->stopped[ i ] = true;
}

static void StopSFX( PlayingSFX * ps ) {
	for( u8 i = 0; i < ps->sfx->num_sounds; i++ ) {
		if( ps->started[ i ] && !ps->stopped[ i ] ) {
			StopSound( ps, i );
		}
	}

	// remove-swap it from playing_sound_effects
	num_playing_sound_effects--;
	Swap2( ps, &playing_sound_effects[ num_playing_sound_effects ] );

	{
		bool ok = playing_sounds_hashtable.update( ps->handle.handle, ps - playing_sound_effects );
		assert( ok );
	}

	{
		bool ok = playing_sounds_hashtable.remove( playing_sound_effects[ num_playing_sound_effects ].handle.handle );
		assert( ok );
	}
}

static void UpdateSound( PlayingSFX * ps, float volume, float pitch ) {
	ps->config.volume = volume;
	ps->config.pitch = pitch;

	for( size_t i = 0; i < ps->sfx->num_sounds; i++ ) {
		if( ps->started[ i ] ) {
			const SoundEffect::PlaybackConfig * config = &ps->sfx->sounds[ i ];
			CheckedALSource( ps->sources[ i ], AL_GAIN, ps->config.volume * config->volume * s_volume->number );
			CheckedALSource( ps->sources[ i ], AL_PITCH, ps->config.pitch * config->pitch + ( RandomFloat11( &cls.rng ) * config->pitch_random * config->pitch * ps->config.pitch ) );
		}
	}
}

void SoundFrame( Vec3 origin, Vec3 velocity, const mat3_t axis ) {
	TracyZoneScoped;

	if( !initialized )
		return;

	if( s_device->modified ) {
		ShutdownSound();
		InitSound();
		s_device->modified = false;
	}

	HotloadSounds();
	HotloadSoundEffects();

	CheckedALListener( AL_GAIN, IsWindowFocused() || s_muteinbackground->integer == 0 ? 1 : 0 );

	CheckedALListener( AL_POSITION, origin );
	CheckedALListener( AL_VELOCITY, velocity );
	CheckedALListener( AL_ORIENTATION, axis );

	for( size_t i = 0; i < num_playing_sound_effects; i++ ) {
		PlayingSFX * ps = &playing_sound_effects[ i ];
		Time t = cls.monotonicTime - ps->start_time;
		bool all_stopped = true;

		for( u8 j = 0; j < ps->sfx->num_sounds; j++ ) {
			if( ps->started[ j ] ) {
				if( ps->stopped[ j ] )
					continue;

				ALint state = CheckedALGetSource( ps->sources[ j ], AL_SOURCE_STATE );
				if( state == AL_STOPPED ) {
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

		bool stop_immediate = ps->immediate && !ps->keep_playing_immediate;
		if( stop_immediate || all_stopped ) {
			StopSFX( ps );
			i--;
			continue;
		}
		ps->keep_playing_immediate = false;

		for( u8 j = 0; j < ps->sfx->num_sounds; j++ ) {
			if( !ps->started[ j ] || ps->stopped[ j ] )
				continue;

			if( s_volume->modified ) {
				CheckedALSource( ps->sources[ j ], AL_GAIN, ps->config.volume * ps->sfx->sounds[ j ].volume * s_volume->number );
			}

			if( ps->config.spatialisation == SpatialisationMethod_Entity ) {
				CheckedALSource( ps->sources[ j ], AL_POSITION, cg_entities[ ps->config.ent_num ].interpolated.origin );
				CheckedALSource( ps->sources[ j ], AL_VELOCITY, cg_entities[ ps->config.ent_num ].velocity );
			}
			else if( ps->config.spatialisation == SpatialisationMethod_Position ) {
				CheckedALSource( ps->sources[ j ], AL_POSITION, ps->config.position );
			}
			else if( ps->config.spatialisation == SpatialisationMethod_LineSegment ) {
				Vec3 p = ClosestPointOnSegment( ps->config.line_segment.start, ps->config.line_segment.end, origin );
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

PlaySFXConfig PlaySFXConfigGlobal( float volume ) {
	PlaySFXConfig config = { };
	config.spatialisation = SpatialisationMethod_None;
	config.volume = volume;
	config.pitch = 1.0f;
	return config;
}

PlaySFXConfig PlaySFXConfigPosition( Vec3 position, float volume ) {
	PlaySFXConfig config = { };
	config.spatialisation = SpatialisationMethod_Position;
	config.position = position;
	config.volume = volume;
	config.pitch = 1.0f;
	return config;
}

PlaySFXConfig PlaySFXConfigEntity( int ent_num, float volume ) {
	PlaySFXConfig config = { };
	config.spatialisation = SpatialisationMethod_Entity;
	config.ent_num = ent_num;
	config.volume = volume;
	config.pitch = 1.0f;
	return config;
}

PlaySFXConfig PlaySFXConfigLineSegment( Vec3 start, Vec3 end, float volume ) {
	PlaySFXConfig config = { };
	config.spatialisation = SpatialisationMethod_LineSegment;
	config.line_segment.start = start;
	config.line_segment.end = end;
	config.volume = volume;
	config.pitch = 1.0f;
	return config;
}

PlayingSFX * PlaySFXInternal( StringHash name, const PlaySFXConfig & config ) {
	if( !initialized )
		return NULL;

	const SoundEffect * sfx = FindSoundEffect( name );
	if( sfx == NULL )
		return NULL;

	if( num_playing_sound_effects == ARRAY_COUNT( playing_sound_effects ) ) {
		Com_Printf( S_COLOR_YELLOW "Too many playing sound effects!\n" );
		return NULL;
	}

	PlayingSFX * ps = &playing_sound_effects[ num_playing_sound_effects ];

	*ps = { };
	ps->config = config;
	ps->hash = name;
	ps->sfx = sfx;

	ps->handle = { playing_sounds_autoinc };
	playing_sounds_autoinc++;

	ps->start_time = cls.monotonicTime;

	bool ok = playing_sounds_hashtable.add( ps->handle.handle, num_playing_sound_effects );
	assert( ok );
	num_playing_sound_effects++;

	return ps;
}

PlayingSFXHandle PlaySFX( StringHash name, const PlaySFXConfig & config ) {
	PlayingSFX * ps = PlaySFXInternal( name, config );
	if( ps == NULL )
		return { };
	return ps->handle;
}

PlayingSFXHandle PlayImmediateSFX( StringHash name, PlayingSFXHandle handle, const PlaySFXConfig & config ) {
	u64 idx;
	if( playing_sounds_hashtable.get( handle.handle, &idx ) ) {
		PlayingSFX * ps = &playing_sound_effects[ idx ];
		if( ps->hash == name ) {
			ps->keep_playing_immediate = true;
			UpdateSound( ps, config.volume, config.pitch );
			return handle;
		}
		StopSFX( &playing_sound_effects[ idx ] );
	}

	PlayingSFX * ps = PlaySFXInternal( name, config );
	if( ps == NULL )
		return { };

	ps->immediate = true;
	ps->keep_playing_immediate = true;

	return ps->handle;
}

void StopSFX( PlayingSFXHandle handle ) {
	u64 idx;
	if( !playing_sounds_hashtable.get( handle.handle, &idx ) )
		return;
	StopSFX( &playing_sound_effects[ idx ] );
}

void StopAllSounds( bool stop_music ) {
	if( !initialized )
		return;

	while( num_playing_sound_effects > 0 ) {
		StopSFX( &playing_sound_effects[ 0 ] );
	}

	if( stop_music ) {
		StopMenuMusic();
	}

	playing_sounds_hashtable.clear();
}

void StartMenuMusic() {
	if( !initialized )
		return;

	Sound sound;
	if( !FindSound( "sounds/music/longcovid", &sound ) )
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

void StopMenuMusic() {
	if( initialized && music_playing ) {
		CheckedALSourceStop( music_source );
		CheckedALSource( music_source, AL_BUFFER, 0 );
	}
	music_playing = false;
}
