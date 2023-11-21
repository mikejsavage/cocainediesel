#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"

#include <AudioToolbox/AudioToolbox.h>

static AudioQueueRef audio_queue;

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

bool InitAudioBackend( const char * preferred_device, AudioBackendCallback callback, void * user_data ) {
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

static void ShutdownAudioBackend() {
	AudioQueueStop(_saudio.backend.ca_audio_queue, true);
	AudioQueueDispose(_saudio.backend.ca_audio_queue, false);
	_saudio.backend.ca_audio_queue = 0;
}

#endif // #if PLATFORM_MACOS
