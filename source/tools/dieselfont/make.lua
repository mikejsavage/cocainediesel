bin( "dieselfont", {
	srcs = {
	 	"source/tools/dieselfont/dieselfont.cpp",
	 	"source/tools/tools.cpp",

		"source/qcommon/allocators.cpp",
		"source/qcommon/base.cpp",
		"source/qcommon/fs.cpp",
		"source/qcommon/hash.cpp",
		"source/qcommon/platform/*_fs.cpp",
		"source/qcommon/platform/*_sys.cpp",
		"source/qcommon/platform/*_threads.cpp",
		"source/qcommon/platform/windows_utf8.cpp",

		"source/gameshared/q_math.cpp",
		"source/gameshared/q_shared.cpp",
		"source/qcommon/rng.cpp",
	},

	libs = {
		"ggformat",
		"msdf",
		"stb_image_write",
		"stb_rect_pack",
		"stb_truetype",
		"tracy",
	},

	windows_ldflags = "ole32.lib shell32.lib user32.lib advapi32.lib",
	linux_ldflags = "-lm",

	-- dont_build_by_default = true,
} )

obj_cxxflags( "source/tools/dieselfont/dieselfont.cpp", "-DMSDFGEN_PUBLIC=" )
