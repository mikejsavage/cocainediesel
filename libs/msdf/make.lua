lib( "msdf", { "libs/msdf/core/*.cpp" } )
obj_cxxflags( "libs/msdf/.*%.cpp", "-DMSDFGEN_PUBLIC=" )
