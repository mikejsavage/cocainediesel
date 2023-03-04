bin( "bc4", {
	srcs = {
		"source/tools/bc4/bc4.cpp",
		"source/qcommon/allocators.cpp",
		"source/qcommon/base.cpp",
		"source/qcommon/fs.cpp",
		"source/qcommon/hash.cpp",
		"source/qcommon/platform/*_fs.cpp",
		"source/qcommon/platform/*_sys.cpp",
		"source/qcommon/platform/*_threads.cpp",
		"source/gameshared/q_shared.cpp",
	},

	libs = {
		"ggformat",
		"rgbcx",
		"stb_image",
		"stb_image_resize",
		"tracy",
		"zstd",
	},

	windows_ldflags = "ole32.lib shell32.lib user32.lib advapi32.lib",
	linux_ldflags = "-lm -lpthread -ldl",
} )
