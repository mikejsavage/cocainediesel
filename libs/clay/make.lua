lib( "clay", { "libs/clay/clay.cpp" } )
msvc_obj_cxxflags( "libs/clay/clay.cpp", "/wd4018 /wd4389" ) -- signed/unsigned mismatch
gcc_obj_cxxflags( "libs/clay/clay.cpp", "-Wno-sign-compare -Wno-missing-field-initializers" )
