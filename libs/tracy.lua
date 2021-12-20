lib( "tracy", { "libs/tracy/TracyClient.cpp", "libs/tracy/common/TracyMutex.cpp" } )
msvc_obj_cxxflags( "libs/tracy/TracyClient.cpp", "/O2" )
msvc_obj_cxxflags( "libs/tracy/.*", "/wd4456" ) -- shadowing
gcc_obj_cxxflags( "libs/tracy/TracyClient.cpp", "-O2 -Wno-unused-function -Wno-maybe-uninitialized" )
