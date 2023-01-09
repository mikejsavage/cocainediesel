require( "ggbuild.gen_ninja" )
require( "ggbuild.git_version" )

global_cxxflags( "-I source -I libs" )

msvc_global_cxxflags( "/std:c++17 /W4 /wd4100 /wd4146 /wd4189 /wd4201 /wd4307 /wd4324 /wd4351 /wd4127 /wd4505 /wd4530 /wd4702 /wd4706 /D_CRT_SECURE_NO_WARNINGS" )
msvc_global_cxxflags( "/wd4244 /wd4267" ) -- silence conversion warnings because there are tons of them
msvc_global_cxxflags( "/wd4611" ) -- setjmp warning
msvc_global_cxxflags( "/GR- /EHs-c-" )

gcc_global_cxxflags( "-std=c++17 -msse3 -fno-exceptions -fno-rtti -fno-strict-aliasing -fno-strict-overflow -fvisibility=hidden" )
gcc_global_cxxflags( "-Wall -Wextra -Wcast-align -Wvla -Wformat-security" ) -- -Wconversion
gcc_global_cxxflags( "-Wno-unused-parameter -Wno-missing-field-initializers -Wno-implicit-fallthrough" )
gcc_global_cxxflags( "-Werror=vla -Werror=format-security -Werror=unused-value" )

if config == "release" then
	global_cxxflags( "-DPUBLIC_BUILD" )
else
	global_cxxflags( "-DTRACY_ENABLE" )
end

require( "libs.cgltf" )
require( "libs.curl" )
require( "libs.discord" )
require( "libs.freetype" )
require( "libs.gg" )
require( "libs.glad" )
require( "libs.glfw3" )
require( "libs.imgui" )
require( "libs.jsmn" )
require( "libs.luau" )
require( "libs.mbedtls" )
require( "libs.meshoptimizer" )
require( "libs.monocypher" )
require( "libs.openal" )
require( "libs.picohttpparser" )
require( "libs.rgbcx" )
require( "libs.stb" )
require( "libs.tracy" )
require( "libs.yoga" )
require( "libs.zstd" )

require( "source.tools.bc4" )
require( "source.tools.dieselmap" )

do
	local platform_srcs
	local platform_libs

	if OS == "windows" then
		platform_srcs = {
			"source/windows/win_client.cpp",
			"source/windows/win_console.cpp",
			"source/windows/win_fs.cpp",
			"source/windows/win_livepp.cpp",
			"source/windows/win_net.cpp",
			"source/windows/win_sys.cpp",
			"source/windows/win_threads.cpp",
			"source/windows/win_time.cpp",
		}
		platform_libs = { }
	else
		platform_srcs = {
			"source/unix/unix_client.cpp",
			"source/unix/unix_console.cpp",
			"source/unix/unix_fs.cpp",
			"source/unix/unix_livepp.cpp",
			"source/unix/unix_net.cpp",
			"source/unix/unix_sys.cpp",
			"source/unix/unix_threads.cpp",
			"source/unix/unix_time.cpp",
		}
		platform_libs = { "mbedtls" }
	end

	bin( "client", {
		srcs = {
			"source/cgame/*.cpp",
			"source/client/**.cpp",
			"source/game/**.cpp",
			"source/gameshared/*.cpp",
			"source/qcommon/*.cpp",
			"source/server/sv_*.cpp",
			platform_srcs
		},

		libs = {
			"imgui",

			"cgltf",
			"curl",
			"discord",
			"freetype",
			"ggentropy",
			"ggformat",
			"ggtime",
			"glad",
			"glfw3",
			"jsmn",
			"luau",
			"monocypher",
			"openal",
			"picohttpparser",
			"stb_image",
			"stb_image_write",
			"stb_rect_pack",
			"stb_vorbis",
			"tracy",
			"yoga",
			"zstd",
			platform_libs,
		},

		rc = "source/windows/client",

		gcc_extra_ldflags = "-lm -lpthread -ldl",
		msvc_extra_ldflags = "shell32.lib gdi32.lib ole32.lib oleaut32.lib ws2_32.lib crypt32.lib winmm.lib version.lib imm32.lib advapi32.lib /SUBSYSTEM:WINDOWS",
		no_static_link = true,
	} )

	obj_cxxflags( "source/client/renderer/text.cpp", "-I libs/freetype" )
end

do
	local platform_srcs

	if OS == "windows" then
		platform_srcs = {
			"source/windows/win_console.cpp",
			"source/windows/win_fs.cpp",
			"source/windows/win_net.cpp",
			"source/windows/win_server.cpp",
			"source/windows/win_sys.cpp",
			"source/windows/win_threads.cpp",
			"source/windows/win_time.cpp",
		}
	else
		platform_srcs = {
			"source/unix/unix_console.cpp",
			"source/unix/unix_fs.cpp",
			"source/unix/unix_net.cpp",
			"source/unix/unix_server.cpp",
			"source/unix/unix_sys.cpp",
			"source/unix/unix_threads.cpp",
			"source/unix/unix_time.cpp",
		}
	end

	bin( "server", {
		srcs = {
			"source/game/**.cpp",
			"source/gameshared/*.cpp",
			"source/qcommon/*.cpp",
			"source/server/*.cpp",
			platform_srcs
		},

		libs = {
			"ggentropy",
			"ggformat",
			"ggtime",
			"monocypher",
			"picohttpparser",
			"tracy",
			"zstd",
		},

		gcc_extra_ldflags = "-lm -lpthread",
		msvc_extra_ldflags = "ole32.lib ws2_32.lib crypt32.lib shell32.lib user32.lib advapi32.lib",
	} )
end

write_ninja_script()
