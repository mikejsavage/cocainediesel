#include "qcommon/platform.h"

#if PLATFORM_WINDOWS

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/threads.h"
#include "qcommon/platform/windows_utf8.h"
#include "gameshared/q_shared.h"
#include "client/client.h"
#include "client/audio/backend.h"

#include <atomic>
#include "qcommon/platform/windows_mini_windows_h.h"
#include <synchapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

struct WasapiBackend {
	bool need_couninitialize;
	IMMDeviceEnumerator * device_enumerator;
	HANDLE buffer_event;
};

struct WasapiDevice {
	IMMDevice * output_device;
	IAudioClient * audio_client;
	IAudioRenderClient * render_client;
	UINT32 actual_buffer_size;
	Thread * thread;
	AudioBackendCallback callback;
	void * callback_user_data;
};

static WasapiBackend backend;
static WasapiDevice active_device;
static std::atomic< bool > device_shutting_down;

template< typename T >
static void ComRelease( T * obj ) {
	if( obj != NULL ) {
		obj->Release();
	}
}

bool InitAudioBackend() {
	TracyZoneScoped;

	backend = { };

	backend.need_couninitialize = CoInitializeEx( 0, COINIT_MULTITHREADED ) == S_OK;

	backend.buffer_event = CreateEventA( NULL, FALSE, FALSE, NULL );
	if( backend.buffer_event == NULL ) {
		FatalGLE( "CreateEventA" );
	}

	if( FAILED( CoCreateInstance( __uuidof( MMDeviceEnumerator ), NULL, CLSCTX_ALL, __uuidof( IMMDeviceEnumerator ), ( void ** ) &backend.device_enumerator ) ) ) {
		ShutdownAudioBackend();
		return false;
	}

	return true;
}

void ShutdownAudioBackend() {
	TracyZoneScoped;

	ComRelease( backend.device_enumerator );
	CloseHandle( backend.buffer_event );
	if( backend.need_couninitialize ) {
		CoUninitialize();
	}
}

static void WasapiThread( void * data ) {
	TracyCSetThreadName( "WASAPI" );

	BYTE * dummy;
	active_device.render_client->GetBuffer( active_device.actual_buffer_size, &dummy );
	active_device.render_client->ReleaseBuffer( active_device.actual_buffer_size, AUDCLNT_BUFFERFLAGS_SILENT );

	while( !device_shutting_down.load( std::memory_order_acquire ) ) {
		WaitForSingleObject( backend.buffer_event, INFINITE );
		UINT32 padding = 0;
		if( FAILED( active_device.audio_client->GetCurrentPadding( &padding ) ) ) {
			continue;
		}

		if( padding < AUDIO_BACKEND_BUFFER_SIZE ) {
			u32 num_frames = active_device.actual_buffer_size - padding;
			BYTE * wasapi_buffer = NULL;
			if( FAILED( active_device.render_client->GetBuffer( num_frames, &wasapi_buffer ) ) ) {
				continue;
			}

			active_device.callback( Span< Vec2 >( ( Vec2 * ) wasapi_buffer, num_frames ), active_device.callback_user_data );
			active_device.render_client->ReleaseBuffer( num_frames, 0 );
		}
	}
}

struct AudioDevice {
	IMMDevice * device;
	char * name;
};

static Span< AudioDevice > GetAudioDevices( Allocator * a ) {
	TracyZoneScoped;

	IMMDeviceCollection * devices;
	if( FAILED( backend.device_enumerator->EnumAudioEndpoints( eRender, DEVICE_STATE_ACTIVE, &devices ) ) ) {
		Fatal( "IMMDeviceEnumerator::EnumAudioEndpoints" );
	}
	defer { devices->Release(); };

	NonRAIIDynamicArray< AudioDevice > result( a );

	UINT num_devices;
	devices->GetCount( &num_devices );
	for( UINT i = 0; i < num_devices; i++ ) {
		IMMDevice * device;
		devices->Item( i, &device );

		IPropertyStore * properties;
		if( FAILED( device->OpenPropertyStore( STGM_READ, &properties ) ) ) {
			Fatal( "IMMDevice::OpenPropertyStore" );
		}
		defer { properties->Release(); };

		PROPVARIANT device_name;
		if( FAILED( properties->GetValue( PKEY_Device_FriendlyName, &device_name ) ) ) {
			Fatal( "IPropertyStore::GetValue" );
		}
		defer { PropVariantClear( &device_name ); };

		char * utf8 = WideToUTF8( a, device_name.pwszVal );
		result.add( { device, utf8 } );
	}

	return result.span();
}

