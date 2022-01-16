local windows_srcs = {
	"source/windows/win_fs.cpp",
	"source/windows/win_threads.cpp",
}

local linux_srcs = {
	"source/unix/unix_fs.cpp",
	"source/unix/unix_threads.cpp",
}

local platform_srcs = OS == "windows" and windows_srcs or linux_srcs

bin( "dieselmap", {
	srcs = {
		"source/tools/dieselmap/*.cpp",
		"source/qcommon/allocators.cpp",
		"source/qcommon/base.cpp",
		"source/qcommon/fs.cpp",
		"source/qcommon/hash.cpp",
		"source/qcommon/utf8.cpp",

		"source/gameshared/q_math.cpp",
		"source/gameshared/q_shared.cpp",
		"source/qcommon/strtonum.cpp",
		"source/qcommon/rng.cpp",

		platform_srcs,
	},

	libs = {
		"ggformat",
		"tracy",
		"physx",
		"physx_cooking",
	},

	msvc_extra_ldflags = "ole32.lib",
	gcc_extra_ldflags = "-lm -lpthread",
} )

obj_cxxflags( "source/tools/dieselmap/dieselmap.cpp", "-I libs/physx -I libs/physx/physx" )
