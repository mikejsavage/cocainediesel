lib( "tracy", { "libs/tracy/TracyClient.cpp" } )
msvc_obj_cxxflags( "libs/tracy/TracyClient.cpp", "/O2" )
msvc_obj_cxxflags( "libs/tracy/TracyClient.cpp", "/wd4456" ) -- shadowing
gcc_obj_cxxflags( "libs/tracy/TracyClient.cpp", "-O2 -Wno-unused-function -Wno-maybe-uninitialized" )
