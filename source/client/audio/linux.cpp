#include "qcommon/platform.h"

#if PLATFORM_LINUX

#include "qcommon/base.h"
#include "qcommon/threads.h"

#include "client/audio/backend.h"

#include <atomic>
#include <alloca.h>
#include <dlfcn.h>
#include <alsa/asoundlib.h>

struct PulseAPI {
	void * lib;
};

struct PulseBackend {
	PulseAPI api;
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
	decltype( snd_pcm_hw_params_set_rate_near ) * hw_params_set_rate_near;
	decltype( snd_pcm_hw_params ) * hw_params;

	decltype( snd_pcm_prepare ) * prepare;
	decltype( snd_pcm_writei ) * writei;
	decltype( snd_pcm_drain ) * drain;
};

struct AlsaBackend {
	AlsaAPI api;
	u32 sample_rate;
	snd_pcm_t * device;
	Span< Vec2 > buffer;
	Thread * thread;
	std::atomic< bool > shutting_down;
};

static AudioBackendCallback audio_callback;

static PulseBackend pulse;
static AlsaBackend alsa;

template< typename F >
static bool LoadFunction( void * lib, F ** f, const char * name ) {
	*f = ( F * ) dlsym( lib, name );
	if( *f == NULL ) {
		printf( "can't find %s\n", name );
	}
	return *f != NULL;
}

static void CloseLib( void * lib ) {
	if( dlclose( lib ) != 0 ) {
		Fatal( "dlclose: %s\n", dlerror() );
	}
}

static Optional< PulseAPI > LoadPulseAPI() {
	return NONE;
}

static bool InitPulse( const char * preferred_device, void * user_data ) {
	Optional< PulseAPI > api = LoadPulseAPI();
	if( !api.exists )
		return false;

	pulse.api = api.value;

	return true;
}

static Optional< AlsaAPI > LoadAlsaAPI() {
	void * alsa = dlopen( "libasound.so", RTLD_NOW );
	if( alsa == NULL )
		return NONE;

	AlsaAPI api = { .lib = alsa };

	bool ok = true;
	ok = ok && LoadFunction( alsa, &api.strerror, "snd_strerror" );
	ok = ok && LoadFunction( alsa, &api.set_error_handler, "snd_lib_error_set_handler" );
	ok = ok && LoadFunction( alsa, &api.open, "snd_pcm_open" );
	ok = ok && LoadFunction( alsa, &api.close, "snd_pcm_close" );
	ok = ok && LoadFunction( alsa, &api.hw_params_any, "snd_pcm_hw_params_any" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_access, "snd_pcm_hw_params_set_access" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_format, "snd_pcm_hw_params_set_format" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_buffer_size, "snd_pcm_hw_params_set_buffer_size" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_channels, "snd_pcm_hw_params_set_channels" );
	ok = ok && LoadFunction( alsa, &api.hw_params_set_rate_near, "snd_pcm_hw_params_set_rate_near" );
	ok = ok && LoadFunction( alsa, &api.hw_params, "snd_pcm_hw_params" );
	ok = ok && LoadFunction( alsa, &api.prepare, "snd_pcm_prepare" );
	ok = ok && LoadFunction( alsa, &api.writei, "snd_pcm_writei" );
	ok = ok && LoadFunction( alsa, &api.drain, "snd_pcm_drain" );

	if( !ok ) {
		CloseLib( alsa );
	}

	return ok ? api : Optional< AlsaAPI >( NONE );
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
	while( !alsa.shutting_down.load( std::memory_order_acquire ) ) {
		audio_callback( alsa.buffer, alsa.sample_rate, user_data );
		int write_res = alsa.api.writei( alsa.device, alsa.buffer.ptr, checked_cast< snd_pcm_uframes_t >( alsa.buffer.n ) );
		if( write_res == -EPIPE ) {
			alsa.api.prepare( alsa.device );
		}
		else {
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

	snd_pcm_hw_params_t * params = ( snd_pcm_hw_params_t * ) alloca( alsa.api.hw_params_sizeof() );
	memset( params, 0, alsa.api.hw_params_sizeof() );
	alsa.api.hw_params_any( device, params );

	bool ok = true;
	ok = ok && AlsaChecked( alsa.api.hw_params_set_access( device, params, SND_PCM_ACCESS_RW_INTERLEAVED ), "snd_pcm_hw_params_set_access" );
	ok = ok && AlsaChecked( alsa.api.hw_params_set_format( device, params, SND_PCM_FORMAT_FLOAT_LE ), "snd_pcm_alsa.api.hw_params_set_format" );
	ok = ok && AlsaChecked( alsa.api.hw_params_set_buffer_size( device, params, checked_cast< snd_pcm_uframes_t >( AudioBackendBufferSize ) ), "snd_pcm_hw_params_set_buffer_size" );
	ok = ok && AlsaChecked( alsa.api.hw_params_set_channels( device, params, 2 ), "snd_pcm_api.hw_params_set_channels" );
	u32 sample_rate = AudioBackendSampleRate;
	ok = ok && AlsaChecked( alsa.api.hw_params_set_rate_near( device, params, &sample_rate, NULL ), "snd_pcm_api.hw_params_set_rate_near" );
	ok = ok && AlsaChecked( alsa.api.hw_params( device, params ), "snd_pcm_api.hw_params" );

	if( !ok ) {
		alsa.api.close( device );
		return false;
	}

	alsa.sample_rate = sample_rate;
	alsa.device = device;
	alsa.buffer = AllocSpan< Vec2 >( sys_allocator, AudioBackendBufferSize );
	alsa.thread = NewThread( AlsaThread, user_data );
	alsa.shutting_down.store( false, std::memory_order_release );

	return true;
}

bool InitAudioBackend( const char * preferred_device, AudioBackendCallback callback, void * user_data ) {
	audio_callback = callback;

	if( InitPulse( preferred_device, user_data ) )
		return true;
	return InitAlsa( preferred_device, user_data );
};

void ShutdownAudioBackend() {
	if( pulse.api.lib != NULL ) {
		CloseLib( pulse.api.lib );
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

#endif // #if PLATFORM_LINUX
