bin( "shadercompiler", {
	srcs = {
		"source/tools/shadercompiler/*.cpp",
		"source/qcommon/allocators.cpp",
		"source/qcommon/base.cpp",
		"source/qcommon/fs.cpp",
		"source/qcommon/platform/*_fs.cpp",
		"source/qcommon/platform/*_threads.cpp",
		"source/gameshared/q_shared.cpp",
	},

	libs = {
		"ggformat",
		"tracy",
	},
} )
