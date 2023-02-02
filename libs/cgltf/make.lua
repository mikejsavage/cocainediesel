lib( "cgltf", { "libs/cgltf/cgltf.cpp" } )
gcc_obj_cxxflags( "libs/cgltf/cgltf.cpp", "-Wno-cast-align" )
