#include "qcommon/platform.h"

#if PLATFORM_LINUX

#include "qcommon/base.h"
#include "qcommon/threads.h"
#include "client/audio/backend.h"
#include "gameshared/q_shared.h"

#include <atomic>
#include <dlfcn.h>
#include <alsa/asoundlib.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

// TODO: implement these properly
bool InitAudioBackend() {
	return true;
}

void ShutdownAudioBackend() {
}

struct PulseAPI {
	void * main_lib;
	void * simple_lib;

	decltype( pa_strerror ) * strerror;

	decltype( pa_simple_new ) * simple_new;
	decltype( pa_simple_free ) * simple_free;
	decltype( pa_simple_write ) * simple_write;
	decltype( pa_simple_drain ) * simple_drain;
};

struct PulseBackend {
	PulseAPI api;
	Thread * thread;
	std::atomic< bool > shutting_down;
};

struct AlsaAPI {
	void * lib;

	decltype( snd_lib_error_set_handler ) * set_error_handler;
	decltype( snd_strerror ) * strerror;

	decltype( snd_pcm_open ) * open;
	decltype( snd_pcm_close ) * close;

	decltype( snd_pcm_hw_params_sizeof ) * hw_params_sizeof;
	decltype( snd_pcm_hw_params_any ) * hw_params_any;
	decltype( snd_pcm_hw_params_set_access ) * hw_params_set_access;
	decltype( snd_pcm_hw_params_set_format ) * hw_params_set_format;
	decltype( snd_pcm_hw_params_set_buffer_size ) * hw_params_set_buffer_size;
	decltype( snd_pcm_hw_params_set_channels ) * hw_params_set_channels;
	decltype( snd_pcm_hw_params_set_rate ) * hw_params_set_rate;
	decltype( snd_pcm_hw_params ) * hw_params;

	decltype( snd_pcm_prepare ) * prepare;
	decltype( snd_pcm_writei ) * writei;
	decltype( snd_pcm_drain ) * drain;
};

struct AlsaBackend {
	AlsaAPI api;
	snd_pcm_t * device;
	Span< Vec2 > buffer;
	Thread * thread;
	std::atomic< bool > shutting_down;
};

static AudioBackendCallback audio_callback;

static PulseBackend pulse;
static AlsaBackend alsa;

static char * pulse_error;
static char * alsa_error;

template< typename F >
static bool LoadFunction( void * lib, F ** f, const char * name ) {
	if( lib == NULL ) {
		return false;
	}

	*f = ( F * ) dlsym( lib, name );
	if( *f == NULL ) {
		printf( "can't find %s\n", name );
	}
	return *f != NULL;
}

static void CloseLib( void * lib ) {
	if( lib != NULL && dlclose( lib ) != 0 ) {
		Fatal( "dlclose: %s\n", dlerror() );
	}
}

static Optional< PulseAPI > LoadPulseAPI() {
	void * pulse = dlopen( "libpulse.so", RTLD_NOW );
	void * simple = dlopen( "libpulse-simple.so", RTLD_NOW );

	PulseAPI api = { .main_lib = pulse, .simple_lib = simple };
	bool ok = true;
	ok = ok && LoadFunction( pulse, &api.strerror, "pa_strerror" );
	ok = ok && LoadFunction( simple, &api.simple_new, "pa_simple_new" );
	ok = ok && LoadFunction( simple, &api.simple_free, "pa_simple_free" );
	ok = ok && LoadFunction( simple, &api.simple_write, "pa_simple_write" );
	ok = ok && LoadFunction( simple, &api.simple_drain, "pa_simple_drain" );

	if( !ok ) {
		CloseLib( pulse );
		CloseLib( simple );
		pulse_error = CopyString( sys_allocator, dlerror() );
		return NONE;
	}

	return api;
}

struct PulseThreadData {
	const char * preferred_device;
	void * user_data;
};

