local windows_srcs = {
	"source/windows/win_fs.cpp",
	"source/windows/win_sys.cpp",
	"source/windows/win_threads.cpp",
}

local linux_srcs = {
	"source/unix/unix_fs.cpp",
	"source/unix/unix_threads.cpp",
}

local platform_srcs = OS == "windows" and windows_srcs or linux_srcs

bin( "bc4", {
	srcs = {
		"source/tools/bc4/bc4.cpp",
		"source/qcommon/allocators.cpp",
		"source/qcommon/base.cpp",
		"source/qcommon/fs.cpp",
		"source/qcommon/hash.cpp",
		"source/qcommon/strtonum.cpp",
		"source/gameshared/q_shared.cpp",
		platform_srcs,
	},

	libs = {
		"ggformat",
		"rgbcx",
		"stb_image",
		"stb_image_resize",
		"tracy",
	},

	gcc_extra_ldflags = "-lm -lpthread -ldl",
	msvc_extra_ldflags = "ole32.lib shell32.lib user32.lib advapi32.lib",
	static_linux_release_build = true,
} )
