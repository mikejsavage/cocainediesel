#! /bin/sh

lua ../ggbuild/sort_lines.lua client/subtitles.h
lua ../ggbuild/sort_lines.lua cgame/mini_obituaries.h
lua ../ggbuild/sort_lines.lua cgame/obituaries.h
lua ../ggbuild/sort_lines.lua cgame/prefixes.h
