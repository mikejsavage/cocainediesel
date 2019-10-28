all: debug
.PHONY: debug asan bench release clean

LUA = ggbuild/lua.linux
NINJA = ggbuild/ninja.linux

WSLENV ?= notwsl
ifndef WSLENV
	LUA = ggbuild/lua.exe
	NINJA = ggbuild/ninja.exe
endif

debug:
	@$(LUA) make.lua > build.ninja
	@$(NINJA)

asan:
	@$(LUA) make.lua asan > build.ninja
	@$(NINJA)

bench:
	@$(LUA) make.lua bench > build.ninja
	@$(NINJA)

release:
	@$(LUA) make.lua release > build.ninja
	@$(NINJA)

clean:
	@$(LUA) make.lua debug > build.ninja
	@$(NINJA) -t clean || true
	@$(LUA) make.lua asan > build.ninja || true
	@$(NINJA) -t clean || true
	@$(LUA) make.lua bench > build.ninja || true
	@$(NINJA) -t clean || true
	@rm -f source/qcommon/gitversion.h
	@rm -rf build release
	@rm -f *.exp *.ilk *.ilp *.lib *.pdb
	@rm -f build.ninja
