#! /usr/bin/env lua

if #arg ~= 2 then
	print( "Usage: " .. arg[ 0 ] .. " replacements.txt file.glb/cdvfx" )
	return 1
end

function io.readfile( path )
	local f = assert( io.open( path, "r" ) )
	local contents = assert( f:read( "*all" ) )
	assert( f:close() )
	return contents
end

function io.writefile( path, contents )
	local f = assert( io.open( path, "w" ) )
	assert( f:write( contents ) )
	assert( f:close() )
end

function string.padright( self, alignment, c )
	local aligned_length = ( ( #self + alignment - 1 ) // alignment ) * alignment
	return self .. c:rep( aligned_length - #self )
end

local function load_replacements( path )
	local contents = io.readfile( path )
	local replacements = { }
	for before, after in contents:gmatch( "([^\n]+) ([^\n]+)" ) do
		replacements[ before ] = after
	end
	return replacements
end

local function get_glb_chunk( contents, cursor )
	local chunk_size, chunk_type, cursor = string.unpack( "<I4c4", contents, cursor )
	assert( chunk_size, chunk_type )
	return chunk_type, contents:sub( cursor, cursor + chunk_size - 1 ), cursor + chunk_size
end

local function replace_in_glb( replacements, path )
	local contents = io.readfile( path )

	local cursor = 1
	local magic, version, length, cursor = string.unpack( "<c4I4I4", contents, cursor )

	assert( magic == "glTF", "magic bytes are not glTF" )
	assert( version == 2, "not glTF 2" )

	local chunk1_type, chunk1_data, cursor = get_glb_chunk( contents, cursor )
	local chunk2_type, chunk2_data, cursor = get_glb_chunk( contents, cursor )

	local json = chunk1_type == "JSON" and chunk1_data or chunk2_data
	local bin = chunk1_type == "JSON" and chunk2_data or chunk1_data

	local before_materials, materials, after_materials = json:match( '^(.-)("materials":%b[])(.-)$' )
	if not before_materials then
		before_materials = json
		materials = ""
		after_materials = ""
	end

	for before, after in pairs( replacements ) do
		materials = materials:gsub( '"name":"' .. before .. '"', '"name":"' .. after .. '"' )
	end

	local new_json = ( before_materials .. materials .. after_materials:gsub( " -$", "" ) ):padright( 4, " " )
	local new_json_chunk = string.pack( "<I4c4", #new_json, "JSON" ) .. new_json
	local bin_chunk = string.pack( "<I4c3B", #bin, "BIN", 0 ) .. bin
	local header = string.pack( "<c4I4I4", magic, version, 12 + #new_json_chunk + #bin_chunk )

	io.writefile( path, header .. ( chunk1_type == "JSON" and new_json_chunk or bin_chunk ) .. ( chunk1_type == "JSON" and bin_chunk or new_json_chunk ) )
end

local replacements = load_replacements( arg[ 1 ] )

if arg[ 2 ]:match( "%.glb$" ) then
	replace_in_glb( replacements, arg[ 2 ] )
else
	print( "don't know this file extension" )
	return 1
end
