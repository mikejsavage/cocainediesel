#pragma once

#include "qcommon/types.h"

using CurlDoneCallback = void ( * )( int http_status, Span< const u8 > data );

void InitDownloads();
void ShutdownDownloads();

void StartDownload( const char * url, CurlDoneCallback done_callback, const char ** headers, size_t num_headers );
void CancelDownload();
void PumpDownloads();
