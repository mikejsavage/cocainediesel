#pragma once

#include "qcommon/types.h"

#define _SAUDIO_LOG_ITEMS \
	_SAUDIO_LOGITEM_XMACRO(OK, "Ok") \
	_SAUDIO_LOGITEM_XMACRO(MALLOC_FAILED, "memory allocation failed") \
	_SAUDIO_LOGITEM_XMACRO(ALSA_SND_PCM_OPEN_FAILED, "snd_pcm_open() failed") \
	_SAUDIO_LOGITEM_XMACRO(ALSA_FLOAT_SAMPLES_NOT_SUPPORTED, "floating point sample format not supported") \
	_SAUDIO_LOGITEM_XMACRO(ALSA_REQUESTED_BUFFER_SIZE_NOT_SUPPORTED, "requested buffer size not supported") \
	_SAUDIO_LOGITEM_XMACRO(ALSA_REQUESTED_CHANNEL_COUNT_NOT_SUPPORTED, "requested channel count not supported") \
	_SAUDIO_LOGITEM_XMACRO(ALSA_SND_PCM_HW_PARAMS_SET_RATE_NEAR_FAILED, "snd_pcm_hw_params_set_rate_near() failed") \
	_SAUDIO_LOGITEM_XMACRO(ALSA_SND_PCM_HW_PARAMS_FAILED, "snd_pcm_hw_params() failed") \
	_SAUDIO_LOGITEM_XMACRO(ALSA_PTHREAD_CREATE_FAILED, "pthread_create() failed") \
	_SAUDIO_LOGITEM_XMACRO(WASAPI_CREATE_EVENT_FAILED, "CreateEvent() failed") \
	_SAUDIO_LOGITEM_XMACRO(WASAPI_CREATE_DEVICE_ENUMERATOR_FAILED, "CoCreateInstance() for IMMDeviceEnumerator failed") \
	_SAUDIO_LOGITEM_XMACRO(WASAPI_GET_DEFAULT_AUDIO_ENDPOINT_FAILED, "IMMDeviceEnumerator.GetDefaultAudioEndpoint() failed") \
	_SAUDIO_LOGITEM_XMACRO(WASAPI_DEVICE_ACTIVATE_FAILED, "IMMDevice.Activate() failed") \
	_SAUDIO_LOGITEM_XMACRO(WASAPI_AUDIO_CLIENT_INITIALIZE_FAILED, "IAudioClient.Initialize() failed") \
	_SAUDIO_LOGITEM_XMACRO(WASAPI_AUDIO_CLIENT_GET_BUFFER_SIZE_FAILED, "IAudioClient.GetBufferSize() failed") \
	_SAUDIO_LOGITEM_XMACRO(WASAPI_AUDIO_CLIENT_GET_SERVICE_FAILED, "IAudioClient.GetService() failed") \
	_SAUDIO_LOGITEM_XMACRO(WASAPI_AUDIO_CLIENT_SET_EVENT_HANDLE_FAILED, "IAudioClient.SetEventHandle() failed") \
	_SAUDIO_LOGITEM_XMACRO(WASAPI_CREATE_THREAD_FAILED, "CreateThread() failed") \
	_SAUDIO_LOGITEM_XMACRO(COREAUDIO_NEW_OUTPUT_FAILED, "AudioQueueNewOutput() failed") \
	_SAUDIO_LOGITEM_XMACRO(COREAUDIO_ALLOCATE_BUFFER_FAILED, "AudioQueueAllocateBuffer() failed") \
	_SAUDIO_LOGITEM_XMACRO(COREAUDIO_START_FAILED, "AudioQueueStart() failed") \

#define _SAUDIO_LOGITEM_XMACRO(item,msg) SAUDIO_LOGITEM_##item,
enum saudio_log_item {
	_SAUDIO_LOG_ITEMS
};
#undef _SAUDIO_LOGITEM_XMACRO

/*
   saudio_logger

   Used in saudio_desc to provide a custom logging and error reporting
   callback to sokol-audio.
   */
