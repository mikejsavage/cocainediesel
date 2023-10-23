#pragma once

#include <stddef.h>
#include <stdint.h>

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
typedef enum saudio_log_item {
	_SAUDIO_LOG_ITEMS
} saudio_log_item;
#undef _SAUDIO_LOGITEM_XMACRO

/*
   saudio_logger

   Used in saudio_desc to provide a custom logging and error reporting
   callback to sokol-audio.
   */
typedef struct saudio_logger {
	void (*func)(
		const char* tag,                // always "saudio"
		uint32_t log_level,             // 0=panic, 1=error, 2=warning, 3=info
		uint32_t log_item_id,           // SAUDIO_LOGITEM_*
		const char* message_or_null,    // a message string, may be nullptr in release mode
		uint32_t line_nr,               // line number in sokol_audio.h
		const char* filename_or_null,   // source filename, may be nullptr in release mode
		void* user_data);
	void* user_data;
} saudio_logger;

/*
   saudio_allocator

   Used in saudio_desc to provide custom memory-alloc and -free functions
   to sokol_audio.h. If memory management should be overridden, both the
   alloc_fn and free_fn function must be provided (e.g. it's not valid to
   override one function but not the other).
   */
typedef struct saudio_allocator {
	void* (*alloc_fn)(size_t size, void* user_data);
	void (*free_fn)(void* ptr, void* user_data);
	void* user_data;
} saudio_allocator;

typedef struct saudio_desc {
	const char * device_name;
	int sample_rate;        // requested sample rate
	int num_channels;       // number of channels, default: 1 (mono)
	size_t buffer_frames;      // number of frames in streaming buffer
	void (*callback)(float* buffer, size_t num_frames, int num_channels, void* user_data); //... and with user data
	void* user_data;        // optional user data argument for callback
	saudio_allocator allocator;     // optional allocation override functions
	saudio_logger logger;           // optional logging function (default: NO LOGGING!)
} saudio_desc;

/* setup sokol-audio */
bool saudio_setup(const saudio_desc& desc);
/* shutdown sokol-audio */
void saudio_shutdown(void);

// ██ ███    ███ ██████  ██      ███████ ███    ███ ███████ ███    ██ ████████  █████  ████████ ██  ██████  ███    ██
// ██ ████  ████ ██   ██ ██      ██      ████  ████ ██      ████   ██    ██    ██   ██    ██    ██ ██    ██ ████   ██
// ██ ██ ████ ██ ██████  ██      █████   ██ ████ ██ █████   ██ ██  ██    ██    ███████    ██    ██ ██    ██ ██ ██  ██
// ██ ██  ██  ██ ██      ██      ██      ██  ██  ██ ██      ██  ██ ██    ██    ██   ██    ██    ██ ██    ██ ██  ██ ██
// ██ ██      ██ ██      ███████ ███████ ██      ██ ███████ ██   ████    ██    ██   ██    ██    ██  ██████  ██   ████
//
// >>implementation
#ifdef SOKOL_AUDIO_IMPL

#include <stdlib.h> // alloc, free
#include <string.h> // memset, memcpy
#include <stddef.h> // size_t
#include <atomic>

#ifndef SOKOL_DEBUG
#ifndef NDEBUG
#define SOKOL_DEBUG
#endif
#endif

// platform detection defines
#if defined(__APPLE__)
#define _SAUDIO_APPLE (1)
#elif defined(_WIN32)
#define _SAUDIO_WINDOWS (1)
#elif defined(__linux__) || defined(__unix__)
#define _SAUDIO_LINUX (1)
#else
#error "sokol_audio.h: Unknown platform"
#endif

// ███████ ████████ ██████  ██    ██  ██████ ████████ ███████
// ██         ██    ██   ██ ██    ██ ██         ██    ██
// ███████    ██    ██████  ██    ██ ██         ██    ███████
//      ██    ██    ██   ██ ██    ██ ██         ██         ██
// ███████    ██    ██   ██  ██████   ██████    ██    ███████
//
// >>structs

#if defined(_SAUDIO_APPLE)

#include <AudioToolbox/AudioToolbox.h>

struct GGAudioBackend {
	AudioQueueRef ca_audio_queue;
};

#elif defined(_SAUDIO_LINUX)

#include <alloca.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

struct GGAudioBackend {
	snd_pcm_t* device;
	float* buffer;
	size_t buffer_byte_size;
	size_t buffer_frames;
	pthread_t thread;
};

