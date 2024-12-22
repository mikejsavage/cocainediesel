#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fpe.h"
#include "qcommon/hash.h"
#include "qcommon/hashmap.h"
#include "qcommon/time.h"
#include "client/audio/api.h"
#include "client/audio/backend.h"
#include "client/assets.h"
#include "client/threadpool.h"
#include "cgame/cg_local.h"
#include "gameshared/gs_public.h"

#include "nanosort/nanosort.hpp"

#define AL_LIBTYPE_STATIC
#define AL_ALEXT_PROTOTYPES
#include "openal/al.h"
#include "openal/alc.h"
#include "openal/alext.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.h"

struct Sound {
	ALuint buf;
	Span< s16 > samples;
	bool mono;
};

struct SoundEffect {
	struct PlaybackConfig {
		BoundedDynamicArray< StringHash, 128 > sounds;

		Time delay;
		float volume;
		float pitch;
		float pitch_random;
		float attenuation;
	};

	BoundedDynamicArray< PlaybackConfig, 8 > sounds;
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

static bool backend_initialized;
static bool backend_device_initialized;

Cvar * s_device;
static Cvar * s_volume;
static Cvar * s_musicvolume;
static Cvar * s_muteinbackground;

static constexpr u32 MAX_SOUND_ASSETS = 4096;
static constexpr u32 MAX_SOUND_EFFECTS = 4096;
static constexpr u32 MAX_PLAYING_SOUNDS = 256;

static u64 GetPlayingSFXKey( const PlayingSFX & sfx ) {
	return sfx.handle.handle;
}

static Hashmap< Sound, MAX_SOUND_ASSETS > sounds;
static Hashmap< SoundEffect, MAX_SOUND_EFFECTS > sound_effects;
static BoundedDynamicArray< ALuint, MAX_PLAYING_SOUNDS > free_sound_sources;
static Hashmap< PlayingSFX, MAX_PLAYING_SOUNDS, GetPlayingSFXKey > playing_sounds;
static u64 playing_sound_handle_autoinc;

static ALuint music_source;
static bool music_playing;

constexpr float MusicIsWayTooLoud = 0.25f;

static const char * ALErrorMessage( ALenum error ) {
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
			Com_Printf( S_COLOR_RED "AL error (%s): %s\n", ALErrorMessage( err ), buf );
		}
		else {
			Fatal( "AL error (%s): %s", ALErrorMessage( err ), buf );
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

static void CheckedALListenerOrientation( Vec3 forward, Vec3 up ) {
	Vec3 forward_and_up[ 2 ] = { forward, up };
	alListenerfv( AL_ORIENTATION, forward_and_up[ 0 ].ptr() );
	CheckALErrors( "alListenerfv( AL_ORIENTATION, {}/{} )", forward, up );
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

static void InitOpenAL() {
	TracyZoneScoped;

	al_device = alcLoopbackOpenDeviceSOFT( NULL );

	constexpr ALCint attrs[] = {
		ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT,
		ALC_FORMAT_TYPE_SOFT, ALC_FLOAT_SOFT,
		ALC_FREQUENCY, AUDIO_BACKEND_SAMPLE_RATE,
		ALC_HRTF_SOFT, ALC_HRTF_ENABLED_SOFT,
		ALC_MONO_SOURCES, MAX_PLAYING_SOUNDS,
		ALC_STEREO_SOURCES, 16,
		0
	};
	al_context = alcCreateContext( al_device, attrs );
	if( al_context == NULL ) {
		Fatal( "alcCreateContext" );
	}
	alcMakeContextCurrent( al_context );

	alDopplerFactor( 1.0f );
	alDopplerVelocity( 10976.0f );
	alSpeedOfSound( 10976.0f );

	alDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );

	for( size_t i = 0; i < ARRAY_COUNT( free_sound_sources ); i++ ) {
		alGenSources( 1, free_sound_sources.add().value );
	}
	alGenSources( 1, &music_source );

	if( alGetError() != AL_NO_ERROR ) {
		Fatal( "Failed to allocate sound sources" );
	}
}

static void ShutdownOpenAL() {
	alDeleteSources( ARRAY_COUNT( free_sound_sources ), free_sound_sources.ptr() );
	alDeleteSources( 1, &music_source );

	CheckALErrors( "ShutdownSound" );

	alcDestroyContext( al_context );
	alcCloseDevice( al_device );
}

struct DecodeSoundJob {
	struct {
		Span< const char > path;
		Span< const u8 > ogg;
	} in;

	struct {
		int channels;
		int sample_rate;
		int num_samples;
		s16 * samples;
	} out;
};

static void AddSound( Span< const char > path, int num_samples, int channels, int sample_rate, s16 * samples ) {
	TracyZoneScoped;
	TracyZoneSpan( path );

	if( num_samples == -1 ) {
		Com_GGPrint( S_COLOR_RED "Couldn't decode sound {}", path );
		return;
	}

	u64 hash = Hash64( StripExtension( path ) );

	bool restart_music = false;

	Sound * sound = sounds.get( hash );
	if( sound == NULL ) {
		if( sounds.full() ) {
			Com_Printf( S_COLOR_YELLOW "Too many sounds!\n" );
			return;
		}
		if( sound_effects.full() ) {
			Com_Printf( S_COLOR_YELLOW "Too many sound effects!\n" );
			return;
		}

		sound = sounds.add( hash );

		// add simple sound effect
		sound_effects.add( hash, SoundEffect {
			.sounds = {
				SoundEffect::PlaybackConfig {
					.sounds = { StringHash( hash ) },
					.volume = 1.0f,
					.pitch = 1.0f,
					.attenuation = ATTN_NORM,
				},
			},
		} );
	}
	else {
		restart_music = music_playing;
		StopAllSounds( true );
		alDeleteBuffers( 1, &sound->buf );
	}

	ALenum format = channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
	alGenBuffers( 1, &sound->buf );
	sound->samples = Span< s16 >( samples, num_samples );
	sound->mono = channels == 1;

	alBufferDataStatic( sound->buf, format, samples, num_samples * channels * sizeof( s16 ), sample_rate );
	CheckALErrors( "AddSound" );

	if( restart_music ) {
		StartMenuMusic();
	}
}

static void LoadSounds() {
	TracyZoneScoped;

	DynamicArray< DecodeSoundJob > jobs( sys_allocator );
	{
		TracyZoneScopedN( "Build job list" );

		for( Span< const char > path : AssetPaths() ) {
			if( FileExtension( path ) == ".ogg" ) {
				DecodeSoundJob job;
				job.in.path = path;
				job.in.ogg = AssetBinary( path );

				jobs.add( job );
			}
		}

		nanosort( jobs.begin(), jobs.end(), []( const DecodeSoundJob & a, const DecodeSoundJob & b ) {
			return a.in.ogg.n > b.in.ogg.n;
		} );
	}

	ParallelFor( jobs.span(), []( TempAllocator * temp, void * data ) {
		DecodeSoundJob * job = ( DecodeSoundJob * ) data;

		TracyZoneScopedN( "stb_vorbis_decode_memory" );
		TracyZoneSpan( job->in.path );

		DisableFPEScoped;
		job->out.num_samples = stb_vorbis_decode_memory( job->in.ogg.ptr, job->in.ogg.num_bytes(), &job->out.channels, &job->out.sample_rate, &job->out.samples );
	} );

	for( DecodeSoundJob job : jobs ) {
		AddSound( job.in.path, job.out.num_samples, job.out.channels, job.out.sample_rate, job.out.samples );
	}
}

static void HotloadSounds() {
	TracyZoneScoped;

	for( Span< const char > path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".ogg" ) {
			Span< const u8 > ogg = AssetBinary( path );

			int num_samples, channels, sample_rate;
			s16 * samples;
			{
				TracyZoneScopedN( "stb_vorbis_decode_memory" );
				TracyZoneSpan( path );
				DisableFPEScoped;
				num_samples = stb_vorbis_decode_memory( ogg.ptr, ogg.num_bytes(), &channels, &sample_rate, &samples );
			}

			AddSound( path, num_samples, channels, sample_rate, samples );
		}
	}
}

static bool ParseSoundEffect( SoundEffect * sfx, Span< const char > * data, Span< const char > base_path ) {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	while( true ) {
		Span< const char > opening_brace = ParseToken( data, Parse_DontStopOnNewLine );
		if( opening_brace == "" )
			break;

		if( opening_brace != "{" ) {
			Com_Printf( S_COLOR_YELLOW "Expected {\n" );
			return false;
		}

		SoundEffect::PlaybackConfig config = { };
		config.volume = 1.0f;
		config.pitch = 1.0f;
		config.pitch_random = 0.0f;
		config.attenuation = ATTN_NORM;

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
				StringHash hash;
				if( StartsWith( value, "." ) ) {
					hash = StringHash( temp( "{}{}", base_path, value + 1 ) );
				}
				else {
					hash = StringHash( value );
				}
				if( !config.sounds.add( hash ) ) {
					Com_Printf( S_COLOR_YELLOW "SFX with too many random sounds\n" );
					return false;
				}
			}
			else if( key == "find_sounds" ) {
				TracyZoneScopedN( "find_sounds" );

				Span< const char > prefix = value;
				if( StartsWith( value, "." ) ) {
					prefix = MakeSpan( temp( "{}{}", base_path, value + 1 ) );
				}

				for( Span< const char > path : AssetPaths() ) {
					if( FileExtension( path ) == ".ogg" && StartsWith( path, prefix ) ) {
						if( !config.sounds.add( StringHash( StripExtension( path ) ) ) ) {
							Com_Printf( S_COLOR_YELLOW "SFX with too many random sounds\n" );
							return false;
						}
					}
				}
			}
			else if( key == "delay" ) {
				float delay;
				if( !TrySpanToFloat( value, &delay ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to delay should be a number\n" );
					return false;
				}
				config.delay = Seconds( double( delay ) );
			}
			else if( key == "volume" ) {
				if( !TrySpanToFloat( value, &config.volume ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to volume should be a number\n" );
					return false;
				}
			}
			else if( key == "pitch" ) {
				if( !TrySpanToFloat( value, &config.pitch ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to pitch should be a number\n" );
					return false;
				}
			}
			else if( key == "pitch_random" ) {
				if( !TrySpanToFloat( value, &config.pitch_random ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to pitch_random should be a number\n" );
					return false;
				}
			}
			else if( key == "attenuation" ) {
				if( value == "none" ) {
					config.attenuation = ATTN_NONE;
				}
				else if( value == "distant" ) {
					config.attenuation = ATTN_DISTANT;
				}
				else if( value == "norm" ) {
					config.attenuation = ATTN_NORM;
				}
				else if( value == "idle" ) {
					config.attenuation = ATTN_IDLE;
				}
				else if( value == "static" ) {
					config.attenuation = ATTN_STATIC;
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

		if( config.sounds.size() == 0 ) {
			Com_Printf( S_COLOR_YELLOW "Section with no sounds\n" );
			return false;
		}

		if( !sfx->sounds.add( config ) ) {
			Com_Printf( S_COLOR_YELLOW "SFX with too many sections\n" );
			return false;
		}
	}

	return true;
}

static void LoadSoundEffect( Span< const char > path ) {
	TracyZoneScoped;
	TracyZoneSpan( path );

	Span< const char > data = AssetString( path );

	SoundEffect sfx = { };
	if( !ParseSoundEffect( &sfx, &data, BasePath( path ) ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Couldn't load {}", path );
		return;
	}

	u64 hash = Hash64( StripExtension( path ) );
	if( !sound_effects.upsert( hash, sfx ) ) {
		Com_Printf( S_COLOR_YELLOW "Too many sound effects!\n" );
	}
}

static void LoadSoundEffects() {
	TracyZoneScoped;

	for( Span< const char > path : AssetPaths() ) {
		if( FileExtension( path ) == ".cdsfx" ) {
			LoadSoundEffect( path );
		}
	}
}

static void HotloadSoundEffects() {
	TracyZoneScoped;

	for( Span< const char > path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".cdsfx" ) {
			LoadSoundEffect( path );
		}
	}
}

static void AudioCallback( Span< Vec2 > buffer, void * userdata ) {
	TracyZoneScoped;

	alcRenderSamplesSOFT( al_device, buffer.ptr, buffer.n );
	CheckALErrors( "alcRenderSamplesSOFT( {} )", buffer.n );
}

void InitSound() {
	TracyZoneScoped;

	playing_sound_handle_autoinc = 1;
	sounds.clear();
	sound_effects.clear();
	playing_sounds.clear();
	music_playing = false;
	backend_initialized = false;
	backend_device_initialized = false;

	s_device = NewCvar( "s_device", "", CvarFlag_Archive );
	s_device->modified = false;
	s_volume = NewCvar( "s_volume", "1", CvarFlag_Archive );
	s_musicvolume = NewCvar( "s_musicvolume", "1", CvarFlag_Archive );
	s_muteinbackground = NewCvar( "s_muteinbackground", "1", CvarFlag_Archive );

	if( !InitAudioBackend() ) {
		Com_Printf( S_COLOR_RED "Couldn't initialize audio backend!\n" );
		return;
	}

	InitOpenAL();
	LoadSounds();
	LoadSoundEffects();

	backend_initialized = true;
	backend_device_initialized = InitAudioDevice( s_device->value, AudioCallback, NULL );
}

void ShutdownSound() {
	TracyZoneScoped;

	if( !backend_initialized ) {
		return;
	}

	if( backend_device_initialized ) {
		ShutdownAudioDevice();
	}

	StopAllSounds( true );

	for( size_t i = 0; i < sounds.size(); i++ ) {
		alDeleteBuffers( 1, &sounds[ i ].buf );
		free( sounds[ i ].samples.ptr );
	}

	ShutdownOpenAL();
	ShutdownAudioBackend();
}

static const Sound * FindSound( StringHash name ) {
	return backend_initialized ? sounds.get( name.hash ) : NULL;
}

static const SoundEffect * FindSoundEffect( StringHash name ) {
	return backend_initialized ? sound_effects.get( name.hash ) : NULL;
}

static bool StartSound( PlayingSFX * ps, size_t i ) {
	SoundEffect::PlaybackConfig config = ps->sfx->sounds[ i ];

	StringHash sound_name;
	if( !ps->config.entropy.exists ) {
		sound_name = RandomElement( &cls.rng, config.sounds.span() );
	}
	else {
		RNG rng = NewRNG( ps->config.entropy.value, 0 );
		sound_name = RandomElement( &rng, config.sounds.span() );
	}

	const Sound * sound = FindSound( sound_name );
	if( sound == NULL )
		return false;

	if( free_sound_sources.size() == 0 ) {
		Com_Printf( S_COLOR_YELLOW "Too many playing sounds!\n" );
		return false;
	}

	if( !sound->mono && ps->config.spatialisation != SpatialisationMethod_None ) {
		Com_Printf( S_COLOR_YELLOW "Positioned sounds must be mono!\n" );
		return false;
	}

	ALuint source = free_sound_sources.pop();
	ps->sources[ i ] = source;

	CheckedALSource( source, AL_BUFFER, sound->buf );
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

static void StopSound( PlayingSFX * ps, size_t i ) {
	CheckedALSourceStop( ps->sources[ i ] );
	CheckedALSource( ps->sources[ i ], AL_BUFFER, 0 );
	[[maybe_unused]] bool ok = free_sound_sources.add( ps->sources[ i ] );
	ps->stopped[ i ] = true;
}

static void StopSFX( PlayingSFX * ps ) {
	for( size_t i = 0; i < ps->sfx->sounds.size(); i++ ) {
		if( ps->started[ i ] && !ps->stopped[ i ] ) {
			StopSound( ps, i );
		}
	}

	playing_sounds.remove( ps->handle.handle );
}

static void UpdateSound( PlayingSFX * ps, float volume, float pitch ) {
	ps->config.volume = volume;
	ps->config.pitch = pitch;

	for( size_t i = 0; i < ps->sfx->sounds.size(); i++ ) {
		if( ps->started[ i ] ) {
			const SoundEffect::PlaybackConfig * config = &ps->sfx->sounds[ i ];
			CheckedALSource( ps->sources[ i ], AL_GAIN, ps->config.volume * config->volume * s_volume->number );
			CheckedALSource( ps->sources[ i ], AL_PITCH, ps->config.pitch * config->pitch + ( RandomFloat11( &cls.rng ) * config->pitch_random * config->pitch * ps->config.pitch ) );
		}
	}
}

void SoundFrame( Vec3 origin, Vec3 velocity, Vec3 forward, Vec3 up ) {
	TracyZoneScoped;

	if( !backend_initialized )
		return;

	if( s_device->modified ) {
		ShutdownAudioDevice();
		backend_device_initialized = InitAudioDevice( s_device->value, AudioCallback, NULL );
		s_device->modified = false;
	}

	HotloadSounds();
	HotloadSoundEffects();

	CheckedALListener( AL_GAIN, IsWindowFocused() || s_muteinbackground->integer == 0 ? 1 : 0 );

	CheckedALListener( AL_POSITION, origin );
	CheckedALListener( AL_VELOCITY, velocity );
	CheckedALListenerOrientation( forward, up );

	for( size_t i = 0; i < playing_sounds.size(); i++ ) {
		PlayingSFX * ps = &playing_sounds[ i ];
		Time t = cls.monotonicTime - ps->start_time;
		bool all_stopped = true;

		for( size_t j = 0; j < ps->sfx->sounds.size(); j++ ) {
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

		for( size_t j = 0; j < ps->sfx->sounds.size(); j++ ) {
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

static PlayingSFX * PlaySFXInternal( StringHash name, const PlaySFXConfig & config ) {
	if( !backend_device_initialized )
		return NULL;

	const SoundEffect * sfx = FindSoundEffect( name );
	if( sfx == NULL )
		return NULL;

	u64 handle = Hash64( playing_sound_handle_autoinc );

	PlayingSFX * ps = playing_sounds.add( handle );
	if( ps == NULL ) {
		Com_Printf( S_COLOR_YELLOW "Too many playing sound effects!\n" );
		return NULL;
	}

	*ps = PlayingSFX {
		.handle = { handle },
		.config = config,
		.hash = name,
		.sfx = sfx,
		.start_time = cls.monotonicTime,
	};

	playing_sound_handle_autoinc++;

	return ps;
}

PlayingSFXHandle PlaySFX( StringHash name, const PlaySFXConfig & config ) {
	PlayingSFX * ps = PlaySFXInternal( name, config );
	if( ps == NULL )
		return { };
	return ps->handle;
}

PlayingSFXHandle PlayImmediateSFX( StringHash name, PlayingSFXHandle handle, const PlaySFXConfig & config ) {
	PlayingSFX * playing = playing_sounds.get( handle.handle );
	if( playing != NULL ) {
		if( playing->hash == name ) {
			playing->keep_playing_immediate = true;
			UpdateSound( playing, config.volume, config.pitch );
			return handle;
		}
		StopSFX( playing );
	}

	PlayingSFX * ps = PlaySFXInternal( name, config );
	if( ps == NULL )
		return { };

	ps->immediate = true;
	ps->keep_playing_immediate = true;

	return ps->handle;
}

void StopSFX( PlayingSFXHandle handle ) {
	PlayingSFX * ps = playing_sounds.get( handle.handle );
	if( ps != NULL ) {
		StopSFX( ps );
	}
}

void StopAllSounds( bool stop_music ) {
	if( !backend_initialized )
		return;

	while( playing_sounds.size() > 0 ) {
		StopSFX( &playing_sounds[ 0 ] );
	}

	if( stop_music ) {
		StopMenuMusic();
	}

	playing_sounds.clear();
}

void StartMenuMusic() {
	if( !backend_device_initialized || music_playing )
		return;

	const Sound * music = FindSound( "sounds/music/longcovid" );
	if( music == NULL )
		return;

	CheckedALSource( music_source, AL_GAIN, s_volume->number * s_musicvolume->number * MusicIsWayTooLoud );
	CheckedALSource( music_source, AL_DIRECT_CHANNELS_SOFT, AL_REMIX_UNMATCHED_SOFT );
	CheckedALSource( music_source, AL_LOOPING, AL_TRUE );
	CheckedALSource( music_source, AL_BUFFER, music->buf );

	CheckedALSourcePlay( music_source );

	music_playing = true;
}

void StopMenuMusic() {
	if( backend_device_initialized && music_playing ) {
		CheckedALSourceStop( music_source );
		CheckedALSource( music_source, AL_BUFFER, 0 );
	}
	music_playing = false;
}
