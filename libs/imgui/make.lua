lib( "imgui", { "libs/imgui/*.cpp" } )

obj_cxxflags( "libs/imgui/imgui_freetype.cpp", "-I libs/freetype" )
obj_cxxflags( "libs/imgui/imgui_impl_opengl3.cpp", "-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=\\\"glad/glad.h\\\"" )
obj_cxxflags( "libs/imgui/imgui_impl_glfw.cpp", "-I libs/glfw3 -DGLFW_INCLUDE_NONE" )
