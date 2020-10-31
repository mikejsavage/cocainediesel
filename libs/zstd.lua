lib( "zstd", { "libs/zstd/zstddeclib.cpp" } )
msvc_obj_cxxflags( "libs/zstd/zstddeclib.cpp", "/O2" )
gcc_obj_cxxflags( "libs/zstd/zstddeclib.cpp", "-O2 -Wno-ignored-qualifiers" )
