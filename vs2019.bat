call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat" x86_amd64
ggbuild\lua.exe make.lua > build.ninja
ggbuild\ninja.exe
