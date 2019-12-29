#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/assets.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "client/client.h"
#include "client/sound.h"
#include "gameshared/gs_public.h"

#define AL_LIBTYPE_STATIC
#include "openal/al.h"
#include "openal/alc.h"
#include "openal/alext.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.h"

struct SoundEffect {
	struct PlaybackConfig {
		StringHash sounds[ 4 ];
		u8 num_random_sounds;

		float delay;
		float volume;
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

	bool immediate;
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
	PlayingSound * immediate_ps;
};

static ALCdevice * al_device;
static ALCcontext * al_context;

// so we don't crash when some other application is running in exclusive playback mode (WASAPI/JACK/etc)
static bool initialized;

static cvar_t * s_volume;
static cvar_t * s_musicvolume;
static cvar_t * s_muteinbackground;

constexpr u32 MAX_SOUND_ASSETS = 4096;
constexpr u32 MAX_SOUND_EFFECTS = 4096;
constexpr u32 MAX_PLAYING_SOUNDS = 128;

static ALuint sounds[ MAX_SOUND_ASSETS ];
static u32 num_sounds;
static Hashtable< MAX_SOUND_ASSETS * 2 > sounds_hashtable;

static SoundEffect sound_effects[ MAX_SOUND_EFFECTS ];
static u32 num_sound_effects;
static Hashtable< MAX_SOUND_EFFECTS * 2 > sound_effects_hashtable;

static ALuint free_sound_sources[ MAX_PLAYING_SOUNDS ];
static u32 num_free_sound_sources;

static PlayingSound playing_sound_effects[ MAX_PLAYING_SOUNDS ];
static u32 num_playing_sound_effects;

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

static void ALAssert() {
	ALenum err = alGetError();
	if( err != AL_NO_ERROR ) {
		Sys_Error( "%s", S_ErrorMessage( err ) );
	}
}

static bool S_InitAL() {
	ZoneScoped;

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

static void LoadSound( const char * path, bool allow_stereo ) {
	ZoneScoped;
	ZoneText( path, strlen( path ) );

	assert( num_sounds < ARRAY_COUNT( sounds ) );

	Span< const u8 > ogg = AssetBinary( path );

	int channels, sample_rate, num_samples;
	s16 * samples;
	{
		ZoneScopedN( "stb_vorbis_decode_memory" );
		num_samples = stb_vorbis_decode_memory( ogg.ptr, ogg.num_bytes(), &channels, &sample_rate, &samples );
	}
	if( num_samples == -1 ) {
		Com_Printf( S_COLOR_RED "Couldn't decode sound %s\n", path );
		return;
	}

	defer { free( samples ); };

	if( !allow_stereo && channels != 1 ) {
		Com_Printf( S_COLOR_RED "Couldn't load sound %s: needs to be a mono file!\n", path );
		return;
	}

	u64 hash = Hash64( path, strlen( path ) - strlen( ".ogg" ) );

	bool restart_music = false;

	u64 idx = num_sounds;
	if( !sounds_hashtable.get( hash, &idx ) ) {
		sounds_hashtable.add( hash, num_sounds );
		num_sounds++;

		// add simple sound effect
		SoundEffect sfx = { };
		sfx.sounds[ 0 ].sounds[ 0 ] = StringHash( hash );
		sfx.sounds[ 0 ].volume = 1;
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
		alDeleteBuffers( 1, &sounds[ idx ] );
	}

	ALenum format = channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
	alGenBuffers( 1, &sounds[ idx ] );
	alBufferData( sounds[ idx ], format, samples, num_samples * channels * sizeof( s16 ), sample_rate );
	ALAssert();

	if( restart_music ) {
		S_StartMenuMusic();
	}
}

static void LoadSounds() {
	ZoneScoped;

	for( const char * path : AssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( FileExtension( path ) == ".ogg" ) {
			bool stereo = strcmp( path, "sounds/music/menu_1.ogg" ) == 0;
			LoadSound( path, stereo );
		}
	}
}

static void HotloadSounds() {
	ZoneScoped;

	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".ogg" ) {
			bool stereo = strcmp( path, "sounds/music/menu_1.ogg" ) == 0;
			LoadSound( path, stereo );
		}
	}
}

