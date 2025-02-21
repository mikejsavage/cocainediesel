lib( "imgui", { "libs/imgui/**.cpp" } )

obj_cxxflags( "libs/imgui/imgui_freetype.cpp", "-I libs/imgui/misc/freetype -I libs/freetype" )
obj_cxxflags( "libs/imgui/imgui_impl_glfw.cpp", "-I libs/glfw3 -DGLFW_INCLUDE_NONE" )
obj_cxxflags( "libs/imgui/imgui_impl_sdl3.cpp", "-I libs/sdl" )

gcc_obj_cxxflags( "libs/imgui/imgui_freetype.cpp", "-w" )
