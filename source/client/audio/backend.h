#pragma once

#include "qcommon/types.h"

using AudioBackendCallback = void ( * )( Span< Vec2 > buffer, void * user_data );

constexpr u32 AUDIO_BACKEND_SAMPLE_RATE = 44100;
constexpr u32 AUDIO_BACKEND_BUFFER_SIZE = u32( 44100 * 0.02f ); // 20ms

bool InitAudioBackend();
void ShutdownAudioBackend();

bool InitAudioDevice( const char * preferred_device, AudioBackendCallback callback, void * user_data );
void ShutdownAudioDevice();