static void PulseThread( void * opaque ) {
	TracyCSetThreadName( "PulseAudio" );

	PulseThreadData * thread_data = ( PulseThreadData * ) opaque;
	defer { Free( sys_allocator, thread_data ); };

	pa_sample_spec sample_spec = {
		.format = PA_SAMPLE_FLOAT32NE,
		.rate = 44100,
		.channels = 2,
	};

	pa_buffer_attr buffer_attr = {
		.maxlength = u32( -1 ),
		.tlength = AUDIO_BACKEND_BUFFER_SIZE * sizeof( Vec2 ),
		.prebuf = u32( -1 ),
		.minreq = u32( -1 ),
		.fragsize = AUDIO_BACKEND_BUFFER_SIZE * sizeof( Vec2 ),
	};

	int err;
	pa_simple * simple = pulse.api.simple_new( NULL, "Cocaine Diesel", PA_STREAM_PLAYBACK, thread_data->preferred_device, "Cocaine Diesel", &sample_spec, NULL, &buffer_attr, &err );
	if( simple == NULL && err == PA_ERR_NOENTITY ) {
		simple = pulse.api.simple_new( NULL, "Cocaine Diesel", PA_STREAM_PLAYBACK, thread_data->preferred_device, "Cocaine Diesel", &sample_spec, NULL, &buffer_attr, &err );
	}

	if( simple == NULL ) {
		Fatal( "pa_simple_new: %s", pulse.api.strerror( err ) );
	}

	while( !pulse.shutting_down.load( std::memory_order_acquire ) ) {
		Vec2 buffer[ AUDIO_BACKEND_BUFFER_SIZE ];
		audio_callback( Span< Vec2 >( buffer, ARRAY_COUNT( buffer ) ), thread_data->user_data );
		if( pulse.api.simple_write( simple, buffer, sizeof( buffer ), &err ) < 0 ) {
			Fatal( "pa_simple_write: %s", pulse.api.strerror( err ) );
		}
	}

	if( pulse.api.simple_drain( simple, &err ) < 0 ) {
		Fatal( "pa_simple_drain: %s", pulse.api.strerror( err ) );
	}

	pulse.api.simple_free( simple );
}

static bool InitPulse( const char * preferred_device, void * user_data ) {
	Optional< PulseAPI > api = LoadPulseAPI();
	if( !api.exists )
		return false;

	PulseThreadData * thread_data = Alloc< PulseThreadData >( sys_allocator );
	*thread_data = {
		.preferred_device = StrEqual( preferred_device, "" ) ? NULL : preferred_device,
		.user_data = user_data,
	};

	pulse.api = api.value;
	pulse.shutting_down.store( false, std::memory_order_release );
	pulse.thread = NewThread( PulseThread, thread_data );

	return true;
}

static Optional< AlsaAPI > LoadAlsaAPI() {
	void * alsa = dlopen( "libasound.so", RTLD_NOW );

	AlsaAPI api = { .lib = alsa };
	bool ok = true;
	ok = ok && LoadFunction( alsa, &api.strerror, "snd_strerror" );
	ok = ok && LoadFunction( alsa, &api.set_error_handler, "snd_lib_error_set_handler" );
	ok = ok && LoadFunction( alsa, &api.open, "snd_pcm_open" );
	ok = ok && LoadFunction( alsa, &api.close, "snd_pcm_close" );
	ok = ok && LoadFunction( alsa, &api.hw_params_sizeof, "snd_pcm_hw_params_sizeof" );
	ok = ok && LoadFunction( alsa, &api.hw_params_any, "snd_pcm_hw_params_any" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_access, "snd_pcm_hw_params_set_access" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_format, "snd_pcm_hw_params_set_format" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_buffer_size, "snd_pcm_hw_params_set_buffer_size" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_channels, "snd_pcm_hw_params_set_channels" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_rate, "snd_pcm_hw_params_set_rate" );
	ok = ok && LoadFunction( alsa, &api.hw_params, "snd_pcm_hw_params" );
	ok = ok && LoadFunction( alsa, &api.prepare, "snd_pcm_prepare" );
	ok = ok && LoadFunction( alsa, &api.writei, "snd_pcm_writei" );
	ok = ok && LoadFunction( alsa, &api.drain, "snd_pcm_drain" );

	if( !ok ) {
		CloseLib( alsa );
		alsa_error = CopyString( sys_allocator, dlerror() );
		return NONE;
	}

	return api;
}

// static void AlsaErrorCallback( const char * file, int line, const char * function, int err, const char * fmt, ... ) {
// 	printf( "error in %s at %s:%d\n", file, function, line );
//
// 	va_list args;
// 	va_start( args, fmt );
// 	vprintf( fmt, args );
// 	va_end( args );
// 	printf( "\n" );
// }

