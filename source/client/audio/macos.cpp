#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"
#include "client/audio/backend.h"

#include <AudioToolbox/AudioToolbox.h>

static AudioQueueRef audio_queue;
static AudioBackendCallback audio_callback;
static void * callback_user_data;

#if 0
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
#endif

static void CoreAudioCallback( void * user_data, AudioQueueRef queue, AudioQueueBufferRef buffer ) {
	audio_callback( Span< Vec2 >( ( Vec2 * ) buffer->mAudioData, buffer->mAudioDataByteSize / sizeof( Vec2 ) ), AudioBackendSampleRate, callback_user_data );
	AudioQueueEnqueueBuffer( queue, buffer, 0, NULL );
}

bool InitAudioBackend( const char * preferred_device, AudioBackendCallback callback, void * user_data ) {
	AudioStreamBasicDescription fmt = {
		.mSampleRate = double( AudioBackendSampleRate ),
		.mFormatID = kAudioFormatLinearPCM,
		.mFormatFlags = kLinearPCMFormatFlagIsFloat | kAudioFormatFlagIsPacked,
		.mFramesPerPacket = 1,
		.mChannelsPerFrame = 2,
		.mBytesPerFrame = sizeof( Vec2 ),
		.mBytesPerPacket = sizeof( Vec2 ),
		.mBitsPerChannel = sizeof( float ) * 8,
	};
	if( AudioQueueNewOutput( &fmt, CoreAudioCallback, 0, NULL, NULL, 0, &audio_queue ) != 0 ) {
		return false;
	}

	for( int i = 0; i < 2; i++ ) {
		AudioQueueBufferRef buf = NULL;
		u32 size = AudioBackendBufferSize * fmt.mBytesPerFrame;
		if( AudioQueueAllocateBuffer( audio_queue, size, &buf ) != 0 ) {
			ShutdownAudioBackend();
			return false;
		}
		buf->mAudioDataByteSize = size;
		memset( buf->mAudioData, 0, size );
		AudioQueueEnqueueBuffer( audio_queue, buf, 0, NULL );
	}

	if( AudioQueueStart( audio_queue, NULL ) != 0 ) {
		ShutdownAudioBackend();
		return false;
	}

	audio_callback = callback;
	callback_user_data = user_data;

	return true;
}

void ShutdownAudioBackend() {
	AudioQueueStop( audio_queue, true );
	AudioQueueDispose( audio_queue, false );
}

#endif // #if PLATFORM_MACOS
