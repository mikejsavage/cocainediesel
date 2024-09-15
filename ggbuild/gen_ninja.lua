package.path = ( "./?.lua;./?/make.lua" ):gsub( "/", package.config:sub( 1, 1 ) )

local lfs = require( "INTERNAL_LFS" )

local function copy( t, extra )
	local res = { }
	for k, v in pairs( t ) do
		res[ k ] = v
	end
	if extra then
		for k, v in pairs( extra ) do
			res[ k ] = v
		end
	end
	return res
end

local zig
do
	local f = assert( io.open( "ggbuild/zig_version.txt", "r" ) )
	local zig_version = assert( f:read( "*all" ) ):gsub( "%s+$", "" )
	assert( f:close() )
	zig = "ggbuild/zig-" .. zig_version .. "/zig"
end

local configs = { }

configs[ "windows" ] = {
	bin_suffix = ".exe",
	obj_suffix = ".obj",
	lib_suffix = ".lib",

	toolchain = "msvc",

	cxxflags = "/c /Oi /Gm- /nologo",
}

configs[ "windows-debug" ] = {
	-- /Z7 puts debug info in the .obj
	-- /FC (Full path of source code file in diagnostics)
	-- /JMC "Just My Code", doesn't step into library code etc
	cxxflags = "/MTd /Z7 /FC /JMC",
	-- /FUNCTIONPADMIN /OPT:NOREF /OPT:NOICF are required for Live++
	ldflags = "/NOLOGO /DEBUG:FULL /FUNCTIONPADMIN /OPT:NOREF /OPT:NOICF",
}
configs[ "windows-release" ] = {
	cxxflags = "/O2 /MT /DNDEBUG",
	ldflags = "/NOLOGO",
	output_dir = "release/",
}
configs[ "windows-bench" ] = {
	bin_suffix = "-bench.exe",
	cxxflags = configs[ "windows-release" ].cxxflags .. " /Z7",
	ldflags = configs[ "windows-release" ].ldflags .. " /DEBUG:FULL",
	prebuilt_lib_dir = "windows-release",
}

local gcc_common_cxxflags = "-c -ggdb3 -fdiagnostics-color -fno-omit-frame-pointer"

configs[ "linux" ] = {
	obj_suffix = ".o",
	lib_prefix = "lib",
	lib_suffix = ".a",

	toolchain = "gcc",
	cxx = zig .. " c++",
	ar = zig .. " ar",

	cxxflags = gcc_common_cxxflags .. " --target=x86_64-linux-musl",
	ldflags = "--build-id=sha1",
}

configs[ "linux-debug" ] = { }
configs[ "linux-tsan" ] = {
	bin_suffix = "-tsan",
	cxxflags = "-fsanitize=thread -D_LARGEFILE64_SOURCE",
	ldflags = "-fsanitize-thread",
	prebuilt_lib_dir = "linux-debug",
}
configs[ "linux-release" ] = {
	cxxflags = "-O2 -DNDEBUG",
	output_dir = "release/",
	can_static_link = true,
}
configs[ "linux-bench" ] = {
	bin_suffix = "-bench",
	cxxflags = configs[ "linux-release" ].cxxflags,
	prebuilt_lib_dir = "linux-release",
}

configs[ "macos" ] = copy( configs[ "linux" ], {
	cxx = "clang++",
	ar = "ar",
	-- -march=haswell on x86
	cxxflags = gcc_common_cxxflags .. " -arch arm64 -mmacosx-version-min=10.13",
	ldflags = "-arch arm64",
} )
configs[ "macos-debug" ] = { }
configs[ "macos-asan" ] = {
	bin_suffix = "-asan",
	cxxflags = "-fsanitize=address",
	ldflags = "-fsanitize=address -static-libsan",
	prebuilt_lib_dir = "macos-debug",
}
configs[ "macos-tsan" ] = {
	bin_suffix = "-tsan",
	cxxflags = "-fsanitize=thread",
	ldflags = "-fsanitize=thread -static-libsan",
	prebuilt_lib_dir = "macos-debug",
}
configs[ "macos-release" ] = {
	cxxflags = "-O2 -DNDEBUG",
	ldflags = "-Wl,-dead_strip -Wl,-x",
	output_dir = "release/",
}

