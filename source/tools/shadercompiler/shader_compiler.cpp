#include "qcommon/base.h"
#include "qcommon/threadpool.h"
#include "tools/shadercompiler/api.h"

void ShowErrorMessage( const char * msg, const char * file, int line ) {
	printf( "%s (%s:%d)\n", msg, file, line );
}

int main() {
	CompileShadersSettings settings = {
		.optimize = false,
	};

	InitThreadPool();
	defer { ShutdownThreadPool(); };

	constexpr size_t arena_size = 1024 * 1024; // 1MB
	ArenaAllocator arena( sys_allocator->allocate( arena_size, 16 ), arena_size );
	defer { Free( sys_allocator, arena.get_memory() ); };
	return CompileShaders( &arena, settings ) ? 0 : 1;
}
