local lfs = require( "INTERNAL_LFS" )

local configs = { }

configs[ "windows" ] = {
	bin_suffix = ".exe",
	obj_suffix = ".obj",
	lib_suffix = ".lib",

	toolchain = "msvc",

	cxxflags = "/c /Oi /Gm- /nologo",
	ldflags = "user32.lib shell32.lib advapi32.lib dbghelp.lib /NOLOGO",
}

configs[ "windows-debug" ] = {
	cxxflags = "/MTd /Z7",
	ldflags = "/DEBUG",
}
configs[ "windows-release" ] = {
	cxxflags = "/O2 /MT /DNDEBUG",
	bin_prefix = "release/",
}
configs[ "windows-bench" ] = {
	bin_suffix = "-bench.exe",
	cxxflags = configs[ "windows-release" ].cxxflags,
	ldflags = configs[ "windows-release" ].ldflags,
	prebuilt_lib_dir = "windows-release",
}

configs[ "linux" ] = {
	obj_suffix = ".o",
	lib_prefix = "lib",
	lib_suffix = ".a",

	toolchain = "gcc",
	cxx = "g++",

	cxxflags = "-c -fdiagnostics-color",
	ldflags = "-fuse-ld=gold",
}

configs[ "linux-debug" ] = {
	cxxflags = "-O0 -ggdb3 -fno-omit-frame-pointer",
}
configs[ "linux-asan" ] = {
	bin_suffix = "-asan",
	cxxflags = configs[ "linux-debug" ].cxxflags .. " -fsanitize=address",
	ldflags = "-fsanitize=address",
	prebuilt_lib_dir = "linux-debug",
}
configs[ "linux-tsan" ] = {
	bin_suffix = "-tsan",
	cxxflags = configs[ "linux-debug" ].cxxflags .. " -fsanitize=thread",
	ldflags = "-fsanitize=thread",
	prebuilt_lib_dir = "linux-debug",
}
configs[ "linux-release" ] = {
	cxxflags = "-O2 -DNDEBUG",
	ldflags = "-s",
	bin_prefix = "release/",
}
configs[ "linux-bench" ] = {
	bin_suffix = "-bench",
	cxxflags = configs[ "linux-release" ].cxxflags,
	ldflags = configs[ "linux-release" ].ldflags,
	prebuilt_lib_dir = "linux-release",
}

local function identify_host()
	local dll_ext = package.cpath:match( "(%a+)$" )

	if dll_ext == "dll" then
		return "windows"
	end

	local p = assert( io.popen( "uname -s" ) )
	local uname = assert( p:read( "*all" ) ):gsub( "%s*$", "" )
	assert( p:close() )

	if uname == "Linux" then
		return "linux"
	end

	io.stderr:write( "can't identify host OS" )
	os.exit( 1 )
end

OS = identify_host()
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

local bin_prefix = rightmost( "bin_prefix" )
local bin_suffix = rightmost( "bin_suffix" )
local obj_suffix = rightmost( "obj_suffix" )
local lib_prefix = rightmost( "lib_prefix" )
local lib_suffix = rightmost( "lib_suffix" )
local prebuilt_lib_dir = rightmost( "prebuilt_lib_dir" )
prebuilt_lib_dir = prebuilt_lib_dir == "" and OS_config or prebuilt_lib_dir
local cxxflags = concat( "cxxflags" )
local ldflags = concat( "ldflags" )

toolchain = rightmost( "toolchain" )

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

local function join_srcs( names, suffix )
	if not names then
		return ""
	end

	local flat = flatten( names )
	for i = 1, #flat do
		flat[ i ] = dir .. "/" .. flat[ i ] .. suffix
	end
	return table.concat( flat, " " )
end

local function join_libs( names )
	if not names then
		return ""
	end

	local joined = { }
	for _, lib in ipairs( flatten( names ) ) do
		local prebuilt = prebuilt_libs[ lib ]
		if prebuilt then
			for _, archive in ipairs( prebuilt ) do
				table.insert( joined, "libs/" .. lib .. "/" .. prebuilt_lib_dir .. "/" .. lib_prefix .. archive .. lib_suffix )
			end
		else
			table.insert( joined, dir .. "/" .. lib_prefix .. lib .. lib_suffix )
		end
	end
	return table.concat( joined, " " )
end

local function printf( form, ... )
	print( form:format( ... ) )
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

msvc_obj_cxxflags = toolchain_helper( "msvc", obj_cxxflags )
msvc_obj_replace_cxxflags = toolchain_helper( "msvc", obj_replace_cxxflags )

gcc_obj_cxxflags = toolchain_helper( "gcc", obj_cxxflags )
gcc_obj_replace_cxxflags = toolchain_helper( "gcc", obj_replace_cxxflags )

printf( "builddir = build" )
printf( "cxxflags = %s", cxxflags )
printf( "ldflags = %s", ldflags )

if toolchain == "msvc" then

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

elseif toolchain == "gcc" then

printf( "cpp = %s", rightmost( "cxx" ) )
printf( [[
rule cpp
    command = $cpp -MD -MF $out.d $cxxflags $extra_cxxflags -c -o $out $in
    depfile = $out.d
    description = $in
    deps = gcc

rule bin
    command = $cpp -o $out $in $ldflags $extra_ldflags
    description = $out

rule lib
    command = ar rs $out $in
    description = $out
]] )

end

local function rule_for_src( src_name )
	local ext = src_name:match( "([^%.]+)$" )
	return ( { cpp = "cpp" } )[ ext ]
end

local function write_ninja_script()
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

	for src_name, cfg in pairs( objs ) do
		local rule = rule_for_src( src_name )
		printf( "build %s/%s%s: %s %s", dir, src_name, obj_suffix, rule, src_name )
		if cfg.cxxflags then
			printf( "    cxxflags = %s", cfg.cxxflags )
		end
		if cfg.extra_cxxflags then
			printf( "    extra_cxxflags = %s", cfg.extra_cxxflags )
		end
	end

	for lib_name, srcs in pairs( libs ) do
		printf( "build %s/%s%s%s: lib %s", dir, lib_prefix, lib_name, lib_suffix, join_srcs( srcs, obj_suffix ) )
	end

	for bin_name, cfg in pairs( bins ) do
		local srcs = { cfg.srcs }

		if OS == "windows" and cfg.rc then
			srcs = { cfg.srcs, cfg.rc }
			-- printf( "build %s/%s%s: rc %s.rc %s.xml", dir, cfg.rc, obj_suffix, cfg.rc, cfg.rc )
			printf( "build %s/%s%s: rc %s.rc", dir, cfg.rc, obj_suffix, cfg.rc )
			printf( "    in_rc = %s.rc", cfg.rc )
		end

		local full_name = bin_prefix .. bin_name .. bin_suffix
		printf( "build %s: bin %s %s",
			full_name,
			join_srcs( srcs, obj_suffix ),
			join_libs( cfg.libs )
		)

		local ldflags_key = toolchain .. "_ldflags"
		local extra_ldflags_key = toolchain .. "_extra_ldflags"
		if cfg[ ldflags_key ] then
			printf( "    ldflags = %s", cfg[ ldflags_key ] )
		end
		if cfg[ extra_ldflags_key ] then
			printf( "    extra_ldflags = %s", cfg[ extra_ldflags_key ] )
		end

		printf( "default %s", full_name )
	end
end

automatically_print_output_at_exit = setmetatable( { }, { __gc = write_ninja_script } )