OS = os.name:lower()
config = arg[ 1 ] or "debug"

local OS_config = OS .. "-" .. config

if not configs[ OS_config ] then
	io.stderr:write( "bad config: " .. OS_config .. "\n" )
	os.exit( 1 )
end

local function concat( key )
	return ""
		.. ( ( configs[ OS ] and configs[ OS ][ key ] ) or "" )
		.. " "
		.. ( ( configs[ OS_config ] and configs[ OS_config ][ key ] ) or "" )
end

local function rightmost( key )
	return nil
		or ( configs[ OS_config ] and configs[ OS_config ][ key ] )
		or ( configs[ OS ] and configs[ OS ][ key ] )
		or ""
end

local output_dir = rightmost( "output_dir" )
local bin_suffix = rightmost( "bin_suffix" )
local obj_suffix = rightmost( "obj_suffix" )
local lib_prefix = rightmost( "lib_prefix" )
local lib_suffix = rightmost( "lib_suffix" )
local prebuilt_lib_dir = rightmost( "prebuilt_lib_dir" )
prebuilt_lib_dir = prebuilt_lib_dir == "" and OS_config or prebuilt_lib_dir
local cxxflags = concat( "cxxflags" )
local ldflags = rightmost( "ldflags" )

local toolchain = rightmost( "toolchain" )
local can_static_link = rightmost( "can_static_link" ) == true

local dir = "build/" .. OS_config
local output = { }

local objs = { }
local objs_flags = { }
local objs_extra_flags = { }

local bins = { }
local bins_flags = { }
local bins_extra_flags = { }

local libs = { }
local prebuilt_libs = { }

local function flatten_into( res, t )
	for _, x in ipairs( t ) do
		if type( x ) == "table" then
			flatten_into( res, x )
		else
			table.insert( res, x )
		end
	end
end

local function flatten( t )
	local res = { }
	flatten_into( res, t )
	return res
end

local function join_srcs( names )
	if not names then
		return ""
	end

	local flat = flatten( names )
	for i = 1, #flat do
		flat[ i ] = dir .. "/" .. flat[ i ] .. obj_suffix
	end
	return table.concat( flat, " " )
end

local function join_libs( names )
	local joined = { }
	for _, lib in ipairs( flatten( names ) ) do
		local prebuilt_lib = prebuilt_libs[ lib ]

		if prebuilt_lib then
			for _, archive in ipairs( prebuilt_lib ) do
				table.insert( joined, "libs/" .. lib .. "/" .. prebuilt_lib_dir .. "/" .. lib_prefix .. archive .. lib_suffix )
			end
		else
			table.insert( joined, dir .. "/" .. lib_prefix .. lib .. lib_suffix )
		end
	end

	return table.concat( joined, " " )
end

local function printf( form, ... )
	print( form and form:format( ... ) or "" )
end

local function glob_impl( dir, rel, res, prefix, suffix, recursive )
	for filename in lfs.dir( dir .. rel ) do
		if filename ~= "." and filename ~= ".." then
			local fullpath = dir .. rel .. "/" .. filename
			local attr = lfs.attributes( fullpath )

			if attr.mode == "directory" then
				if recursive then
					glob_impl( dir, rel .. "/" .. filename, res, prefix, suffix, true )
				end
			else
				local prefix_start = dir:len() + rel:len() + 2
				if fullpath:find( prefix, prefix_start, true ) == prefix_start and fullpath:sub( -suffix:len() ) == suffix then
					table.insert( res, fullpath )
				end
			end
		end
	end
end

