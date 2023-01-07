@set vs2022="C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat"
@if not defined INCLUDE (
	if exist %vs2022% call %vs2022% x86_amd64
)

@set vs2019="C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat"
@if not defined INCLUDE (
	if exist %vs2019% call %vs2019% x86_amd64
)

ggbuild\lua.exe make.lua > build.ninja
ggbuild\ninja.exe
