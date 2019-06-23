/*
Copyright (C) 2012 Chasseur de bots

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "game/g_angeliface.h"
#include "game/g_ascript.h"

#include "angelscript/angelscript.h"

#define GAME_SCRIPTS_DIRECTORY              "progs"

#define GAMETYPE_SCRIPTS_MODULE_NAME        "gametype"
#define GAMETYPE_SCRIPTS_DIRECTORY          "gametypes"

#define MAP_SCRIPTS_MODULE_NAME             "map"

asIScriptModule *G_LoadGameScript( const char *moduleName, const char *dir, const char *filename, const char *ext );
bool G_ExecutionErrorReport( int error );
