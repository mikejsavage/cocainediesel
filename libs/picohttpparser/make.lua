lib( "picohttpparser", { "libs/picohttpparser/picohttpparser.cpp" } )
gcc_obj_cxxflags( "libs/picohttpparser/picohttpparser.cpp", "-Wno-cast-align" )
