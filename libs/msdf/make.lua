lib( "msdf", { "libs/msdf/core/*.cpp" } )
obj_cxxflags( "libs/msdf/.*%.cpp", "-DMSDFGEN_PUBLIC=" )
msvc_obj_cxxflags( "libs/msdf.*%.cpp", "/O2" )
gcc_obj_cxxflags( "libs/msdf.*%.cpp", "-O2" )
