#pragma once

#include "qcommon/types.h"

using AudioBackendCallback = void ( * )( Span< Vec2 > buffer );

constexpr u32 AUDIO_BACKEND_SAMPLE_RATE = 44100;

bool InitAudioDevice( const char * preferred_device, AudioBackendCallback callback );
void ShutdownAudioDevice();
