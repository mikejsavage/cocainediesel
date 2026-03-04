#! /usr/bin/env lua

local shader_variants_h = ReadFile( "source/client/renderer/shader_variants.h" )

-- strip comments
shader_variants_h = shader_variants_h:gsub( "//[^\r\n]*", "" ):gsub( "/%*.-%*/", "" )

local function ReadableWhitespace( pattern )
	return ( pattern:gsub( " ", "%%s*" ) )
end

local function StripExtension( path )
	return ( path:gsub( "%.[^%.]+$", "" ) )
end

local function ParseFeatures( features )
	local cli = ""
	local filename = ""

	if features then
		for quoted_feature in features:gmatch( "%b\"\"" ) do
			local feature = quoted_feature:sub( 2, -2 )
			cli = cli .. " -D" .. feature
			filename = filename .. "_" .. feature
		end
	end

	return cli, filename
end

local function PlatformSpecificDxcStuff( cmd )
	-- NOTE(mike 20251117): we have to do `dxc -MD -MF && dxc` because of
	-- https://github.com/microsoft/DirectXShaderCompiler/issues/5416
	-- NOTE(mike 20260131): dxc -M -fspv-debug ICEs
	-- NOTE(mike 20260208): dxc && dxc isn't valid PowerShell so we have to do cmd /c on Windows
	local debug = "-fspv-debug=vulkan-with-source"
	cmd = cmd:format( debug )
	return OS == "windows" and ( "cmd /c \"%s\"" ):format( cmd ) or cmd
end

function write_shaders_ninja_script()
	printf( [[


# Shaders
dxcflags = -Ibase/glsl -I. -spirv -fspv-target-env=vulkan1.2 -fvk-use-scalar-layout -fspv-preserve-bindings -Werror=conversion -Wno-sign-conversion
rule dxc_vertex
    command = %s
    deps = gcc
rule dxc_fragment
    command = %s
    deps = gcc
rule dxc_compute
    command = %s
    deps = gcc
]],
		PlatformSpecificDxcStuff( "dxc $dxcflags -MD -MF $depfile -T vs_6_0 $features $in && dxc $dxcflags %s -T vs_6_0 -E VertexMain $features -Fo $out $in" ),
		PlatformSpecificDxcStuff( "dxc $dxcflags -MD -MF $depfile -T ps_6_0 $features $in && dxc $dxcflags %s -T ps_6_0 -E FragmentMain $features -Fo $out $in" ),
		PlatformSpecificDxcStuff( "dxc $dxcflags -MD -MF $depfile -T cs_6_0 $features $in && dxc $dxcflags %s -T cs_6_0 -E ComputeMain $features -Fo $out $in" )
	)

	if OS == "macos" then
		printf( [[
rule spirv-cross
    command = spirv-cross --msl --msl-version 22000 --msl-argument-buffers --msl-decoration-binding --msl-argument-buffer-tier 1 --msl-force-active-argument-buffer-resources --output $out $in
rule metal
    command = xcrun -sdk macosx metal -c $in -o $out -gline-tables-only -frecord-sources -Wno-unused-variable
rule metallib
    command = xcrun -sdk macosx metallib $in -o $out
]]
		)
	end

	local spv_dir = OS == "macos" and "build" or "base"

	print( "# Graphics shaders" )
	local dedupe = { }
	for graphics_shader in shader_variants_h:gmatch( ReadableWhitespace( "GraphicsShaderDescriptor (%b{}) ," ) ) do
		local src = graphics_shader:match( ReadableWhitespace( "%.src = \"([^\"]+)\"" ) ) .. ".hlsl"
		local cli_features, filename_features = ParseFeatures( graphics_shader:match( ReadableWhitespace( "%.features = (%b{})" ) ) )
		local out_filename = StripExtension( src ) .. filename_features
		local depfile = "build/shaders/" .. out_filename

		if not dedupe[ out_filename ] then
			printf( "build %s/shaders/%s.vert.spv: dxc_vertex base/glsl/%s", spv_dir, out_filename, src )
			printf( "    depfile = %s.vert.d", depfile )
			if cli_features ~= "" then
				printf( "    features = %s", cli_features )
			end

			printf( "build %s/shaders/%s.frag.spv: dxc_fragment base/glsl/%s", spv_dir, out_filename, src )
			printf( "    depfile = %s.frag.d", depfile )
			if cli_features ~= "" then
				printf( "    features = %s", cli_features )
			end

			if OS == "macos" then
				printf( "build build/shaders/%s.vert.metal: spirv-cross build/shaders/%s.vert.spv", out_filename, out_filename )
				printf( "build build/shaders/%s.frag.metal: spirv-cross build/shaders/%s.frag.spv", out_filename, out_filename )
				printf( "build build/shaders/%s.vert.air: metal build/shaders/%s.vert.metal", out_filename, out_filename )
				printf( "build build/shaders/%s.frag.air: metal build/shaders/%s.frag.metal", out_filename, out_filename )
				printf( "build base/shaders/%s.metallib: metallib build/shaders/%s.vert.air build/shaders/%s.frag.air", out_filename, out_filename, out_filename )
				printf( "default base/shaders/%s.metallib", out_filename )
			else
				printf( "default base/shaders/%s.vert.spv base/shaders/%s.frag.spv", out_filename, out_filename )
			end

			dedupe[ out_filename ] = true

			print()
		end
	end

	print( "# Compute shaders" )
	for compute_shader in shader_variants_h:gmatch( ReadableWhitespace( "ComputeShaderDescriptor (%b{}) ," ) ) do
		local src = compute_shader:match( ReadableWhitespace( "{.-, \"([^\"]+)\" }" ) ) .. ".hlsl"
		local out_filename = StripExtension( src )

		printf( "build %s/shaders/%s.comp.spv: dxc_compute base/glsl/%s", spv_dir, out_filename, src )
		printf( "    depfile = build/shaders/%s.d", out_filename )

		if OS == "macos" then
			printf( "build build/shaders/%s.comp.metal: spirv-cross build/shaders/%s.comp.spv", out_filename, out_filename )
			printf( "build build/shaders/%s.comp.air: metal build/shaders/%s.comp.metal", out_filename, out_filename )
			printf( "build base/shaders/%s.metallib: metallib build/shaders/%s.comp.air", out_filename, out_filename, out_filename )
			printf( "default base/shaders/%s.metallib", out_filename )
		else
			printf( "default base/shaders/%s.comp.spv", out_filename )
		end

		print()
	end
end
