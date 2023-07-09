#pragma once

#include "qcommon/types.h"

struct ZSTD_CCtx_s;
struct SyncEntityState;

struct RecordDemoContext {
	char * filename;
	FILE * file;
	char * temp_filename;
	FILE * temp_file;

	size_t decompressed_size;

	ZSTD_CCtx_s * zstd;

	void * in_buf;
	size_t in_buf_cursor;
	size_t in_buf_capacity;
	void * out_buf;
	size_t out_buf_capacity;
};

struct DemoMetadata {
	u32 metadata_version;
	Span< char > game_version;
	Span< char > server;
	Span< char > map;
	s64 utc_time;
	u64 duration_seconds;
	u64 decompressed_size;
};

enum DemoMetadataVersions : u32 {
	DemoMetadataVersion_Initial = 1,
	DemoMetadataVersion_AddDurationAndDecompressedSize,

	DemoMetadataVersion_Count
};

constexpr u32 DEMO_METADATA_VERSION = DemoMetadataVersion_Count - 1;

bool StartRecordingDemo( TempAllocator * temp, RecordDemoContext * ctx, const char * filename, unsigned int spawncount, unsigned int snapFrameTime,
	int max_clients, const SyncEntityState * baselines );
void WriteDemoMessage( RecordDemoContext * ctx, msg_t msg, size_t skip = 0 );
void StopRecordingDemo( TempAllocator * temp, RecordDemoContext * ctx, const DemoMetadata & metadata );

bool ReadDemoMetadata( Allocator * a, DemoMetadata * metadata, Span< const u8 > contents );
bool DecompressDemo( Allocator * a, const DemoMetadata & metadata, Span< u8 > * decompressed, Span< const u8 > demo );