local function glob( srcs )
	local res = { }
	for _, pattern in ipairs( flatten( srcs ) ) do
		if pattern:find( "*", 1, true ) then
			local dir, prefix, suffix = pattern:match( "^(.-)/?([^/*]*)%*+(.*)$" )
			local recursive = pattern:find( "**", 1, true ) ~= nil
			assert( not recursive or prefix == "" )

			glob_impl( dir, "", res, prefix, suffix, recursive )
		else
			table.insert( res, pattern )
		end
	end
	return res
end

local function add_srcs( srcs )
	for _, src in ipairs( srcs ) do
		if not objs[ src ] then
			objs[ src ] = { }
		end
	end
end

function bin( bin_name, cfg )
	assert( type( cfg ) == "table", "cfg should be a table" )
	assert( type( cfg.srcs ) == "table", "cfg.srcs should be a table" )
	assert( not cfg.libs or type( cfg.libs ) == "table", "cfg.libs should be a table or nil" )
	assert( not bins[ bin_name ] )

	bins[ bin_name ] = cfg
	cfg.srcs = glob( cfg.srcs )
	add_srcs( cfg.srcs )
end

function lib( lib_name, srcs )
	assert( type( srcs ) == "table", "srcs should be a table" )
	assert( not libs[ lib_name ] )

	local globbed = glob( srcs )
	libs[ lib_name ] = globbed
	add_srcs( globbed )
end

function prebuilt_lib( lib_name, archives )
	assert( not prebuilt_libs[ lib_name ] )
	prebuilt_libs[ lib_name ] = archives or { lib_name }
end

function global_cxxflags( flags )
	cxxflags = cxxflags .. " " .. flags
end

function obj_cxxflags( pattern, flags )
	table.insert( objs_extra_flags, { pattern = pattern, flags = flags } )
end

function obj_replace_cxxflags( pattern, flags )
	table.insert( objs_flags, { pattern = pattern, flags = flags } )
end

local function toolchain_helper( t, f )
	return function( ... )
		if toolchain == t then
			f( ... )
		end
	end
end

msvc_global_cxxflags = toolchain_helper( "msvc", global_cxxflags )
msvc_obj_cxxflags = toolchain_helper( "msvc", obj_cxxflags )
msvc_obj_replace_cxxflags = toolchain_helper( "msvc", obj_replace_cxxflags )

gcc_global_cxxflags = toolchain_helper( "gcc", global_cxxflags )
gcc_obj_cxxflags = toolchain_helper( "gcc", obj_cxxflags )
gcc_obj_replace_cxxflags = toolchain_helper( "gcc", obj_replace_cxxflags )

local function sort_by_key( t )
	local ret = { }
	for k, v in pairs( t ) do
		table.insert( ret, { key = k, value = v } )
	end
	table.sort( ret, function( a, b ) return a.key < b.key end )

	function iter()
		for _, x in ipairs( ret ) do
			coroutine.yield( x.key, x.value )
		end
	end

	return coroutine.wrap( iter )
end

function write_ninja_script()
	printf( "builddir = build" )
	printf( "cxxflags = %s", cxxflags )
	printf( "ldflags = %s", ldflags )
	printf()

	if OS == "windows" then

printf( [[
rule cpp
    command = cl /showIncludes $cxxflags $extra_cxxflags -Fo$out $in
    description = $in
    deps = msvc

rule bin
    command = link /OUT:$out $in $ldflags $extra_ldflags
    description = $out

rule lib
    command = lib /NOLOGO /OUT:$out $in
    description = $out

rule rc
    command = rc /fo$out /nologo $in_rc
    description = $in
]] )

	else

printf( "cpp = %s", rightmost( "cxx" ) )
printf( "ar = %s", rightmost( "ar" ) )

		if OS == "macos" then

printf( [[
rule bin
    command = g++ -o $out $in $ldflags $extra_ldflags
    description = $out
]] )

		else
			if config ~= "release" then

printf( [[
rule bin
    command = %s build-exe -femit-bin=$out $in -lc -lc++ $ldflags $extra_ldflags
    description = $out

rule bin-static
    command = %s build-exe -femit-bin=$out $in -lc -lc++ $ldflags $extra_ldflags -target x86_64-linux-musl -static
    description = $out
]], zig, zig )

			else