#elif defined(_SAUDIO_WINDOWS)

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
	IMMDevice* device;
	IAudioClient* audio_client;
	IAudioRenderClient* render_client;
	HANDLE thread_handle;
	HANDLE buffer_end_event;
	UINT32 dst_buffer_frames;
};

#else
#error "unknown platform"
#endif

typedef struct {
	void ( *callback )( float * buffer, size_t num_frames, int num_channels, void * user_data );
	void * user_data;
	int sample_rate;
	size_t buffer_frames;          /* number of frames in streaming buffer */
	int bytes_per_frame;        /* filled by backend */
	int num_channels;           /* actual number of channels */
	GGAudioBackend backend;
	saudio_allocator allocator;
	saudio_logger logger;
} _saudio_state_t;

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

#define _SAUDIO_PANIC(code) _saudio_log(SAUDIO_LOGITEM_ ##code, 0, __LINE__)
#define _SAUDIO_ERROR(code) _saudio_log(SAUDIO_LOGITEM_ ##code, 1, __LINE__)
#define _SAUDIO_WARN(code) _saudio_log(SAUDIO_LOGITEM_ ##code, 2, __LINE__)
#define _SAUDIO_INFO(code) _saudio_log(SAUDIO_LOGITEM_ ##code, 3, __LINE__)

static void _saudio_log(saudio_log_item log_item, uint32_t log_level, uint32_t line_nr) {
	if (_saudio.logger.func) {
#if defined(SOKOL_DEBUG)
		const char* filename = __FILE__;
		const char* message = _saudio_log_messages[log_item];
#else
		const char* filename = 0;
		const char* message = 0;
#endif
		_saudio.logger.func("saudio", log_level, log_item, message, line_nr, filename, _saudio.logger.user_data);
	}
	else {
		// for log level PANIC it would be 'undefined behaviour' to continue
		if (log_level == 0) {
			abort();
		}
	}
}

// ███    ███ ███████ ███    ███  ██████  ██████  ██    ██
// ████  ████ ██      ████  ████ ██    ██ ██   ██  ██  ██
// ██ ████ ██ █████   ██ ████ ██ ██    ██ ██████    ████
// ██  ██  ██ ██      ██  ██  ██ ██    ██ ██   ██    ██
// ██      ██ ███████ ██      ██  ██████  ██   ██    ██
//
// >>memory
static void _saudio_clear(void* ptr, size_t size) {
	memset(ptr, 0, size);
}

static void* _saudio_malloc(size_t size) {
	void* ptr;
	if (_saudio.allocator.alloc_fn) {
		ptr = _saudio.allocator.alloc_fn(size, _saudio.allocator.user_data);
	} else {
		ptr = malloc(size);
	}
	if (0 == ptr) {
		_SAUDIO_PANIC(MALLOC_FAILED);
	}
	return ptr;
}

static void _saudio_free(void* ptr) {
	if (_saudio.allocator.free_fn) {
		_saudio.allocator.free_fn(ptr, _saudio.allocator.user_data);
	} else {
		free(ptr);
	}
}

//  █████  ██      ███████  █████
// ██   ██ ██      ██      ██   ██
// ███████ ██      ███████ ███████
// ██   ██ ██           ██ ██   ██
// ██   ██ ███████ ███████ ██   ██
//
// >>alsa
#if defined(_SAUDIO_LINUX)

/* the streaming callback runs in a separate thread */
static void* _saudio_alsa_cb(void* param) {
	_SOKOL_UNUSED(param);
	while( !shutting_down.load( std::memory_order_acquire ) ) {
		_saudio.callback(_saudio.backend.buffer, _saudio.backend.buffer_frames, _saudio.num_channels, _saudio.user_data);
		int write_res = snd_pcm_writei(_saudio.backend.device, _saudio.backend.buffer, (snd_pcm_uframes_t)_saudio.backend.buffer_frames);
		if (write_res == -EPIPE) {
			/* underrun occurred */
			snd_pcm_prepare(_saudio.backend.device);
		}
		else {
			// TODO: banged
		}
	}
	return 0;
}