static bool OpenDeviceOrDefault( IMMDevice ** result, const char * preferred_device_name ) {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	bool found = false;

	Span< AudioDevice > devices = GetAudioDevices( &temp );
	for( AudioDevice device : devices ) {
		if( !found && StrEqual( device.name, preferred_device_name ) ) {
			*result = device.device;
			found = true;
		}
		else {
			device.device->Release();
		}
	}

	if( found ) {
		return true;
	}

	if( FAILED( backend.device_enumerator->GetDefaultAudioEndpoint( eRender, eConsole, result ) ) ) {
		Fatal( "IMMDeviceEnumerator::GetDefaultAudioEndpoint" );
	}

	return true;
}

bool InitAudioDevice( const char * preferred_device, AudioBackendCallback callback, void * user_data ) {
	active_device = { };
	device_shutting_down.store( false, std::memory_order_release );

	WAVEFORMATEXTENSIBLE fmtex = { };
	fmtex.Format.nChannels = 2;
	fmtex.Format.nSamplesPerSec = AUDIO_BACKEND_SAMPLE_RATE;
	fmtex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	fmtex.Format.wBitsPerSample = sizeof( float ) * 8;
	fmtex.Format.nBlockAlign = ( fmtex.Format.nChannels * fmtex.Format.wBitsPerSample ) / 8;
	fmtex.Format.nAvgBytesPerSec = fmtex.Format.nSamplesPerSec * fmtex.Format.nBlockAlign;
	fmtex.Format.cbSize = 22; /* WORD + DWORD + GUID */
	fmtex.Samples.wValidBitsPerSample = 32;
	fmtex.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	fmtex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	REFERENCE_TIME dur = REFERENCE_TIME( double( AUDIO_BACKEND_BUFFER_SIZE ) / ( ( double( AUDIO_BACKEND_SAMPLE_RATE ) * ( 1.0 / 10000000.0 ) ) ) );

	bool ok = true;
	ok = ok && OpenDeviceOrDefault( &active_device.output_device, preferred_device );
	ok = ok && !FAILED( active_device.output_device->Activate( __uuidof( IAudioClient ), CLSCTX_ALL, 0, ( void ** ) &active_device.audio_client ) );
	ok = ok && !FAILED( active_device.audio_client->Initialize( AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, dur, 0, ( WAVEFORMATEX * ) &fmtex, NULL ) );
	ok = ok && !FAILED( active_device.audio_client->GetBufferSize( &active_device.actual_buffer_size ) );
	ok = ok && !FAILED( active_device.audio_client->GetService( __uuidof( IAudioRenderClient ), ( void ** ) &active_device.render_client ) );
	ok = ok && !FAILED( active_device.audio_client->SetEventHandle( backend.buffer_event ) );

	if( !ok ) {
		ShutdownAudioDevice();
		return false;
	}

	active_device.audio_client->Start();
	active_device.callback = callback;
	active_device.callback_user_data = user_data;
	active_device.thread = NewThread( WasapiThread, NULL );

	return true;
}

void ShutdownAudioDevice() {
	device_shutting_down.store( true, std::memory_order_release );
	if( active_device.thread != NULL ) {
		JoinThread( active_device.thread );
	}
	if( active_device.audio_client != NULL ) {
		active_device.audio_client->Stop();
	}
	ComRelease( active_device.render_client );
	ComRelease( active_device.audio_client );
	ComRelease( active_device.output_device );
}

Span< const char * > GetAudioDeviceNames( Allocator * a ) {
	if( backend.device_enumerator == NULL ) {
		return Span< const char * >();
	}

	Span< AudioDevice > devices = GetAudioDevices( a );

	NonRAIIDynamicArray< const char * > names( a );
	for( AudioDevice device : devices ) {
		device.device->Release();
		names.add( device.name );
	}

	return names.span();
}

#endif // #if PLATFORM_WINDOWS
