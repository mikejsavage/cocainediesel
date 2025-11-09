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

function write_shaders_ninja_script()
	printf( [[
slangflags = -Ibase/glsl -I. -unscoped-enum -fvk-use-entrypoint-name -fvk-use-scalar-layout
rule slangc_vertex
    command = slangc $slangflags -depfile $out.d -stage vertex -entry VertexMain $in -o $out $features
    deps = gcc
    depfile = $out.d
rule slangc_fragment
    command = slangc $slangflags -depfile $out.d -stage fragment -entry FragmentMain $in -o $out $features
    deps = gcc
    depfile = $out.d
rule slangc_compute
    command = slangc $slangflags -depfile $out.d -stage compute -entry ComputeMain $in -o $out $features
    deps = gcc
    depfile = $out.d
]] )

	if OS == "macos" then
		printf( [[
rule spirv-cross
    command = spirv-cross --msl --msl-version 20000 --msl-argument-buffers --msl-decoration-binding --msl-argument-buffer-tier 1 --msl-force-active-argument-buffer-resources --output $out $in
rule metal
    command = xcrun -sdk macosx metal -c $in -o $out -gline-tables-only -frecord-sources
rule metallib
    command = xcrun -sdk macosx metallib $in -o $out
]]
		)
	end

	local spv_dir = OS == "macos" and "build" or "base"

	print( "# Graphics shaders" )
	local dedupe = { }
	for graphics_shader in shader_variants_h:gmatch( ReadableWhitespace( "GraphicsShaderDescriptor (%b{}) ," ) ) do
		local src = graphics_shader:match( ReadableWhitespace( "%.src = \"([^\"]+)\"" ) ) .. ".slang"
		local cli_features, filename_features = ParseFeatures( graphics_shader:match( ReadableWhitespace( "%.features = (%b{})" ) ) )
		local out_filename = StripExtension( src ) .. filename_features

		if not dedupe[ out_filename ] then
			printf( "build %s/shaders/%s.vert.spv: slangc_vertex base/glsl/%s", spv_dir, out_filename, src )
			if cli_features ~= "" then
				printf( "    features = %s", cli_features )
			end
			printf( "build %s/shaders/%s.frag.spv: slangc_fragment base/glsl/%s", spv_dir, out_filename, src )
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

	print()

	print( "# Compute shaders" )
	for compute_shader in shader_variants_h:gmatch( ReadableWhitespace( "ComputeShaderDescriptor (%b{}) ," ) ) do
		local src = compute_shader:match( ReadableWhitespace( "{.-, \"([^\"]+)\" }" ) ) .. ".slang"
		local out_filename = StripExtension( src )

		printf( "build %s/shaders/%s.comp.spv: slangc_compute base/glsl/%s", spv_dir, out_filename, src )

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
