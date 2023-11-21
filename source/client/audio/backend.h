#pragma once

#include "qcommon/types.h"

using AudioBackendCallback = void ( * )( Span< Vec2 > buffer, u32 sample_rate, void * user_data );

constexpr u32 AudioBackendSampleRate = 44100;
constexpr u32 AudioBackendBufferSize = u32( 44100 * 0.02f ); // 20ms

bool InitAudioBackend( const char * preferred_device, AudioBackendCallback callback, void * user_data );
void ShutdownAudioBackend();
