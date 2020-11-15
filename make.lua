require( "ggbuild.gen_ninja" )
require( "ggbuild.git_version" )

obj_cxxflags( ".*", "-I source -I libs" )

msvc_obj_cxxflags( ".*", "/W4 /wd4100 /wd4146 /wd4189 /wd4201 /wd4307 /wd4324 /wd4351 /wd4127 /wd4505 /wd4530 /wd4702 /wd4706 /D_CRT_SECURE_NO_WARNINGS" )
msvc_obj_cxxflags( ".*", "/wd4244 /wd4267" ) -- silence conversion warnings because there are tons of them
msvc_obj_cxxflags( ".*", "/fp:fast /GR- /EHs-c-" )

gcc_obj_cxxflags( ".*", "-std=c++11 -msse3 -ffast-math -fno-exceptions -fno-rtti -fno-strict-aliasing -fno-strict-overflow -fvisibility=hidden" )
gcc_obj_cxxflags( ".*", "-Wall -Wextra -Wcast-align -Wvla -Wformat-security" ) -- -Wconversion
gcc_obj_cxxflags( ".*", "-Wno-unused-parameter -Wno-missing-field-initializers -Wno-implicit-fallthrough -Wno-format-truncation" )
gcc_obj_cxxflags( ".*", "-Werror=vla -Werror=format-security -Werror=unused-value" )

gcc_obj_cxxflags( "source/game/angelwrap/addon/addon_scriptarray.cpp", "-Wno-cast-function-type" )

obj_cxxflags( ".*", "-D_LIBCPP_TYPE_TRAITS" )

if config == "release" then
	obj_cxxflags( ".*", "-DPUBLIC_BUILD" )
else
	obj_cxxflags( ".*", "-DTRACY_ENABLE" )
end

require( "libs.angelscript" )
require( "libs.cgltf" )
require( "libs.curl" )
require( "libs.freetype" )
require( "libs.gg" )
require( "libs.glad" )
require( "libs.glfw3" )
require( "libs.imgui" )
require( "libs.mbedtls" )
require( "libs.meshoptimizer" )
require( "libs.monocypher" )
require( "libs.openal" )
require( "libs.stb" )
require( "libs.tracy" )
require( "libs.whereami" )
require( "libs.zlib" )
require( "libs.zstd" )

do
	local platform_srcs
	local platform_libs

	if OS == "windows" then
		platform_srcs = {
			"source/windows/win_client.cpp",
			"source/windows/win_console.cpp",
			"source/windows/win_fs.cpp",
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

			"angelscript",
			"cgltf",
			"curl",
			"freetype",
			"ggentropy",
			"ggformat",
			"glad",
			"glfw3",
			"meshoptimizer",
			"monocypher",
			"openal",
			"stb_image",
			"stb_image_write",
			"stb_rect_pack",
			"stb_vorbis",
			"tracy",
			"whereami",
			"zlib",
			"zstd",
			platform_libs,
		},

		rc = "source/windows/client",

		gcc_extra_ldflags = "-lm -lpthread -ldl -lX11 -no-pie -static-libstdc++ -Wl,-rpath,. -L. -lNoesis -lNoesisApp",
		msvc_extra_ldflags = "gdi32.lib ole32.lib oleaut32.lib ws2_32.lib crypt32.lib winmm.lib version.lib imm32.lib /SUBSYSTEM:WINDOWS libs/noesis/windows-release/Noesis.lib libs/noesis/windows-release/NoesisApp.lib",
	} )

	obj_cxxflags( "source/client/renderer/text.cpp", "-I libs/freetype" )
	obj_cxxflags( "source/client/cl_noesis.cpp", "-I libs/noesis" )
	obj_cxxflags( "source/client/cl_glfw.cpp", "-I libs/noesis" )
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
			"angelscript",
			"ggentropy",
			"ggformat",
			"monocypher",
			"tracy",
			"whereami",
			"zlib",
			"zstd",
		},

		gcc_extra_ldflags = "-lm -lpthread -ldl -no-pie -static-libstdc++",
		msvc_extra_ldflags = "ws2_32.lib crypt32.lib",
	} )
end

obj_cxxflags( "source/game/angelwrap/.+", "-I third-party/angelscript/sdk/angelscript/include" )
obj_cxxflags( "source/.+_as_.+", "-I third-party/angelscript/sdk/angelscript/include" )
obj_cxxflags( "source/.+_ascript.cpp", "-I third-party/angelscript/sdk/angelscript/include" )
