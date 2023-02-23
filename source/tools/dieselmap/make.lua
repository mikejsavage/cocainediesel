bin( "dieselmap", {
	srcs = {
		"source/tools/dieselmap/*.cpp",
		"source/qcommon/allocators.cpp",
		"source/qcommon/base.cpp",
		"source/qcommon/fs.cpp",
		"source/qcommon/hash.cpp",
		"source/qcommon/utf8.cpp",
		"source/qcommon/platform/*_fs.cpp",
		"source/qcommon/platform/*_sys.cpp",
		"source/qcommon/platform/*_threads.cpp",

		"source/gameshared/q_math.cpp",
		"source/gameshared/q_shared.cpp",
		"source/qcommon/strtonum.cpp",
		"source/qcommon/rng.cpp",
	},

	libs = {
		"ggformat",
		"tracy",
		"zstd",
	},

	msvc_extra_ldflags = "ole32.lib shell32.lib user32.lib advapi32.lib",
	gcc_extra_ldflags = "-lm -lpthread",
} )
