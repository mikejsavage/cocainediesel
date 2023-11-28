#include "qcommon/platform.h"

#if PLATFORM_WINDOWS

#include "qcommon/base.h"
#include "qcommon/threads.h"
#include "client/audio/backend.h"

#include <atomic>
#include "qcommon/platform/windows_mini_windows_h.h"
#include <synchapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

static IMMDeviceEnumerator * device_enumerator;
static IMMDevice * output_device;
static IAudioClient * audio_client;
static IAudioRenderClient * render_client;
static UINT32 actual_buffer_size;
static Thread * thread;
static std::atomic< bool > shutting_down;
static HANDLE buffer_event;
static bool need_couninitialize;

static AudioBackendCallback audio_callback;
static void * callback_user_data;

static void WasapiThread( void * data ) {
	BYTE * dummy;
	render_client->GetBuffer( actual_buffer_size, &dummy );
	render_client->ReleaseBuffer( actual_buffer_size, AUDCLNT_BUFFERFLAGS_SILENT );

	while( !shutting_down.load( std::memory_order_acquire ) ) {
		WaitForSingleObject( buffer_event, INFINITE );
		UINT32 padding = 0;
		if( FAILED( audio_client->GetCurrentPadding( &padding ) ) ) {
			continue;
		}

		if( padding < AudioBackendBufferSize ) {
			u32 num_frames = actual_buffer_size - padding;
			BYTE * wasapi_buffer = NULL;
			if( FAILED( render_client->GetBuffer( num_frames, &wasapi_buffer ) ) ) {
				continue;
			}

			audio_callback( Span< Vec2 >( ( Vec2 * ) wasapi_buffer, num_frames ), AudioBackendSampleRate, callback_user_data );
			render_client->ReleaseBuffer( num_frames, 0 );
		}
	}
}

static void _saudio_wasapi_release() {
	render_client->Release();
	audio_client->Release();
	output_device->Release();
	device_enumerator->Release();
	CloseHandle( buffer_event );
}

static IMMDevice * OpenDeviceOrDefault( const char * preferred_device_name ) {
	IMMDeviceCollection * devices;
	if( FAILED( device_enumerator->EnumAudioEndpoints( eRender, DEVICE_STATE_ACTIVE, &devices ) ) ) {
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
	if( FAILED( device_enumerator->GetDefaultAudioEndpoint(
			    eRender, eConsole,
			    &device ) ) )
	{
		Fatal( "IMMDeviceEnumerator::GetDefaultAudioEndpoint" );
	}

	return device;
}

bool InitAudioBackend( const char * preferred_device, AudioBackendCallback callback, void * user_data ) {
	need_couninitialize = CoInitializeEx( 0, COINIT_MULTITHREADED ) == S_OK;

	buffer_event = CreateEventA( NULL, FALSE, FALSE, NULL );
	if( buffer_event == NULL ) {
		FatalGLE( "CreateEventA" );
	}

	WAVEFORMATEXTENSIBLE fmtex = { };
	fmtex.Format.nChannels = 2;
	fmtex.Format.nSamplesPerSec = AudioBackendSampleRate;
	fmtex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	fmtex.Format.wBitsPerSample = sizeof( float ) * 8;
	fmtex.Format.nBlockAlign = ( fmtex.Format.nChannels * fmtex.Format.wBitsPerSample ) / 8;
	fmtex.Format.nAvgBytesPerSec = fmtex.Format.nSamplesPerSec * fmtex.Format.nBlockAlign;
	fmtex.Format.cbSize = 22; /* WORD + DWORD + GUID */
	fmtex.Samples.wValidBitsPerSample = 32;
	fmtex.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	fmtex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	REFERENCE_TIME dur = REFERENCE_TIME( double( AudioBackendBufferSize ) / ( ( double( AudioBackendSampleRate ) * ( 1.0 / 10000000.0 ) ) ) );

	bool ok = true;
	ok = ok && !FAILED( CoCreateInstance( __uuidof( MMDeviceEnumerator ), NULL, CLSCTX_ALL, __uuidof( IMMDeviceEnumerator ), ( void ** ) &device_enumerator ) );

	output_device = OpenDeviceOrDefault( "hello" );
	ok = ok && !FAILED( output_device->Activate( __uuidof( IAudioClient ), CLSCTX_ALL, 0, ( void ** ) &audio_client ) );
	ok = ok && !FAILED( audio_client->Initialize( AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, dur, 0, ( WAVEFORMATEX * ) &fmtex, NULL ) );
	ok = ok && !FAILED( audio_client->GetBufferSize( &actual_buffer_size ) );
	ok = ok && !FAILED( audio_client->GetService( __uuidof( IAudioRenderClient ), ( void ** ) &render_client ) );
	ok = ok && !FAILED( audio_client->SetEventHandle( buffer_event ) );

	if( !ok ) {
		_saudio_wasapi_release();
		return false;
	}

	audio_client->Start();
	thread = NewThread( WasapiThread, NULL );

	audio_callback = callback;
	callback_user_data = user_data;

	return true;
}

void ShutdownAudioBackend() {
	shutting_down.store( true, std::memory_order_release );
	JoinThread( thread );
	audio_client->Stop();
	_saudio_wasapi_release();
	if( need_couninitialize ) {
		CoUninitialize();
	}
}

#endif // #if PLATFORM_WINDOWS