struct saudio_logger {
	void (*func)(
		const char* tag,                // always "saudio"
		uint32_t log_item_id,           // SAUDIO_LOGITEM_*
		const char* message_or_null,    // a message string, may be nullptr in release mode
		uint32_t line_nr,               // line number in sokol_audio.h
		const char* filename_or_null,   // source filename, may be nullptr in release mode
		void* user_data);
	void* user_data;
};

struct saudio_desc {
	const char * device_name;
	u32 sample_rate;        // requested sample rate
	size_t buffer_frames;      // number of frames in streaming buffer
	void (*callback)(Vec2 * buffer, size_t num_frames, void* user_data); //... and with user data
	void* user_data;        // optional user data argument for callback
	saudio_logger logger;           // optional logging function (default: NO LOGGING!)
};

bool saudio_setup( const saudio_desc & desc );
void saudio_shutdown();

#ifdef SOKOL_AUDIO_IMPL

#include <stdlib.h> // alloc, free
#include <string.h> // memset, memcpy
#include <stddef.h> // size_t
#include <atomic>

#include "qcommon/threads.h"

#ifndef SOKOL_DEBUG
#ifndef NDEBUG
#define SOKOL_DEBUG
#endif
#endif

// ███████ ████████ ██████  ██    ██  ██████ ████████ ███████
// ██         ██    ██   ██ ██    ██ ██         ██    ██
// ███████    ██    ██████  ██    ██ ██         ██    ███████
//      ██    ██    ██   ██ ██    ██ ██         ██         ██
// ███████    ██    ██   ██  ██████   ██████    ██    ███████

#if PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <synchapi.h>
#pragma comment (lib, "kernel32")
#pragma comment (lib, "ole32")
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#include <atomic>

struct GGAudioBackend {
	IMMDeviceEnumerator * device_enumerator;
	IMMDevice * device;
	IAudioClient * audio_client;
	IAudioRenderClient * render_client;
	Thread * thread;
	HANDLE buffer_end_event;
	UINT32 dst_buffer_frames;
};

#elif PLATFORM_MACOS

#include <AudioToolbox/AudioToolbox.h>

struct GGAudioBackend {
	AudioQueueRef ca_audio_queue;
};

#elif PLATFORM_LINUX

#include <alloca.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

struct GGAudioBackend {
	snd_pcm_t * device;
	Vec2 * buffer;
	size_t buffer_frames;
	Thread * thread;
};

#else
#error "unknown platform"
#endif

struct _saudio_state_t {
	void ( *callback )( Vec2 * buffer, size_t num_frames, void * user_data );
	void * user_data;
	u32 sample_rate;
	size_t buffer_frames;          /* number of frames in streaming buffer */
	GGAudioBackend backend;
	saudio_logger logger;
};

static _saudio_state_t _saudio;
static std::atomic< bool > shutting_down;

// ██       ██████   ██████   ██████  ██ ███    ██  ██████
// ██      ██    ██ ██       ██       ██ ████   ██ ██
// ██      ██    ██ ██   ███ ██   ███ ██ ██ ██  ██ ██   ███
// ██      ██    ██ ██    ██ ██    ██ ██ ██  ██ ██ ██    ██
// ███████  ██████   ██████   ██████  ██ ██   ████  ██████
//
// >>logging
#if defined(SOKOL_DEBUG)
#define _SAUDIO_LOGITEM_XMACRO(item,msg) #item ": " msg,
static const char* _saudio_log_messages[] = {
	_SAUDIO_LOG_ITEMS
};
#undef _SAUDIO_LOGITEM_XMACRO
#endif // SOKOL_DEBUG

#define _SAUDIO_ERROR(code) _saudio_log(SAUDIO_LOGITEM_ ##code, __LINE__)

static void _saudio_log(saudio_log_item log_item, uint32_t line_nr) {
	if (_saudio.logger.func) {
#if defined(SOKOL_DEBUG)
		const char* filename = __FILE__;
		const char* message = _saudio_log_messages[log_item];
#else
		const char* filename = 0;
		const char* message = 0;
#endif
		_saudio.logger.func("saudio", log_item, message, line_nr, filename, _saudio.logger.user_data);
	}
}

// ██     ██  █████  ███████  █████  ██████  ██
// ██     ██ ██   ██ ██      ██   ██ ██   ██ ██
// ██  █  ██ ███████ ███████ ███████ ██████  ██
// ██ ███ ██ ██   ██      ██ ██   ██ ██      ██
//  ███ ███  ██   ██ ███████ ██   ██ ██      ██

