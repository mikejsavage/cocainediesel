#pragma once

#include "qcommon/types.h"
#include "qcommon/qcommon.h"

struct ZSTD_CCtx_s;

struct RecordDemoContext {
	char * filename;
	FILE * file;
	char * temp_filename;
	FILE * temp_file;

	ZSTD_CCtx_s * zstd;

	void * in_buf;
	size_t in_buf_cursor;
	size_t in_buf_capacity;
	void * out_buf;
	size_t out_buf_capacity;
};

struct DemoHeader {
	u64 magic;
	u64 metadata_size;
};

struct DemoMetadata {
	u32 metadata_version;
	Span< char > game_version;
	Span< char > server;
	Span< char > map;
	s64 utc_time;
	u64 duration_seconds;
};

constexpr u32 DEMO_METADATA_VERSION = 1;
constexpr const char DEMO_METADATA_MAGIC[ sizeof( DemoHeader::magic ) ] = "cddemo";

bool StartRecordingDemo( TempAllocator * temp, RecordDemoContext * ctx, const char * filename, unsigned int spawncount, unsigned int snapFrameTime,
	int max_clients, const char * configstrings, SyncEntityState * baselines );
void WriteDemoMessage( RecordDemoContext * ctx, msg_t msg, size_t skip = 0 );
void StopRecordingDemo( TempAllocator * temp, RecordDemoContext * ctx, const DemoMetadata & metadata );

bool ReadDemoMetadata( Allocator * a, DemoMetadata * metadata, Span< const u8 > contents );
bool DecompressDemo( Allocator * a, Span< u8 > * decompressed, Span< const u8 > contents );