static bool _saudio_backend_init(void) {
	int dir; uint32_t rate;
	int rc = snd_pcm_open(&_saudio.backend.device, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		_SAUDIO_ERROR(ALSA_SND_PCM_OPEN_FAILED);
		return false;
	}

	/* configuration works by restricting the 'configuration space' step
	   by step, we require all parameters except the sample rate to
	   match perfectly
	   */
	snd_pcm_hw_params_t* params = 0;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(_saudio.backend.device, params);
	snd_pcm_hw_params_set_access(_saudio.backend.device, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (0 > snd_pcm_hw_params_set_format(_saudio.backend.device, params, SND_PCM_FORMAT_FLOAT_LE)) {
		_SAUDIO_ERROR(ALSA_FLOAT_SAMPLES_NOT_SUPPORTED);
		goto error;
	}
	if (0 > snd_pcm_hw_params_set_buffer_size(_saudio.backend.device, params, (snd_pcm_uframes_t)_saudio.buffer_frames)) {
		_SAUDIO_ERROR(ALSA_REQUESTED_BUFFER_SIZE_NOT_SUPPORTED);
		goto error;
	}
	if (0 > snd_pcm_hw_params_set_channels(_saudio.backend.device, params, (uint32_t)_saudio.num_channels)) {
		_SAUDIO_ERROR(ALSA_REQUESTED_CHANNEL_COUNT_NOT_SUPPORTED);
		goto error;
	}
	/* let ALSA pick a nearby sampling rate */
	rate = (uint32_t) _saudio.sample_rate;
	dir = 0;
	if (0 > snd_pcm_hw_params_set_rate_near(_saudio.backend.device, params, &rate, &dir)) {
		_SAUDIO_ERROR(ALSA_SND_PCM_HW_PARAMS_SET_RATE_NEAR_FAILED);
		goto error;
	}
	if (0 > snd_pcm_hw_params(_saudio.backend.device, params)) {
		_SAUDIO_ERROR(ALSA_SND_PCM_HW_PARAMS_FAILED);
		goto error;
	}

	/* read back actual sample rate and channels */
	_saudio.sample_rate = (int)rate;
	_saudio.bytes_per_frame = _saudio.num_channels * (int)sizeof(float);

	/* allocate the streaming buffer */
	_saudio.backend.buffer_byte_size = _saudio.buffer_frames * _saudio.bytes_per_frame;
	_saudio.backend.buffer_frames = _saudio.buffer_frames;
	_saudio.backend.buffer = (float*) _saudio_malloc((size_t)_saudio.backend.buffer_byte_size);

	/* create the buffer-streaming start thread */
	if (0 != pthread_create(&_saudio.backend.thread, 0, _saudio_alsa_cb, 0)) {
		_SAUDIO_ERROR(ALSA_PTHREAD_CREATE_FAILED);
		goto error;
	}

	return true;
	error:
		if (_saudio.backend.device) {
			snd_pcm_close(_saudio.backend.device);
			_saudio.backend.device = 0;
		}
	return false;
};

static void _saudio_backend_shutdown(void) {
	pthread_join(_saudio.backend.thread, 0);
	snd_pcm_drain(_saudio.backend.device);
	snd_pcm_close(_saudio.backend.device);
	_saudio_free(_saudio.backend.buffer);
};

// ██     ██  █████  ███████  █████  ██████  ██
// ██     ██ ██   ██ ██      ██   ██ ██   ██ ██
// ██  █  ██ ███████ ███████ ███████ ██████  ██
// ██ ███ ██ ██   ██      ██ ██   ██ ██      ██
//  ███ ███  ██   ██ ███████ ██   ██ ██      ██
//
// >>wasapi
#elif defined(_SAUDIO_WINDOWS)

#include <stdio.h>

static DWORD WINAPI _saudio_wasapi_thread_fn( LPVOID param ) {
	BYTE * dummy;
	_saudio.backend.render_client->GetBuffer( _saudio.backend.dst_buffer_frames, &dummy );
	_saudio.backend.render_client->ReleaseBuffer( _saudio.backend.dst_buffer_frames, AUDCLNT_BUFFERFLAGS_SILENT );

	while (!shutting_down.load( std::memory_order_acquire )) {
		WaitForSingleObject(_saudio.backend.buffer_end_event, INFINITE);
		UINT32 padding = 0;
		if (FAILED(_saudio.backend.audio_client->GetCurrentPadding(&padding))) {
			continue;
		}
		int num_frames = (int)_saudio.backend.dst_buffer_frames - (int)padding;

		if (num_frames > 0) {
			BYTE* wasapi_buffer = 0;
			if (FAILED(_saudio.backend.render_client->GetBuffer(num_frames, &wasapi_buffer))) {
				continue;
			}

			float* samples = (float*)wasapi_buffer;
			_saudio.callback(samples, num_frames, _saudio.num_channels, _saudio.user_data);

			_saudio.backend.render_client->ReleaseBuffer(num_frames, 0);
		}
	}

	return 0;
}

static void _saudio_wasapi_release(void) {
	_saudio.backend.render_client->Release();
	_saudio.backend.audio_client->Release();
	_saudio.backend.device->Release();
	_saudio.backend.device_enumerator->Release();
	CloseHandle(_saudio.backend.buffer_end_event);
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
	if (FAILED(_saudio.backend.device_enumerator->GetDefaultAudioEndpoint(
			    eRender, eConsole,
			    &device)))
	{
		_SAUDIO_ERROR(WASAPI_GET_DEFAULT_AUDIO_ENDPOINT_FAILED);
		abort();
	}

	return device;
}

static bool _saudio_backend_init(void) {
	REFERENCE_TIME dur;
	/* CoInitializeEx could have been called elsewhere already, in which
	   case the function returns with S_FALSE (thus it does not make much
	   sense to check the result)
	   TODO: only uninit if this returns true
	   */
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	_saudio.backend.buffer_end_event = CreateEvent(0, FALSE, FALSE, 0);
	if (0 == _saudio.backend.buffer_end_event) {
		_SAUDIO_ERROR(WASAPI_CREATE_EVENT_FAILED);
		goto error;
	}

	if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),
			    NULL, CLSCTX_ALL,
			    IID_IMMDeviceEnumerator,
			    (void**)&_saudio.backend.device_enumerator)))
	{
		_SAUDIO_ERROR(WASAPI_CREATE_DEVICE_ENUMERATOR_FAILED);
		goto error;
	}

	_saudio.backend.device = OpenDeviceOrDefault( "hello" );
	if (FAILED(_saudio.backend.device->Activate(
			    __uuidof(IAudioClient),
			    CLSCTX_ALL, 0,
			    (void**)&_saudio.backend.audio_client)))
	{
		_SAUDIO_ERROR(WASAPI_DEVICE_ACTIVATE_FAILED);
		goto error;
	}

	WAVEFORMATEXTENSIBLE fmtex;
	fmtex = { };
	fmtex.Format.nChannels = (WORD)_saudio.num_channels;
	fmtex.Format.nSamplesPerSec = (DWORD)_saudio.sample_rate;
	fmtex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	fmtex.Format.wBitsPerSample = sizeof( float ) * 8;
	fmtex.Format.nBlockAlign = (fmtex.Format.nChannels * fmtex.Format.wBitsPerSample) / 8;
	fmtex.Format.nAvgBytesPerSec = fmtex.Format.nSamplesPerSec * fmtex.Format.nBlockAlign;
	fmtex.Format.cbSize = 22;   /* WORD + DWORD + GUID */
	fmtex.Samples.wValidBitsPerSample = 32;
	if (_saudio.num_channels == 1) {
		fmtex.dwChannelMask = SPEAKER_FRONT_CENTER;
	}
	else {
		fmtex.dwChannelMask = SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;
	}
	fmtex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	dur = (REFERENCE_TIME)
		(((double)_saudio.buffer_frames) / (((double)_saudio.sample_rate) * (1.0/10000000.0)));
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
	_saudio.bytes_per_frame = _saudio.num_channels * (int)sizeof(float);

	_saudio.backend.audio_client->Start();

	/* create streaming thread */
	_saudio.backend.thread_handle = CreateThread(NULL, 0, _saudio_wasapi_thread_fn, 0, 0, 0);
	if (0 == _saudio.backend.thread_handle) {
		_SAUDIO_ERROR(WASAPI_CREATE_THREAD_FAILED);
		goto error;
	}
	return true;
	error:
		_saudio_wasapi_release();
	return false;
}

