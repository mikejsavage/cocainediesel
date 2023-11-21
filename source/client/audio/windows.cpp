#include "qcommon/platform.h"

#if PLATFORM_WINDOWS

#include "qcommon/base.h"

#include <atomic>
#include "qcommon/platform/windows_mini_windows_h.h"
#include <synchapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

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

bool InitAudioBackend( const char * preferred_device, AudioBackendCallback callback, void * user_data ) {
	REFERENCE_TIME dur;
	/* CoInitializeEx could have been called elsewhere already, in which
	   case the function returns with S_FALSE (thus it does not make much
	   sense to check the result)
	   TODO: only uninit if this returns true
	   */
	HRESULT hr = CoInitializeEx( 0, COINIT_MULTITHREADED );
	_saudio.backend.buffer_end_event = CreateEventA( NULL, FALSE, FALSE, NULL );

	bool ok = false;
	defer {
		if( !ok ) {
			_saudio_wasapi_release();
		}
	};

	if( _saudio.backend.buffer_end_event == NULL ) {
		_SAUDIO_ERROR(WASAPI_CREATE_EVENT_FAILED);
		return false;
	}

	if( FAILED( CoCreateInstance( __uuidof( MMDeviceEnumerator ),
			    NULL, CLSCTX_ALL,
			    __uuidof( IMMDeviceEnumerator ),
			    (void**) &_saudio.backend.device_enumerator ) ) )
	{
		_SAUDIO_ERROR( WASAPI_CREATE_DEVICE_ENUMERATOR_FAILED );
		return false;
	}

	_saudio.backend.device = OpenDeviceOrDefault( "hello" );
	if( FAILED( _saudio.backend.device->Activate(
			    __uuidof( IAudioClient ),
			    CLSCTX_ALL, 0,
			    (void**)&_saudio.backend.audio_client ) ) )
	{
		_SAUDIO_ERROR( WASAPI_DEVICE_ACTIVATE_FAILED );
		return false;
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
	dur = REFERENCE_TIME(double(_saudio.buffer_frames) / ((double(_saudio.sample_rate) * (1.0/10000000.0))));
	if (FAILED(_saudio.backend.audio_client->Initialize(
			    AUDCLNT_SHAREMODE_SHARED,
			    AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
			    dur, 0, (WAVEFORMATEX*)&fmtex, NULL)))
	{
		_SAUDIO_ERROR(WASAPI_AUDIO_CLIENT_INITIALIZE_FAILED);
		return false;
	}
	if (FAILED(_saudio.backend.audio_client->GetBufferSize(&_saudio.backend.dst_buffer_frames))) {
		_SAUDIO_ERROR(WASAPI_AUDIO_CLIENT_GET_BUFFER_SIZE_FAILED);
		return false;
	}
	if (FAILED(_saudio.backend.audio_client->GetService(
			    __uuidof(IAudioRenderClient),
			    (void**)&_saudio.backend.render_client)))
	{
		_SAUDIO_ERROR(WASAPI_AUDIO_CLIENT_GET_SERVICE_FAILED);
		return false;
	}
	if (FAILED(_saudio.backend.audio_client->SetEventHandle(_saudio.backend.buffer_end_event))) {
		_SAUDIO_ERROR(WASAPI_AUDIO_CLIENT_SET_EVENT_HANDLE_FAILED);
		return false;
	}

	_saudio.backend.audio_client->Start();
	_saudio.backend.thread = NewThread( wasapi_thread, NULL );

	ok = true;
	return true;
}

void ShutdownAudioBackend() {
	JoinThread( _saudio.backend.thread );
	_saudio.backend.audio_client->Stop();
	_saudio_wasapi_release();
	CoUninitialize();
}

#endif // #if PLATFORM_WINDOWS
