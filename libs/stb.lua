lib( "stb_image", { "libs/stb/stb_image.cpp" } )
msvc_obj_cxxflags( "libs/stb/stb_image.cpp", "/O2 /wd4244 /wd4456" )
gcc_obj_cxxflags( "libs/stb/stb_image.cpp", "-O2 -Wno-type-limits" )
obj_cxxflags( "libs/stb/stb_image.cpp", "-DSTBI_NO_BMP -DSTBI_NO_GIF -DSTBI_NO_HDR -DSTBI_NO_LINEAR -DSTBI_NO_PIC -DSTBI_NO_PNM -DSTBI_NO_PSD -DSTBI_NO_TGA" )

lib( "stb_image_write", { "libs/stb/stb_image_write.cpp" } )

lib( "stb_vorbis", { "libs/stb/stb_vorbis.cpp" } )
msvc_obj_cxxflags( "libs/stb/stb_vorbis.cpp", "/O2 /wd4244 /wd4245 /wd4456 /wd4457 /wd4701" )
gcc_obj_cxxflags( "libs/stb/stb_vorbis.cpp", "-O2 -Wno-unused-value -Wno-maybe-uninitialized" )
