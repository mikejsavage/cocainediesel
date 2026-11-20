exe := if os() == "windows" { "exe" } else if env( "WSL_DISTRO_NAME", "" ) != "" { "exe" } \
	else if os() == "macos" { "macos" } else { "linux" }

debug: (_build "")
asan: (_build "asan")
tsan: (_build "tsan")
bench: (_build "bench")
release: (_build "release")

clean: (_clean "") (_clean "asan") (_clean "tsan") (_clean "bench") (_clean "release")
	@rm -f source/qcommon/gitversion.h
	@rm -rf build release
	@rm -f -- *.exp *.ilk *.ilp *.lib *.pdb
	@rm -f build.ninja

_build config:
	@ggbuild/lua.{{exe}} make.lua {{config}} > build.ninja
	@ggbuild/ninja.{{exe}} -k 0

_clean config:
	ggbuild/lua.{{exe}} make.lua {{config}} > build.ninja && ggbuild/ninja.{{exe}} -t clean || true

sort_string_lists:
	lua ggbuild/sort_lines.lua source/client/subtitles.h
	lua ggbuild/sort_lines.lua source/cgame/mini_obituaries.h
	lua ggbuild/sort_lines.lua source/cgame/obituaries.h
	lua ggbuild/sort_lines.lua source/cgame/prefixes.h

[arg("dir", long)]
wc dir="":
	@find source/{{dir}} \( -name "*.lua" -or -name "*.cpp" -or -name "*.h" -or -name "*.hlsl" \) \
		-and -not \( -name blue_noise.cpp -or -name icon.cpp -or -name rgb_noise.cpp \
		-or -name subtitles.h -or -name mini_obituaries.h -or -name obituaries.h -or -name prefixes.h \) \
		| xargs wc -l
