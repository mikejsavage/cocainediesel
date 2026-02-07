local head = ReadFile( ".git/HEAD" )
local branch = head:match( "^ref: ([^\n]+)" )
if branch then
	head = ReadFile( ".git/" .. branch )
end
local packed_refs = ReadFile( ".git/packed-refs" )

local version
for commit, ref in packed_refs:gmatch( "(%x+) ([^\n]+)" ) do
	if commit == head then
		version = ref:match( "^refs/tags/(.*)$" )
	end
end

version = version or head:sub( 1, 8 )

local a, b, c, d = version:match( "^v(%d+)%.(%d+)%.(%d+)%.(%d+)$" )
if not a then
	a = 0
	b = 0
	c = 0
	d = 0
end

local gitversion = ""
	.. "#define APP_VERSION \"" .. version .. "\"\n"
	.. "#define APP_VERSION_A " .. a .. "\n"
	.. "#define APP_VERSION_B " .. b .. "\n"
	.. "#define APP_VERSION_C " .. c .. "\n"
	.. "#define APP_VERSION_D " .. d .. "\n"

local r = io.open( "source/qcommon/gitversion.h", "r" )
local current = r and r:read( "*all" )
if r then
	r:close()
end

if current ~= gitversion then
	local w = assert( io.open( "source/qcommon/gitversion.h", "w" ) )
	w:write( gitversion )
	assert( w:close() )
end
