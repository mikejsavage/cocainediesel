local graph_width = 85
local filename_length = 16

local log = io.open( "build/.ninja_log", "r" ):read( "*all" )

local edges = { }

for line in log:gmatch( "([^\n]+)" ) do
	local start, finish, target = line:match( "^(%d+)%s+(%d+)%s+%d+%s+(%S+)" )
	if start then
		table.insert( edges, {
			start = tonumber( start ),
			dt = tonumber( finish ) - tonumber( start ),
			target = target:match( "[^/]+$" ),
		} )
	end
end

table.sort( edges, function( a, b )
	return a.start < b.start
end )

local function truncate( str, len )
	str = str:gsub( "%.pic", "" ):gsub( "%.obj", "" ):gsub( "%.o", "" )
	return str:sub( 1, len ) .. string.rep( " ", len - str:len() )
end

local total_duration = edges[ #edges ].start + edges[ #edges ].dt

for i, e in ipairs( edges ) do
	local pre = math.floor( graph_width * e.start / total_duration )
	local width = math.max( 0, math.floor( graph_width * e.dt / total_duration - 2 ) )

	local target = truncate( e.target, filename_length )
	local spacing = string.rep( " ", pre )

	if i % 2 == 0 then
		spacing = spacing:gsub( "   ", " - " )
	end

	print( ( "%s %s[%s] %.2fs" ):format( target, spacing, string.rep( ">", width ), e.dt / 1000 ) )
end

print()
print( ( "Total build time: %.2fs" ):format( total_duration / 1000 ) )
