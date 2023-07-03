all: debug
.PHONY: debug asan tsan bench release clean

EXE = exe

ifndef WSL_DISTRO_NAME
	ifeq ($(shell uname -s),Darwin)
		EXE = macos
	else
		EXE = linux
	endif
endif

debug:
	@ggbuild/lua.$(EXE) make.lua > build.ninja
	@ggbuild/ninja.$(EXE) -k 0

asan:
	@ggbuild/lua.$(EXE) make.lua asan > build.ninja
	@ggbuild/ninja.$(EXE) -k 0

tsan:
	@ggbuild/lua.$(EXE) make.lua tsan > build.ninja
	@ggbuild/ninja.$(EXE) -k 0

bench:
	@ggbuild/lua.$(EXE) make.lua bench > build.ninja
	@ggbuild/ninja.$(EXE) -k 0

release:
	@ggbuild/lua.$(EXE) make.lua release > build.ninja
	@ggbuild/ninja.$(EXE) -k 0

clean:
	@ggbuild/lua.$(EXE) make.lua debug > build.ninja
	@ggbuild/ninja.$(EXE) -t clean || true
	@ggbuild/lua.$(EXE) make.lua asan > build.ninja || true
	@ggbuild/ninja.$(EXE) -t clean || true
	@ggbuild/lua.$(EXE) make.lua tsan > build.ninja || true
	@ggbuild/ninja.$(EXE) -t clean || true
	@ggbuild/lua.$(EXE) make.lua bench > build.ninja || true
	@ggbuild/ninja.$(EXE) -t clean || true
	@rm -f source/qcommon/gitversion.h
	@rm -rf build release
	@rm -f -- *.exp *.ilk *.ilp *.lib *.pdb
	@rm -f build.ninja
