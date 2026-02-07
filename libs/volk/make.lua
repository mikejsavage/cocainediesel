lib( "volk", { "libs/volk/volk.cpp" } )
obj_cxxflags( "libs/volk/volk.cpp", "-I libs/vulkan-headers" )
