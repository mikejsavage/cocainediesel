bin( "shadercompiler", {
	srcs = {
		"source/tools/shadercompiler/*.cpp",
		"source/qcommon/allocators.cpp",
		"source/qcommon/base.cpp",
		"source/qcommon/fs.cpp",
		"source/qcommon/threadpool.cpp",
		"source/qcommon/platform/*_fs.cpp",
		"source/qcommon/platform/*_sys.cpp",
		"source/qcommon/platform/*_threads.cpp",
		"source/gameshared/q_shared.cpp",
	},

	libs = {
		"ggformat",
		"tracy",
	},

	windows_ldflags = "ole32.lib shell32.lib user32.lib advapi32.lib",
	linux_ldflags = "-lm -lpthread -ldl",
} )
