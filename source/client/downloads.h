#pragma once

using DownloadDataCallback = bool ( * )( const void * data, size_t n );
using DownloadDoneCallback = void ( * )( bool success, int http_status );

void InitDownloads();
void ShutdownDownloads();

void StartDownload( const char * url, DownloadDataCallback data_callback, DownloadDoneCallback done_callback, const char ** headers, size_t num_headers );
void PumpDownloads();
void CancelDownloads();
