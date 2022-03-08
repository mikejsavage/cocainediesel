local windows_srcs = {
	"libs/discord/connection_win.cpp",
	"libs/discord/discord_register_win.cpp",
}

local linux_srcs = {
	"libs/discord/connection_unix.cpp",
	"libs/discord/discord_register_linux.cpp",
}

lib( "discord", {
	OS == "windows" and windows_srcs or linux_srcs,
	"libs/discord/discord_rpc.cpp",
	"libs/discord/rpc_connection.cpp",
	"libs/discord/serialization.cpp",
} )