#if PLATFORM_WINDOWS

#include <stdio.h>

static void wasapi_thread( void * data ) {
	BYTE * dummy;
	_saudio.backend.render_client->GetBuffer( _saudio.backend.dst_buffer_frames, &dummy );
	_saudio.backend.render_client->ReleaseBuffer( _saudio.backend.dst_buffer_frames, AUDCLNT_BUFFERFLAGS_SILENT );

	while( !shutting_down.load( std::memory_order_acquire ) ) {
		WaitForSingleObject( _saudio.backend.buffer_end_event, INFINITE );
		UINT32 padding = 0;
		if( FAILED( _saudio.backend.audio_client->GetCurrentPadding( &padding ) ) ) {
			continue;
		}
		int num_frames = int( _saudio.backend.dst_buffer_frames ) - int( padding );

		if( num_frames > 0 ) {
			BYTE * wasapi_buffer = NULL;
			if( FAILED( _saudio.backend.render_client->GetBuffer( num_frames, &wasapi_buffer ) ) ) {
				continue;
			}

			_saudio.callback( ( Vec2 * ) wasapi_buffer, num_frames, _saudio.user_data );
			_saudio.backend.render_client->ReleaseBuffer( num_frames, 0 );
		}
	}

	return 0;
}

static void _saudio_wasapi_release() {
	_saudio.backend.render_client->Release();
	_saudio.backend.audio_client->Release();
	_saudio.backend.device->Release();
	_saudio.backend.device_enumerator->Release();
	CloseHandle( _saudio.backend.buffer_end_event );
}

static IMMDevice * OpenDeviceOrDefault( const char * preferred_device_name ) {
	IMMDeviceCollection * devices;
	if( FAILED( _saudio.backend.device_enumerator->EnumAudioEndpoints( eRender, DEVICE_STATE_ACTIVE, &devices ) ) ) {
		abort();
	}

	UINT num_devices;
	devices->GetCount( &num_devices );
	for( UINT i = 0; i < num_devices; i++ ) {
		IMMDevice * device;
		devices->Item( i, &device );

		IPropertyStore * properties;
		if( FAILED( device->OpenPropertyStore( STGM_READ, &properties ) ) ) {
			abort();
		}

		PROPVARIANT device_name;
		if( FAILED( properties->GetValue( PKEY_Device_FriendlyName, &device_name ) ) ) {
			abort();
		}

		char utf8[ 256 ] = { };
		WideCharToMultiByte( CP_UTF8, 0, device_name.pwszVal, wcslen( device_name.pwszVal ), utf8, sizeof( utf8 ) - 1, NULL, NULL );

		if( strcmp( utf8, preferred_device_name ) == 0 ) {
			PropVariantClear( &device_name );
			properties->Release();
			devices->Release();
			return device;
		}

		PropVariantClear( &device_name );

		properties->Release();
		device->Release();
	}

	devices->Release();

	IMMDevice * device;
	if( FAILED( _saudio.backend.device_enumerator->GetDefaultAudioEndpoint(
			    eRender, eConsole,
			    &device ) ) )
	{
		_SAUDIO_ERROR(WASAPI_GET_DEFAULT_AUDIO_ENDPOINT_FAILED);
		abort();
	}

	return device;
}

