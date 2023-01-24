@set vcvarsall="C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat"
@if not defined INCLUDE (
	if exist %vcvarsall% call %vcvarsall% x86_amd64
)

@set vcvarsall="C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat"
@if not defined INCLUDE (
	if exist %vcvarsall% call %vcvarsall% x86_amd64
)

@set vcvarsall="C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat"
@if not defined INCLUDE (
	if exist %vcvarsall% call %vcvarsall% x86_amd64
)

ggbuild\lua.exe make.lua > build.ninja
ggbuild\ninja.exe
