lib( "dr_mp3", { "libs/dr_mp3/dr_mp3.cpp" } )
gcc_obj_cxxflags( "libs/dr_mp3/dr_mp3.cpp", "-O3 -Wno-cast-align" )
msvc_obj_cxxflags( "libs/dr_mp3/dr_mp3.cpp", "/O2" )
