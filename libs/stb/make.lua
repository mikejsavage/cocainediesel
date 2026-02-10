lib( "stb_image", { "libs/stb/stb_image.cpp" } )
obj_cxxflags( "libs/stb/stb_image.cpp", "-DSTBI_NO_BMP -DSTBI_NO_GIF -DSTBI_NO_HDR -DSTBI_NO_LINEAR -DSTBI_NO_PIC -DSTBI_NO_PNM -DSTBI_NO_PSD -DSTBI_NO_TGA" )
msvc_obj_cxxflags( "libs/stb/stb_image.cpp", "/O2 /wd4244 /wd4456" )

-- we only use stb_image_resize in bc4 so go hard on the CPU features
lib( "stb_image_resize", { "libs/stb/stb_image_resize2.cpp" } )
msvc_obj_cxxflags( "libs/stb/stb_image_resize2.cpp", "/arch:AVX2 -DSTBIR_USE_FMA /wd4456" )
if OS ~= "macos" then
	gcc_obj_cxxflags( "libs/stb/stb_image_resize2.cpp", "-mavx2 -mf16c -mfma -DSTBIR_USE_FMA" )
end

lib( "stb_image_write", { "libs/stb/stb_image_write.cpp" } )

lib( "stb_rect_pack", { "libs/stb/stb_rect_pack.cpp" } )

lib( "stb_truetype", { "libs/stb/stb_truetype.cpp" } )

lib( "stb_vorbis", { "libs/stb/stb_vorbis.cpp" } )
msvc_obj_cxxflags( "libs/stb/stb_vorbis.cpp", "/O2 /wd4244 /wd4245 /wd4456 /wd4457 /wd4701" )

gcc_obj_cxxflags( "libs/stb/.*%.cpp", "-O2 -w" )
