lib( "msdfgen", { "libs/msdfgen/core/*.cpp" } )
obj_cxxflags( "libs/msdfgen/.*%.cpp", "-DMSDFGEN_PUBLIC=" )
msvc_obj_cxxflags( "libs/msdfgen/.*%.cpp", "/O2" )
gcc_obj_cxxflags( "libs/msdfgen/.*%.cpp", "-O2" )