static bool ParseSoundEffect( SoundEffect * sfx, Span< const char > * data ) {
	if( sfx->num_sounds == ARRAY_COUNT( sfx->sounds ) ) {
		Com_Printf( S_COLOR_YELLOW "SFX with too many sections\n" );
		return false;
	}

	while( true ) {
		Span< const char > opening_brace = ParseToken( data, Parse_DontStopOnNewLine );
		if( opening_brace == "" )
			break;

		if( opening_brace != "{" ) {
			Com_Printf( S_COLOR_YELLOW "Expected {" );
			return false;
		}

		SoundEffect::PlaybackConfig * config = &sfx->sounds[ sfx->num_sounds ];
		config->volume = 1.0f;
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
				config->sounds[ config->num_random_sounds ] = StringHash( Hash64( value.ptr, value.n ) );
				config->num_random_sounds++;
			}
			else if( key == "delay" ) {
				if( !SpanToFloat( value, &config->delay ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to delay should be a number\n" );
					return false;
				}
			}
			else if( key == "volume" ) {
				if( !SpanToFloat( value, &config->volume ) ) {
					Com_Printf( S_COLOR_YELLOW "Argument to volume should be a number\n" );
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
	ZoneScoped;
	ZoneText( path, strlen( path ) );

	Span< const char > data = AssetString( path );

	SoundEffect sfx = { };

	if( !ParseSoundEffect( &sfx, &data ) ) {
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
	ZoneScoped;

	for( const char * path : AssetPaths() ) {
		if( FileExtension( path ) == ".cdsfx" ) {
			LoadSoundEffect( path );
		}
	}
}

static void HotloadSoundEffects() {
	ZoneScoped;

	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".cdsfx" ) {
			LoadSoundEffect( path );
		}
	}
}

bool S_Init() {
	ZoneScoped;

	num_sounds = 0;
	num_sound_effects = 0;
	num_playing_sound_effects = 0;
	music_playing = false;
	window_focused = true;
	initialized = false;

	memset( entities, 0, sizeof( entities ) );

	s_volume = Cvar_Get( "s_volume", "1", CVAR_ARCHIVE );
	s_musicvolume = Cvar_Get( "s_musicvolume", "1", CVAR_ARCHIVE );
	s_muteinbackground = Cvar_Get( "s_muteinbackground", "1", CVAR_ARCHIVE );
	s_muteinbackground->modified = true;

	if( !S_InitAL() )
		return false;

	LoadSounds();
	LoadSoundEffects();

	initialized = true;

	return true;
}

void S_Shutdown() {
	if( !initialized )
		return;

	S_StopAllSounds( true );

	alDeleteSources( ARRAY_COUNT( free_sound_sources ), free_sound_sources );
	alDeleteSources( 1, &music_source );
	alDeleteBuffers( num_sounds, sounds );

	ALAssert();

	alcDestroyContext( al_context );
	alcCloseDevice( al_device );
}

static bool FindSound( StringHash name, ALuint * buffer ) {
	u64 idx;
	if( !initialized || !sounds_hashtable.get( name.hash, &idx ) )
		return false;
	*buffer = sounds[ idx ];
	return true;
}

const SoundEffect * FindSoundEffect( StringHash name ) {
	u64 idx;
	if( !initialized || !sound_effects_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &sound_effects[ idx ];
}

const SoundEffect * FindSoundEffect( const char * name ) {
	return FindSoundEffect( StringHash( name ) );
}

static void StartSound( PlayingSound * ps, u8 i ) {
	SoundEffect::PlaybackConfig config = ps->sfx->sounds[ i ];

	int idx = random_uniform( &cls.rng, 0, config.num_random_sounds );
	ALuint buffer;
	if( !FindSound( config.sounds[ idx ], &buffer ) )
		return;

	if( num_free_sound_sources == 0 ) {
		Com_Printf( S_COLOR_YELLOW "Too many playing sounds!" );
		return;
	}

	num_free_sound_sources--;
	ALuint source = free_sound_sources[ num_free_sound_sources ];
	ps->sources[ i ] = source;
	ps->started[ i ] = true;

	alSourcei( source, AL_BUFFER, buffer );
	alSourcef( source, AL_GAIN, ps->volume * config.volume * s_volume->value );
	alSourcef( source, AL_REFERENCE_DISTANCE, S_DEFAULT_ATTENUATION_REFDISTANCE );
	alSourcef( source, AL_MAX_DISTANCE, S_DEFAULT_ATTENUATION_MAXDISTANCE );
	alSourcef( source, AL_ROLLOFF_FACTOR, config.attenuation );

	switch( ps->type ) {
		case PlayingSoundType_Global:
			alSource3f( source, AL_POSITION, 0.0f, 0.0f, 0.0f );
			alSource3f( source, AL_VELOCITY, 0.0f, 0.0f, 0.0f );
			alSourcei( source, AL_SOURCE_RELATIVE, AL_TRUE );
			break;

		case PlayingSoundType_Position:
			alSourcefv( source, AL_POSITION, ps->origin.ptr() );
			alSource3f( source, AL_VELOCITY, 0.0f, 0.0f, 0.0f );
			alSourcei( source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;

		case PlayingSoundType_Entity:
			alSourcefv( source, AL_POSITION, entities[ ps->ent_num ].origin.ptr() );
			alSourcefv( source, AL_VELOCITY, entities[ ps->ent_num ].velocity.ptr() );
			alSourcei( source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;

		case PlayingSoundType_Line:
			alSourcefv( source, AL_POSITION, ps->origin.ptr() );
			alSource3f( source, AL_VELOCITY, 0.0f, 0.0f, 0.0f ); // TODO
			alSourcei( source, AL_SOURCE_RELATIVE, AL_FALSE );
			break;
	}

	alSourcei( source, AL_LOOPING, ps->immediate ? AL_TRUE : AL_FALSE );

	alSourcePlay( source );
	ALAssert();
}

static void StopSound( PlayingSound * ps, u8 i ) {
	alSourceStop( ps->sources[ i ] );
	alSourcei( ps->sources[ i ], AL_BUFFER, 0 );
	free_sound_sources[ num_free_sound_sources ] = ps->sources[ i ];
	num_free_sound_sources++;
	ps->stopped[ i ] = true;
}

void S_Update( Vec3 origin, Vec3 velocity, const mat3_t axis ) {
	ZoneScoped;

	if( !initialized )
		return;

	HotloadSounds();
	HotloadSoundEffects();

	if( s_muteinbackground->modified ) {
		alListenerf( AL_GAIN, window_focused || s_muteinbackground->integer == 0 ? 1 : 0 );
		s_muteinbackground->modified = false;
	}

	float orientation[ 6 ];
	VectorCopy( &axis[ AXIS_FORWARD ], &orientation[ 0 ] );
	VectorCopy( &axis[ AXIS_UP ], &orientation[ 3 ] );

	alListenerfv( AL_POSITION, origin.ptr() );
	alListenerfv( AL_VELOCITY, velocity.ptr() );
	alListenerfv( AL_ORIENTATION, orientation );

	for( size_t i = 0; i < num_playing_sound_effects; i++ ) {
		PlayingSound * ps = &playing_sound_effects[ i ];
		float t = ( cls.monotonicTime - ps->start_time ) * 0.001f;
		bool all_stopped = true;

		bool not_touched = ps->immediate && !ps->touched_since_last_update;
		ps->touched_since_last_update = false;

		for( u8 j = 0; j < ps->sfx->num_sounds; j++ ) {
			if( ps->started[ j ] ) {
				if( ps->stopped[ j ] )
					continue;

				ALint state;
				alGetSourcei( ps->sources[ j ], AL_SOURCE_STATE, &state );
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
				StartSound( ps, j );
			}
		}

		if( all_stopped ) {
			// stop the current sound
			if( ps->immediate ) {
				entities[ ps->ent_num ].immediate_ps = NULL;
			}

			// remove-swap it from playing_sound_effects
			num_playing_sound_effects--;
			if( ps != &playing_sound_effects[ num_playing_sound_effects ] ) {
				Swap2( ps, &playing_sound_effects[ num_playing_sound_effects ] );

				// fix up the immediate_ps pointer for the sound that got swapped in
				if( ps->immediate ) {
					entities[ ps->ent_num ].immediate_ps = ps;
				}
			}

			i--;
			continue;
		}

		for( u8 j = 0; j < ps->sfx->num_sounds; j++ ) {
			if( !ps->started[ j ] || ps->stopped[ j ] )
				continue;

			if( s_volume->modified ) {
				alSourcef( ps->sources[ j ], AL_GAIN, ps->volume * ps->sfx->sounds[ j ].volume * s_volume->value );
			}

			if( ps->type == PlayingSoundType_Entity ) {
				alSourcefv( ps->sources[ j ], AL_POSITION, entities[ ps->ent_num ].origin.ptr() );
				alSourcefv( ps->sources[ j ], AL_VELOCITY, entities[ ps->ent_num ].velocity.ptr() );
			}
			else if( ps->type == PlayingSoundType_Line ) {
				Vec3 p = ClosestPointOnSegment( ps->origin, ps->end, origin );
				alSourcefv( ps->sources[ j ], AL_POSITION, p.ptr() );
			}
		}
	}

	if( ( s_volume->modified || s_musicvolume->modified ) && music_playing ) {
		alSourcef( music_source, AL_GAIN, s_volume->value * s_musicvolume->value );
	}

	s_volume->modified = false;
	s_musicvolume->modified = false;

	ALAssert();
}

void S_UpdateEntity( int ent_num, Vec3 origin, Vec3 velocity ) {
	if( !initialized )
		return;

	entities[ ent_num ].origin  = origin;
	entities[ ent_num ].velocity = velocity;
}

void S_SetWindowFocus( bool focused ) {
	if( !initialized )
		return;

	window_focused = focused;
	alListenerf( AL_GAIN, window_focused || s_muteinbackground->integer == 0 ? 1 : 0 );
}

static PlayingSound * S_FindEmptyPlayingSound( int ent_num, int channel ) {
	if( channel != 0 ) {
		for( size_t i = 0; i < num_playing_sound_effects; i++ ) {
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

static PlayingSound * StartSoundEffect( const SoundEffect * sfx, int ent_num, int channel, float volume, float attenuation, PlayingSoundType type, bool immediate ) {
	if( !initialized || sfx == NULL )
		return NULL;

	PlayingSound * ps = S_FindEmptyPlayingSound( ent_num, channel );
	if( ps == NULL ) {
		Com_Printf( S_COLOR_YELLOW "Too many playing sound effects!" );
		return NULL;
	}

	*ps = { };
	ps->type = type;
	ps->sfx = sfx;
	ps->start_time = cls.monotonicTime;
	ps->ent_num = ent_num;
	ps->channel = channel;
	ps->volume = volume;
	ps->immediate = immediate;
	ps->touched_since_last_update = true;

	return ps;
}

void S_StartFixedSound( const SoundEffect * sfx, Vec3 origin, int channel, float volume, float attenuation ) {
	PlayingSound * ps = StartSoundEffect( sfx, 0, channel, volume, attenuation, PlayingSoundType_Position, false );
	ps->origin = origin;
}

void S_StartEntitySound( const SoundEffect * sfx, int ent_num, int channel, float volume, float attenuation ) {
	StartSoundEffect( sfx, ent_num, channel, volume, attenuation, PlayingSoundType_Entity, false );
}

void S_StartGlobalSound( const SoundEffect * sfx, int channel, float volume ) {
	StartSoundEffect( sfx, 0, channel, volume, ATTN_NONE, PlayingSoundType_Global, false );
}

void S_StartLocalSound( const SoundEffect * sfx, int channel, float volume ) {
	StartSoundEffect( sfx, -1, channel, volume, ATTN_NONE, PlayingSoundType_Global, false );
}

static PlayingSound * StartImmediateSound( const SoundEffect * sfx, int ent_num, float volume, float attenuation, PlayingSoundType type ) {
	// TODO: replace old immediate sound if sfx changed
	if( entities[ ent_num ].immediate_ps == NULL ) {
		entities[ ent_num ].immediate_ps = StartSoundEffect( sfx, ent_num, CHAN_AUTO, volume, attenuation, type, true );
	}

	if( entities[ ent_num ].immediate_ps != NULL ) {
		entities[ ent_num ].immediate_ps->touched_since_last_update = true;
	}

	return entities[ ent_num ].immediate_ps;
}

void S_ImmediateEntitySound( const SoundEffect * sfx, int ent_num, float volume, float attenuation ) {
	StartImmediateSound( sfx, ent_num, volume, attenuation, PlayingSoundType_Entity );
}

void S_ImmediateLineSound( const SoundEffect * sfx, int ent_num, Vec3 start, Vec3 end, float volume, float attenuation ) {
	PlayingSound * ps = StartImmediateSound( sfx, ent_num, volume, attenuation, PlayingSoundType_Line );
	if( ps == NULL )
		return;

	entities[ ent_num ].immediate_ps->origin = start;
	entities[ ent_num ].immediate_ps->end = end;
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

	for( EntitySound & e : entities ) {
		e.immediate_ps = NULL;
	}
}

void S_StartMenuMusic() {
	if( !initialized )
		return;

	ALuint buffer;
	if( !FindSound( "sounds/music/menu_1", &buffer ) )
		return;

	alSourcef( music_source, AL_GAIN, s_volume->value * s_musicvolume->value );
	alSourcei( music_source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE );
	alSourcei( music_source, AL_LOOPING, AL_TRUE );
	alSourcei( music_source, AL_BUFFER, buffer );

	alSourcePlay( music_source );

	music_playing = true;
}

void S_StopBackgroundTrack() {
	if( initialized && music_playing ) {
		alSourceStop( music_source );
		alSourcei( music_source, AL_BUFFER, 0 );
	}
	music_playing = false;
}