static void _saudio_backend_shutdown(void) {
	WaitForSingleObject(_saudio.backend.thread_handle, INFINITE);
	CloseHandle(_saudio.backend.thread_handle);
	_saudio.backend.audio_client->Stop();
	_saudio_wasapi_release();
	CoUninitialize();
}

//  ██████  ██████  ██████  ███████  █████  ██    ██ ██████  ██  ██████
// ██      ██    ██ ██   ██ ██      ██   ██ ██    ██ ██   ██ ██ ██    ██
// ██      ██    ██ ██████  █████   ███████ ██    ██ ██   ██ ██ ██    ██
// ██      ██    ██ ██   ██ ██      ██   ██ ██    ██ ██   ██ ██ ██    ██
//  ██████  ██████  ██   ██ ███████ ██   ██  ██████  ██████  ██  ██████
//
// >>coreaudio
#elif defined(_SAUDIO_APPLE)

#include <vector>

static constexpr AudioObjectPropertyAddress DEVICES_PROPERTY_ADDRESS = {
    kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster};

static void PrintDevices() {
	UInt32 size = 0;
	if( AudioObjectGetPropertyDataSize( kAudioObjectSystemObject, &DEVICES_PROPERTY_ADDRESS, 0, NULL, &size ) != noErr ) {
		abort();
	}

	std::vector< AudioObjectID > devices( size / sizeof( AudioObjectID ) );
	if( AudioObjectGetPropertyData(kAudioObjectSystemObject, &DEVICES_PROPERTY_ADDRESS, 0, NULL, &size, devices.data() ) != noErr ) {
		abort();
	}

	for( AudioObjectID device : devices ) {
		UInt32 size = sizeof( CFStringRef );
		CFStringRef name = nullptr;
		AudioObjectPropertyAddress address_uuid = {kAudioDevicePropertyDeviceUID,
			kAudioObjectPropertyScopeGlobal,
			kAudioObjectPropertyElementMaster};
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

/* NOTE: the buffer data callback is called on a separate thread! */
static void _saudio_coreaudio_callback(void* user_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
	const int num_frames = (int)buffer->mAudioDataByteSize / _saudio.bytes_per_frame;
	const int num_channels = _saudio.num_channels;
	_saudio.callback((float*)buffer->mAudioData, num_frames, num_channels, _saudio.user_data);
	AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static void _saudio_backend_shutdown(void) {
	if (_saudio.backend.ca_audio_queue) {
		AudioQueueStop(_saudio.backend.ca_audio_queue, true);
		AudioQueueDispose(_saudio.backend.ca_audio_queue, false);
		_saudio.backend.ca_audio_queue = 0;
	}
}

static bool _saudio_backend_init(void) {
	/* create an audio queue with fp32 samples */
	AudioStreamBasicDescription fmt;
	_saudio_clear(&fmt, sizeof(fmt));
	fmt.mSampleRate = (double) _saudio.sample_rate;
	fmt.mFormatID = kAudioFormatLinearPCM;
	fmt.mFormatFlags = kLinearPCMFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	fmt.mFramesPerPacket = 1;
	fmt.mChannelsPerFrame = (uint32_t) _saudio.num_channels;
	fmt.mBytesPerFrame = (uint32_t)sizeof(float) * (uint32_t)_saudio.num_channels;
	fmt.mBytesPerPacket = fmt.mBytesPerFrame;
	fmt.mBitsPerChannel = 32;
	OSStatus res = AudioQueueNewOutput(&fmt, _saudio_coreaudio_callback, 0, NULL, NULL, 0, &_saudio.backend.ca_audio_queue);
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
		_saudio_clear(buf->mAudioData, buf->mAudioDataByteSize);
		AudioQueueEnqueueBuffer(_saudio.backend.ca_audio_queue, buf, 0, NULL);
	}

	/* init or modify actual playback parameters */
	_saudio.bytes_per_frame = (int)fmt.mBytesPerFrame;

	/* ...and start playback */
	res = AudioQueueStart(_saudio.backend.ca_audio_queue, NULL);
	if (0 != res) {
		_SAUDIO_ERROR(COREAUDIO_START_FAILED);
		_saudio_backend_shutdown();
		return false;
	}
	return true;
}

#else
#error "unsupported platform"
#endif

bool saudio_setup(const saudio_desc & desc) {
	_saudio = _saudio_state_t {
		.callback = desc.callback,
		.user_data = desc.user_data,
		.sample_rate = desc.sample_rate,
		.buffer_frames = desc.buffer_frames,
		.num_channels = desc.num_channels,
	};
	shutting_down.store( false );
	return _saudio_backend_init();
}

void saudio_shutdown(void) {
	shutting_down.store( true, std::memory_order_release );
	_saudio_backend_shutdown();
}

#endif /* SOKOL_AUDIO_IMPL */
