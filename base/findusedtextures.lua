#! /usr/bin/lua

local lfs = require( "lfs" )

local used_shaders = { }

local function process_map( filename )
	local f = io.open( filename )
	local contents = f:read( "*all" )
	f:close()

	for shader in contents:gmatch( "(textures/%Z+)" ) do
		used_shaders[ shader ] = true
	end
end

process_map( "maps/wbomb1.bsp" )
process_map( "maps/wbomb2.bsp" )
process_map( "maps/wbomb4.bsp" )
process_map( "maps/wbomb6.bsp" )
process_map( "maps/cocaine_b2.bsp" )

local shader_textures = { }

local function stripext( str )
	return ( str:gsub( "%.[^.]*$", "" ) )
end

local function add_textures( name, shader, key )
	for line in shader:gmatch( key .. "%s+([^\n]+)" ) do
		for tex in line:gmatch( "%S+" ) do
			table.insert( shader_textures[ name ], stripext( tex ) )
		end
	end
end

local function process_shader( filename )
	local f = io.open( filename )
	local contents = f:read( "*all" )
	f:close()

	for name, shader in contents:gmatch( "(%S+)%s*(%b{})" ) do
		if not shader_textures[ name ] then
			shader_textures[ name ] = { }
		end

		add_textures( name, shader, "map" )
		add_textures( name, shader, "animmap" )
		add_textures( name, shader, "animMap" )
		add_textures( name, shader, "material" )

		for tex in shader:gmatch( "celshade%s+%S+%s+%S+%s+(%S+)" ) do
			table.insert( shader_textures[ name ], stripext( tex ) )
		end

		for tex in shader:gmatch( "sky[pP]arms%s+(%S+)" ) do
			table.insert( shader_textures[ name ], stripext( tex ) .. "_px" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_nx" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_py" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_ny" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_pz" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_nz" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_rt" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_lf" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_ft" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_bk" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_up" )
			table.insert( shader_textures[ name ], stripext( tex ) .. "_dn" )
		end
	end
end

for f in lfs.dir( "scripts" ) do
	if f ~= "." and f ~= ".." then
		process_shader( "scripts/" .. f )
	end
end

local used_textures = { }

for shader in pairs( used_shaders ) do
	if shader_textures[ shader ] then
		for _, texture in ipairs( shader_textures[ shader ] ) do
			used_textures[ texture ] = true
		end
	else
		-- print( "no textures for ", "[" .. shader .. "]" )
	end
end

for line in io.stdin:lines() do
	if not used_textures[ stripext( line ) ] then
		print( "rm " .. line )
	else
		print( "# " .. line )
	end
end