printf( [[
rule bin
    command = %s build-exe -femit-bin=%s/$out.fat $in -lc -lc++ $ldflags $extra_ldflags && %s objcopy --only-keep-debug %s/$out.fat $out.debug && %s objcopy --strip-all --add-gnu-debuglink=$out.debug %s/$out.fat $out && chmod +x $out
    description = $out

rule bin-static
    command = %s build-exe -femit-bin=%s/$out.fat $in -lc -lc++ $ldflags $extra_ldflags -target x86_64-linux-musl -static && %s objcopy --only-keep-debug %s/$out.fat $out.debug && %s objcopy --strip-all --add-gnu-debuglink=$out.debug %s/$out.fat $out && chmod +x $out
    description = $out
]], zig, dir, zig, dir, zig, dir, zig, dir, zig, dir, zig, dir )

			end

printf( [[
rule zig
    command = ggbuild/download_zig.sh
    description = Downloading zig, this can be slow and first time linking is slow, subsequent builds will go fast
    generator = true

build %s: zig ggbuild/download_zig.sh
]], zig )

		end

printf( [[
rule cpp
    command = $cpp -MD -MF $out.d $cxxflags $extra_cxxflags -c -o $out $in
    depfile = $out.d
    description = $in
    deps = gcc

rule lib
    command = $ar cr $out $in
    description = $out
]] )

	end

	for _, flag in ipairs( objs_flags ) do
		for name, cfg in pairs( objs ) do
			if name:match( flag.pattern ) then
				cfg.cxxflags = flag.flags
			end
		end
	end

	for _, flag in ipairs( objs_extra_flags ) do
		for name, cfg in pairs( objs ) do
			if name:match( flag.pattern ) then
				cfg.extra_cxxflags = ( cfg.extra_cxxflags or "" ) .. " " .. flag.flags
			end
		end
	end

	for src_name, cfg in sort_by_key( objs ) do
		printf( "build %s/%s%s: cpp %s%s", dir, src_name, obj_suffix, src_name, OS == "linux" and ( " | " .. zig ) or "" )
		if cfg.cxxflags then
			printf( "    cxxflags = %s", cfg.cxxflags )
		end
		if cfg.extra_cxxflags then
			printf( "    extra_cxxflags = %s", cfg.extra_cxxflags )
		end
	end

	printf()

	for lib_name, srcs in sort_by_key( libs ) do
		printf( "build %s/%s%s%s: lib %s", dir, lib_prefix, lib_name, lib_suffix, join_srcs( srcs ) )
	end

	printf()

	for bin_name, cfg in sort_by_key( bins ) do
		local srcs = { cfg.srcs }

		if OS == "windows" and cfg.rc then
			srcs = { cfg.srcs, cfg.rc }
			printf( "build %s/%s%s: rc %s.rc", dir, cfg.rc, obj_suffix, cfg.rc )
			printf( "    in_rc = %s.rc", cfg.rc )
		end

		local full_name = output_dir .. bin_name .. bin_suffix

		local implicit_outputs = ""
		local implicit_dependencies = ""
		if OS == "linux" then
			if config == "release" then
				implicit_outputs = ( " | %s/%s.fat %s.debug" ):format( dir, full_name, full_name )
			end
			implicit_dependencies = " | " .. zig
		end

		printf( "build %s%s: %s %s %s%s",
			full_name,
			implicit_outputs,
			( can_static_link and not cfg.no_static_link ) and "bin-static" or "bin",
			join_srcs( srcs ),
			join_libs( cfg.libs ),
			implicit_dependencies
		)

		local ldflags_key = OS .. "_ldflags"
		if cfg[ ldflags_key ] then
			printf( "    extra_ldflags = %s", cfg[ ldflags_key ] )
		end

		printf( "default %s", full_name )
	end
end