static bool _saudio_backend_init() {
	REFERENCE_TIME dur;
	/* CoInitializeEx could have been called elsewhere already, in which
	   case the function returns with S_FALSE (thus it does not make much
	   sense to check the result)
	   TODO: only uninit if this returns true
	   */
	HRESULT hr = CoInitializeEx( 0, COINIT_MULTITHREADED );
	_saudio.backend.buffer_end_event = CreateEventA( NULL, FALSE, FALSE, NULL );
	if( _saudio.backend.buffer_end_event == NULL ) {
		_SAUDIO_ERROR(WASAPI_CREATE_EVENT_FAILED);
		goto error;
	}

	if( FAILED( CoCreateInstance( __uuidof( MMDeviceEnumerator ),
			    NULL, CLSCTX_ALL,
			    IID_IMMDeviceEnumerator,
			    (void**) &_saudio.backend.device_enumerator ) ) )
	{
		_SAUDIO_ERROR( WASAPI_CREATE_DEVICE_ENUMERATOR_FAILED );
		goto error;
	}

	_saudio.backend.device = OpenDeviceOrDefault( "hello" );
	if( FAILED( _saudio.backend.device->Activate(
			    __uuidof( IAudioClient ),
			    CLSCTX_ALL, 0,
			    (void**)&_saudio.backend.audio_client ) ) )
	{
		_SAUDIO_ERROR( WASAPI_DEVICE_ACTIVATE_FAILED );
		goto error;
	}

	WAVEFORMATEXTENSIBLE fmtex = { };
	fmtex.Format.nChannels = 2;
	fmtex.Format.nSamplesPerSec = DWORD( _saudio.sample_rate );
	fmtex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	fmtex.Format.wBitsPerSample = sizeof( float ) * 8;
	fmtex.Format.nBlockAlign = ( fmtex.Format.nChannels * fmtex.Format.wBitsPerSample ) / 8;
	fmtex.Format.nAvgBytesPerSec = fmtex.Format.nSamplesPerSec * fmtex.Format.nBlockAlign;
	fmtex.Format.cbSize = 22;   /* WORD + DWORD + GUID */
	fmtex.Samples.wValidBitsPerSample = 32;
	fmtex.dwChannelMask = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;
	fmtex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	dur = REFERENCE_TIME(double(_saudio.buffer_frames) / ((double(_saudio.sample_rate) * (1.0/10000000.0));
	if (FAILED(_saudio.backend.audio_client->Initialize(
			    AUDCLNT_SHAREMODE_SHARED,
			    AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
			    dur, 0, (WAVEFORMATEX*)&fmtex, NULL)))
	{
		_SAUDIO_ERROR(WASAPI_AUDIO_CLIENT_INITIALIZE_FAILED);
		goto error;
	}
	if (FAILED(_saudio.backend.audio_client->GetBufferSize(&_saudio.backend.dst_buffer_frames))) {
		_SAUDIO_ERROR(WASAPI_AUDIO_CLIENT_GET_BUFFER_SIZE_FAILED);
		goto error;
	}
	if (FAILED(_saudio.backend.audio_client->GetService(
			    __uuidof(IAudioRenderClient),
			    (void**)&_saudio.backend.render_client)))
	{
		_SAUDIO_ERROR(WASAPI_AUDIO_CLIENT_GET_SERVICE_FAILED);
		goto error;
	}
	if (FAILED(_saudio.backend.audio_client->SetEventHandle(_saudio.backend.buffer_end_event))) {
		_SAUDIO_ERROR(WASAPI_AUDIO_CLIENT_SET_EVENT_HANDLE_FAILED);
		goto error;
	}

	_saudio.backend.audio_client->Start();
	_saudio.backend.thread = NewThread( wasapi_thread, NULL );
	return true;
	error:
		_saudio_wasapi_release();
	return false;
}

static void _saudio_backend_shutdown() {
	JoinThread( _saudio.backend.thread );
	_saudio.backend.audio_client->Stop();
	_saudio_wasapi_release();
	CoUninitialize();
}

//  ██████  ██████  ██████  ███████  █████  ██    ██ ██████  ██  ██████
// ██      ██    ██ ██   ██ ██      ██   ██ ██    ██ ██   ██ ██ ██    ██
// ██      ██    ██ ██████  █████   ███████ ██    ██ ██   ██ ██ ██    ██
// ██      ██    ██ ██   ██ ██      ██   ██ ██    ██ ██   ██ ██ ██    ██
//  ██████  ██████  ██   ██ ███████ ██   ██  ██████  ██████  ██  ██████

#elif PLATFORM_MACOS

#include <vector>

static void PrintDevices() {
	constexpr AudioObjectPropertyAddress devices_property_address = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	UInt32 size = 0;
	if( AudioObjectGetPropertyDataSize( kAudioObjectSystemObject, &devices_property_address, 0, NULL, &size ) != noErr ) {
		abort();
	}

	std::vector< AudioObjectID > devices( size / sizeof( AudioObjectID ) );
	if( AudioObjectGetPropertyData(kAudioObjectSystemObject, &devices_property_address, 0, NULL, &size, devices.data() ) != noErr ) {
		abort();
	}

	for( AudioObjectID device : devices ) {
		UInt32 size = sizeof( CFStringRef );
		CFStringRef name = nullptr;
		constexpr AudioObjectPropertyAddress address_uuid = {
			kAudioDevicePropertyDeviceUID,
			kAudioObjectPropertyScopeGlobal,
			kAudioObjectPropertyElementMaster
		};
		if( AudioObjectGetPropertyData( device, &address_uuid, 0, nullptr, &size, &name) != noErr ) {
			abort();
		}

		char utf8[ 256 ];
		// CFIndex len = CFStringGetLength( name );
		// CFIndex utf8_size = CFStringGetMaximumSizeForEncoding( len, kCFStringEncodingUTF8 ) + 1;

		if( !CFStringGetCString( name, utf8, sizeof( utf8 ), kCFStringEncodingUTF8 ) ) {
			abort();
		}

		printf( "%s\n", utf8 );
		CFRelease( name );
	}

	// /* Expected sorted but did not find anything in the docs. */
	// sort(devices.begin(), devices.end(),
	// 	[](AudioObjectID a, AudioObjectID b) { return a < b; });
    //
	// if (devtype == (CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_OUTPUT)) {
	// 	return devices;
	// }
    //
	// AudioObjectPropertyScope scope = (devtype == CUBEB_DEVICE_TYPE_INPUT)
	// 	? kAudioDevicePropertyScopeInput
	// 	: kAudioDevicePropertyScopeOutput;
    //
	// vector<AudioObjectID> devices_in_scope;
	// for (uint32_t i = 0; i < devices.size(); ++i) {
	// 	/* For device in the given scope channel must be > 0. */
	// 	if (audiounit_get_channel_count(devices[i], scope) > 0) {
	// 		devices_in_scope.push_back(devices[i]);
	// 	}
	// }
    //
	// return devices_in_scope;
}

static void coreaudio_callback(void* user_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
	_saudio.callback( ( Vec2 * ) buffer->mAudioData, buffer->mAudioDataByteSize / sizeof( Vec2 ), _saudio.user_data );
	AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static void _saudio_backend_shutdown() {
	if (_saudio.backend.ca_audio_queue) {
		AudioQueueStop(_saudio.backend.ca_audio_queue, true);
		AudioQueueDispose(_saudio.backend.ca_audio_queue, false);
		_saudio.backend.ca_audio_queue = 0;
	}
}

static bool _saudio_backend_init() {
	/* create an audio queue with fp32 samples */
	AudioStreamBasicDescription fmt = {
		.mSampleRate = double( _saudio.sample_rate ),
		.mFormatID = kAudioFormatLinearPCM,
		.mFormatFlags = kLinearPCMFormatFlagIsFloat | kAudioFormatFlagIsPacked,
		.mFramesPerPacket = 1,
		.mChannelsPerFrame = 2,
		.mBytesPerFrame = sizeof( Vec2 ),
		.mBytesPerPacket = sizeof( Vec2 ),
		.mBitsPerChannel = sizeof( float ) * 8,
	};
	OSStatus res = AudioQueueNewOutput(&fmt, coreaudio_callback, 0, NULL, NULL, 0, &_saudio.backend.ca_audio_queue);
	if (0 != res) {
		_SAUDIO_ERROR(COREAUDIO_NEW_OUTPUT_FAILED);
		return false;
	}

	/* create 2 audio buffers */
	for (int i = 0; i < 2; i++) {
		AudioQueueBufferRef buf = NULL;
		const uint32_t buf_byte_size = (uint32_t)_saudio.buffer_frames * fmt.mBytesPerFrame;
		res = AudioQueueAllocateBuffer(_saudio.backend.ca_audio_queue, buf_byte_size, &buf);
		if (0 != res) {
			_SAUDIO_ERROR(COREAUDIO_ALLOCATE_BUFFER_FAILED);
			_saudio_backend_shutdown();
			return false;
		}
		buf->mAudioDataByteSize = buf_byte_size;
		memset( buf->mAudioData, 0, buf->mAudioDataByteSize );
		AudioQueueEnqueueBuffer(_saudio.backend.ca_audio_queue, buf, 0, NULL);
	}

	if( AudioQueueStart( _saudio.backend.ca_audio_queue, NULL ) != 0 ) {
		_SAUDIO_ERROR(COREAUDIO_START_FAILED);
		_saudio_backend_shutdown();
		return false;
	}
	return true;
}

//  █████  ██      ███████  █████
// ██   ██ ██      ██      ██   ██
// ███████ ██      ███████ ███████
// ██   ██ ██           ██ ██   ██
// ██   ██ ███████ ███████ ██   ██

#elif PLATFORM_LINUX

#include "qcommon/base.h"

static void alsa_thread( void * param ) {
	while( !shutting_down.load( std::memory_order_acquire ) ) {
		_saudio.callback( _saudio.backend.buffer, _saudio.backend.buffer_frames, _saudio.user_data );
		int write_res = snd_pcm_writei( _saudio.backend.device, _saudio.backend.buffer, checked_cast< snd_pcm_uframes_t >( _saudio.backend.buffer_frames ) );
		if( write_res == -EPIPE ) {
			snd_pcm_prepare( _saudio.backend.device );
		}
		else {
			Fatal( "snd_pcm_writei = %d", write_res );
		}
	}
	return 0;
}

static bool _saudio_backend_init() {
	if( snd_pcm_open( &_saudio.backend.device, "default", SND_PCM_STREAM_PLAYBACK, 0 ) < 0 ) {
		return false;
	}

	bool ok = false;
	defer {
		if( !ok ) {
			snd_pcm_close( saudio.backend.device );
		}
	};

	/* configuration works by restricting the 'configuration space' step
	   by step, we require all parameters except the sample rate to
	   match perfectly
	   */
	snd_pcm_hw_params_t * params = NULL;
	snd_pcm_hw_params_alloca( &params );
	snd_pcm_hw_params_any( _saudio.backend.device, params );
	snd_pcm_hw_params_set_access( _saudio.backend.device, params, SND_PCM_ACCESS_RW_INTERLEAVED );
	if( snd_pcm_hw_params_set_format( _saudio.backend.device, params, SND_PCM_FORMAT_FLOAT_LE ) < 0 ) {
		return false;
	}
	if( snd_pcm_hw_params_set_buffer_size( _saudio.backend.device, params, checked_cast< snd_pcm_uframes_t >( _saudio.buffer_frames ) ) < 0 ) {
		return false;
	}
	if( snd_pcm_hw_params_set_channels( _saudio.backend.device, params, 2 ) < 0 ) {
		return false;
	}
	u32 rate = _saudio.sample_rate;
	if( snd_pcm_hw_params_set_rate_near( _saudio.backend.device, params, &rate, NULL ) < 0 ) {
		return false;
	}
	if( snd_pcm_hw_params( _saudio.backend.device, params ) < 0 ) {
		return false;
	}

	_saudio.sample_rate = rate;
	_saudio.backend.buffer_frames = _saudio.buffer_frames;
	_saudio.backend.buffer = AllocMany< Vec2 >( sys_allocator, _saudio.buffer_frames );
	_saudio.backend.thread = NewThread( alsa_thread, NULL );

	ok = true;
	return true;
};

static void _saudio_backend_shutdown() {
	JoinThread( _saudio.backend.thread );
	snd_pcm_drain( _saudio.backend.device );
	snd_pcm_close( _saudio.backend.device );
	Free( sys_allocator, _saudio.backend.buffer );
};

#else
#error "unsupported platform"
#endif

bool saudio_setup(const saudio_desc & desc) {
	_saudio = _saudio_state_t {
		.callback = desc.callback,
		.user_data = desc.user_data,
		.sample_rate = desc.sample_rate,
		.buffer_frames = desc.buffer_frames,
	};
	shutting_down.store( false );
	return _saudio_backend_init();
}

void saudio_shutdown() {
	shutting_down.store( true, std::memory_order_release );
	_saudio_backend_shutdown();
}

#endif /* SOKOL_AUDIO_IMPL */
