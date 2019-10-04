lib( "tracy", { "libs/tracy/TracyClient.cpp" } )
msvc_obj_cxxflags( "libs/tracy/TracyClient.cpp", "/O2" )
gcc_obj_cxxflags( "libs/tracy/TracyClient.cpp", "-O2" )
