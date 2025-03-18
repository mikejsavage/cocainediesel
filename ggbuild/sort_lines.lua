local r = assert( io.open( arg[ 1 ] ) )
local lines = { }
for line in r:lines() do
	table.insert( lines, line )
end
assert( r:close() )

table.sort( lines )

local w = assert( io.open( arg[ 1 ], "w" ) )
w:write( table.concat( lines, "\n" ) .. "\n" )
assert( w:close() )
