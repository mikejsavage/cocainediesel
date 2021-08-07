local windows_srcs = {
	"source/windows/win_fs.cpp",
	"source/windows/win_threads.cpp",
}

local linux_srcs = {
	"source/unix/unix_fs.cpp",
	"source/unix/unix_threads.cpp",
}

local platform_srcs = OS == "windows" and windows_srcs or linux_srcs

-- if OS == "windows" then
-- 	platform_srcs = {
-- 		"source/windows/win_fs.cpp",
-- 		"source/windows/win_net.cpp",
-- 		"source/windows/win_sys.cpp",
-- 		"source/windows/win_threads.cpp",
-- 		"source/windows/win_time.cpp",
-- 	}
-- else
-- 	platform_srcs = {
-- 		"source/unix/unix_fs.cpp",
-- 		"source/unix/unix_net.cpp",
-- 		"source/unix/unix_sys.cpp",
-- 		"source/unix/unix_threads.cpp",
-- 		"source/unix/unix_time.cpp",
-- 	}
-- end

bin( "map2gltf", {
	srcs = {
		"source/tools/maptogltf/*.cpp",
		"source/qcommon/allocators.cpp",
		"source/qcommon/base.cpp",
		"source/qcommon/fs.cpp",
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
		"whereami",
	},

	msvc_extra_ldflags = "ole32.lib",
} )
