bin( "dieselmap", {
	srcs = {
		"source/tools/dieselmap/*.cpp",
	 	"source/tools/tools.cpp",

		"source/qcommon/allocators.cpp",
		"source/qcommon/base.cpp",
		"source/qcommon/fs.cpp",
		"source/qcommon/hash.cpp",
		"source/qcommon/linear_algebra_kernels.cpp",
		"source/qcommon/platform/*_fs.cpp",
		"source/qcommon/platform/*_sys.cpp",
		"source/qcommon/platform/*_threads.cpp",
		"source/qcommon/platform/windows_utf8.cpp",

		"source/gameshared/q_math.cpp",
		"source/gameshared/q_shared.cpp",
		"source/gameshared/editor_materials.cpp",
		"source/qcommon/rng.cpp",
	},

	libs = {
		"ggformat",
		"tracy",
		"meshoptimizer",
		"zstd",
	},

	windows_ldflags = "ole32.lib shell32.lib user32.lib advapi32.lib",
	linux_ldflags = "-lm -lpthread",
} )