static void AlsaThread( void * user_data ) {
	TracyCSetThreadName( "ALSA" );

	while( !alsa.shutting_down.load( std::memory_order_acquire ) ) {
		audio_callback( alsa.buffer, user_data );
		int write_res = alsa.api.writei( alsa.device, alsa.buffer.ptr, checked_cast< snd_pcm_uframes_t >( alsa.buffer.n ) );
		if( write_res == -EPIPE ) {
			alsa.api.prepare( alsa.device );
		}
		else if( write_res < 0 ) {
			Fatal( "snd_pcm_writei = %d", write_res );
		}
	}
}

static bool AlsaChecked( int ret, const char * name ) {
	if( ret != 0 ) {
		printf( "ALSA error %s: %s\n", name, alsa.api.strerror( ret ) );
		return false;
	}
	return true;
}

static bool InitAlsa( const char * preferred_device, void * user_data ) {
	Optional< AlsaAPI > api = LoadAlsaAPI();
	if( !api.exists )
		return false;

	alsa.api = api.value;

	// alsa.api.set_error_handler( AlsaErrorCallback );

	snd_pcm_t * device;
	if( !AlsaChecked( alsa.api.open( &device, "default", SND_PCM_STREAM_PLAYBACK, 0 ), "snd_pcm_open" ) ) {
		return false;
	}

	snd_pcm_hw_params_t * params = ( snd_pcm_hw_params_t * ) sys_allocator->allocate( alsa.api.hw_params_sizeof(), 16 );
	defer { Free( sys_allocator, params ); };
	memset( params, 0, alsa.api.hw_params_sizeof() );
	alsa.api.hw_params_any( device, params );

	bool ok = true;
	ok = ok && AlsaChecked( alsa.api.hw_params_set_access( device, params, SND_PCM_ACCESS_RW_INTERLEAVED ), "snd_pcm_hw_params_set_access" );
	ok = ok && AlsaChecked( alsa.api.hw_params_set_format( device, params, SND_PCM_FORMAT_FLOAT_LE ), "snd_pcm_alsa.api.hw_params_set_format" );
	ok = ok && AlsaChecked( alsa.api.hw_params_set_buffer_size( device, params, checked_cast< snd_pcm_uframes_t >( AUDIO_BACKEND_BUFFER_SIZE ) ), "snd_pcm_hw_params_set_buffer_size" );
	ok = ok && AlsaChecked( alsa.api.hw_params_set_channels( device, params, 2 ), "snd_pcm_api.hw_params_set_channels" );
	ok = ok && AlsaChecked( alsa.api.hw_params_set_rate( device, params, AUDIO_BACKEND_SAMPLE_RATE, 0 ), "snd_pcm_api.hw_params_set_rate" );
	ok = ok && AlsaChecked( alsa.api.hw_params( device, params ), "snd_pcm_api.hw_params" );

	if( !ok ) {
		alsa.api.close( device );
		return false;
	}

	alsa.device = device;
	alsa.buffer = AllocSpan< Vec2 >( sys_allocator, AUDIO_BACKEND_BUFFER_SIZE );
	alsa.shutting_down.store( false, std::memory_order_release );
	alsa.thread = NewThread( AlsaThread, user_data );

	return true;
}

bool InitAudioDevice( const char * preferred_device, AudioBackendCallback callback, void * user_data ) {
	audio_callback = callback;

	pulse_error = NULL;
	alsa_error = NULL;
	defer {
		if( pulse_error != NULL && alsa_error != NULL ) {
			printf( "%s\n%s\n", pulse_error, alsa_error );
			Free( sys_allocator, pulse_error );
			Free( sys_allocator, alsa_error );
		}
	};

	if( InitPulse( preferred_device, user_data ) )
		return true;
	return InitAlsa( preferred_device, user_data );
};

void ShutdownAudioDevice() {
	if( pulse.api.main_lib != NULL ) {
		pulse.shutting_down.store( true, std::memory_order_release );
		JoinThread( pulse.thread );
		CloseLib( pulse.api.main_lib );
		CloseLib( pulse.api.simple_lib );
	}

	if( alsa.api.lib != NULL ) {
		alsa.shutting_down.store( true, std::memory_order_release );
		JoinThread( alsa.thread );
		alsa.api.drain( alsa.device );
		alsa.api.close( alsa.device );
		CloseLib( alsa.api.lib );
		Free( sys_allocator, alsa.buffer.ptr );
	}
};

Span< const char * > GetAudioDeviceNames( Allocator * a ) {
	return Span< const char * >();
}

#endif // #if PLATFORM_LINUX
