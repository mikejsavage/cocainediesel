#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"
#include "client/audio/backend.h"

#include <AudioToolbox/AudioToolbox.h>

static AudioQueueRef audio_queue;
static AudioBackendCallback audio_callback;
static void * callback_user_data;

bool InitAudioBackend() {
	return true;
}

void ShutdownAudioBackend() {
}

static void CoreAudioCallback( void * user_data, AudioQueueRef queue, AudioQueueBufferRef buffer ) {
	audio_callback( Span< Vec2 >( ( Vec2 * ) buffer->mAudioData, buffer->mAudioDataByteSize / sizeof( Vec2 ) ), callback_user_data );
	AudioQueueEnqueueBuffer( queue, buffer, 0, NULL );
}

bool InitAudioDevice( const char * preferred_device, AudioBackendCallback callback, void * user_data ) {
	AudioStreamBasicDescription fmt = {
		.mSampleRate = double( AUDIO_BACKEND_SAMPLE_RATE ),
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
		u32 size = AUDIO_BACKEND_BUFFER_SIZE * fmt.mBytesPerFrame;
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

void ShutdownAudioDevice() {
	AudioQueueStop( audio_queue, true );
	AudioQueueDispose( audio_queue, false );
}

Span< const char * > GetAudioDeviceNames( Allocator * a ) {
	return Span< const char * >();
}

#endif // #if PLATFORM_MACOS
