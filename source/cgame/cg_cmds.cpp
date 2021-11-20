/*
Copyright (C) 2002-2003 Victor Luchits

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

#include "cgame/cg_local.h"
#include "client/ui.h"
#include "qcommon/fs.h"

static void CG_SC_Print() {
	CG_LocalPrint( "%s", Cmd_Argv( 1 ) );
}

static void CG_SC_ChatPrint() {
	bool teamonly = Q_stricmp( Cmd_Argv( 0 ), "tch" ) == 0;
	int who = atoi( Cmd_Argv( 1 ) );

	if( who < 0 || who > MAX_CLIENTS ) {
		return;
	}

	if( cg_chat->integer == 0 ) {
		return;
	}

	const char * text = Cmd_Argv( 2 );

	if( who == 0 ) {
		CG_LocalPrint( "Console: %s\n", text );
		return;
	}

	const char * name = PlayerName( who - 1 );
	int team = cg_entities[ who ].current.team;
	RGB8 team_color = team == TEAM_SPECTATOR ? RGB8( 128, 128, 128 ) : CG_TeamColor( team );

	const char * prefix = "";
	if( teamonly ) {
		prefix = team == TEAM_SPECTATOR ? "[SPEC] " : "[TEAM] ";
	}

	ImGuiColorToken color( team_color );
	CG_LocalPrint( "%s%s%s%s: %s\n",
		prefix,
		( const char * ) ImGuiColorToken( team_color ).token, name,
		( const char * ) ImGuiColorToken( rgba8_white ).token, text );
}

static void CG_SC_CenterPrint() {
	CG_CenterPrint( Cmd_Argv( 1 ) );
}

static void CG_SC_Debug() {
	if( cg_showServerDebugPrints->integer != 0 ) {
		Com_Printf( S_COLOR_ORANGE "Debug: %s\n", Cmd_Argv( 1 ) );
	}
}

void CG_ConfigString( int i, const char *s ) {
	size_t len;

	// wsw : jal : warn if configstring overflow
	len = strlen( s );
	if( len >= MAX_CONFIGSTRING_CHARS ) {
		Com_Printf( "%sWARNING:%s Configstring %i overflowed\n", S_COLOR_YELLOW, S_COLOR_WHITE, i );
	}

	if( i < 0 || i >= MAX_CONFIGSTRINGS ) {
		Com_Error( "configstring > MAX_CONFIGSTRINGS" );
		return;
	}

	Q_strncpyz( cgs.configStrings[i], s, sizeof( cgs.configStrings[i] ) );

	// do something appropriate
	if( i == CS_AUTORECORDSTATE ) {
		CG_SC_AutoRecordAction( cgs.configStrings[i] );
	} else if( i >= CS_GAMECOMMANDS && i < CS_GAMECOMMANDS + MAX_GAMECOMMANDS ) {
		if( !cgs.demoPlaying ) {
			Cmd_AddCommand( cgs.configStrings[i], NULL );
		}
	}
}

static const char *CG_SC_AutoRecordName() {
	static char name[MAX_STRING_CHARS];

	char date[ 128 ];
	Sys_FormatTime( date, sizeof( date ), "%Y-%m-%d_%H-%M" );

	snprintf( name, sizeof( name ), "%s_%s_%04i", date, cl.map->name, RandomUniform( &cls.rng, 0, 10000 ) );

	return name;
}

void CG_SC_AutoRecordAction( const char *action ) {
	static bool autorecording = false;
	const char *name;
	bool spectator;

	if( !action[0] ) {
		return;
	}

	// filter out autorecord commands when playing a demo
	if( cgs.demoPlaying ) {
		return;
	}

	// let configstrings and other stuff arrive before taking any action
	if( !cgs.precacheDone ) {
		return;
	}

	if( cg.frame.playerState.pmove.pm_type == PM_SPECTATOR || cg.frame.playerState.pmove.pm_type == PM_CHASECAM ) {
		spectator = true;
	} else {
		spectator = false;
	}

	name = CG_SC_AutoRecordName();

	if( !Q_stricmp( action, "start" ) ) {
		if( cg_autoaction_demo->integer && ( !spectator || cg_autoaction_spectator->integer ) ) {
			Cbuf_ExecuteText( EXEC_NOW, "stop silent" );
			Cbuf_ExecuteText( EXEC_NOW, va( "record autorecord/%s silent", name ) );
			autorecording = true;
		}
	} else if( !Q_stricmp( action, "altstart" ) ) {
		if( cg_autoaction_demo->integer && ( !spectator || cg_autoaction_spectator->integer ) ) {
			Cbuf_ExecuteText( EXEC_NOW, va( "record autorecord/%s silent", name ) );
			autorecording = true;
		}
	} else if( !Q_stricmp( action, "stop" ) ) {
		if( autorecording ) {
			Cbuf_ExecuteText( EXEC_NOW, "stop silent" );
			autorecording = false;
		}

		if( cg_autoaction_screenshot->integer && ( !spectator || cg_autoaction_spectator->integer ) ) {
			Cbuf_ExecuteText( EXEC_NOW, va( "screenshot autorecord/%s silent", name ) );
		}
	} else if( !Q_stricmp( action, "cancel" ) ) {
		if( autorecording ) {
			Cbuf_ExecuteText( EXEC_NOW, "stop cancel silent" );
			autorecording = false;
		}
	} else if( developer->integer ) {
		Com_Printf( "CG_SC_AutoRecordAction: Unknown action: %s\n", action );
	}
}

static bool demo_requested = false;
static void CG_Cmd_DemoGet_f() {
	if( demo_requested ) {
		Com_Printf( "Already requesting a demo\n" );
		return;
	}

	if( Cmd_Argc() != 2 || ( atoi( Cmd_Argv( 1 ) ) <= 0 && Cmd_Argv( 1 )[0] != '.' ) ) {
		Com_Printf( "Usage: demoget <number>\n" );
		Com_Printf( "Downloads a demo from the server\n" );
		Com_Printf( "Use the demolist command to see list of demos on the server\n" );
		return;
	}

	Cbuf_ExecuteText( EXEC_NOW, va( "cmd demoget %s", Cmd_Argv( 1 ) ) );

	demo_requested = true;
}

static void CG_SC_DemoGet() {
	if( cgs.demoPlaying ) {
		// ignore download commands coming from demo files
		return;
	}

	if( !demo_requested ) {
		Com_Printf( "Warning: demoget when not requested, ignored\n" );
		return;
	}

	demo_requested = false;

	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Invalid demo ID\n" );
		return;
	}

	const char * filename = Cmd_Argv( 1 );
	Span< const char > extension = FileExtension( filename );
	if( !COM_ValidateRelativeFilename( filename ) || extension != APP_DEMO_EXTENSION_STR ) {
		Com_Printf( "Warning: demoget: Invalid filename, ignored\n" );
		return;
	}

	CL_DownloadFile( filename, []( const char * filename, Span< const u8 > data ) {
		if( data.ptr == NULL )
			return;

		TempAllocator temp = cls.frame_arena.temp();
		const char * path = temp( "{}/demos/server/{}", HomeDirPath(), FileName( filename ) );

		if( FileExists( &temp, path ) ) {
			Com_Printf( "%s already exists!\n", path );
			return;
		}

		if( !WriteFile( &temp, path, data.ptr, data.num_bytes() ) ) {
			Com_Printf( "Couldn't write to %s\n", path );
		}
	} );
}

static void CG_SC_ChangeLoadout() {
	if( cgs.demoPlaying )
		return;

	int weapons[ WeaponCategory_Count ] = { };

	if( Cmd_Argc() != ARRAY_COUNT( weapons ) + 2 )
		return;

	for( size_t i = 0; i < ARRAY_COUNT( weapons ); i++ ) {
		weapons[ i ] = atoi( Cmd_Argv( i + 1 ) );
	}

	PerkType perk = PerkType( atoi( Cmd_Argv( ARRAY_COUNT( weapons ) + 1 ) ) );

	UI_ShowLoadoutMenu( Span< int >( weapons, ARRAY_COUNT( weapons ) ), perk );
}

static void CG_SC_SaveLoadout() {
	Cvar_Set( "cg_loadout", Cmd_Args() );
}

struct ServerCommand {
	const char * name;
	void ( *func )();
};

static const ServerCommand server_commands[] = {
	{ "pr", CG_SC_Print },
	{ "ch", CG_SC_ChatPrint },
	{ "tch", CG_SC_ChatPrint },
	{ "cp", CG_SC_CenterPrint },
	{ "obry", CG_SC_Obituary },
	{ "debug", CG_SC_Debug },
	{ "demoget", CG_SC_DemoGet },
	{ "changeloadout", CG_SC_ChangeLoadout },
	{ "saveloadout", CG_SC_SaveLoadout },
};

void CG_GameCommand( const char * command ) {
	Cmd_TokenizeString( command );
	const char * name = Cmd_Argv( 0 );

	for( ServerCommand cmd : server_commands ) {
		if( strcmp( name, cmd.name ) == 0 ) {
			cmd.func();
			return;
		}
	}

	Com_Printf( "Unknown game command: %s\n", name );
}

/*
==========================================================================

CGAME COMMANDS

==========================================================================
*/

static void SwitchWeapon( WeaponType weapon ) {
	cl.weaponSwitch = weapon;
}

static void CG_Cmd_UseItem_f() {
	if( !Cmd_Argc() ) {
		Com_Printf( "Usage: 'use <item name>' or 'use <item index>'\n" );
		return;
	}

	const char * name = Cmd_Args();
	for( WeaponType i = 0; i < Weapon_Count; i++ ) {
		const WeaponDef * weapon = GS_GetWeaponDef( i );
		if( ( Q_stricmp( weapon->name, name ) == 0 || Q_stricmp( weapon->short_name, name ) == 0 ) && GS_CanEquip( &cg.predictedPlayerState, i ) ) {
			SwitchWeapon( i );
		}
	}
}

static void ScrollWeapon( int step ) {
	WeaponType current = cg.predictedPlayerState.weapon;
	if( cg.predictedPlayerState.pending_weapon != Weapon_None ) {
		current = cg.predictedPlayerState.pending_weapon;
	}

	if( current == Weapon_None )
		return;

	SyncPlayerState * ps = &cg.predictedPlayerState;

	size_t num_weapons = ARRAY_COUNT( ps->weapons );

	int slot = 0;
	for( int i = 0; i < num_weapons; i++ ) {
		if( ps->weapons[ i ].weapon == current ) {
			slot = i;
			break;
		}
	}

	slot += step;

	if( slot >= num_weapons || slot < 0 )
		return;

	WeaponType weapon = ps->weapons[ slot ].weapon;
	if( weapon != Weapon_None && weapon != Weapon_Knife ) {
		SwitchWeapon( weapon );
	}
}

static void CG_Cmd_NextWeapon_f() {
	if( cgs.demoPlaying || cg.predictedPlayerState.pmove.pm_type == PM_CHASECAM ) {
		CG_ChaseStep( 1 );
		return;
	}

	ScrollWeapon( 1 );
}

static void CG_Cmd_PrevWeapon_f() {
	if( cgs.demoPlaying || cg.predictedPlayerState.pmove.pm_type == PM_CHASECAM ) {
		CG_ChaseStep( -1 );
		return;
	}

	ScrollWeapon( -1 );
}

static void CG_Cmd_LastWeapon_f() {
	SwitchWeapon( cg.predictedPlayerState.last_weapon );
}

static void CG_Cmd_Weapon_f() {
	WeaponType weap = cg.predictedPlayerState.weapons[ atoi( Cmd_Argv( 1 ) ) - 1 ].weapon;

	if( weap != Weapon_None ) {
		SwitchWeapon( weap );
	}
}

static void CG_Viewpos_f() {
	Com_Printf( "\"origin\" \"%i %i %i\"\n", (int)cg.view.origin.x, (int)cg.view.origin.y, (int)cg.view.origin.z );
	Com_Printf( "\"angles\" \"%i %i %i\"\n", (int)cg.view.angles.x, (int)cg.view.angles.y, (int)cg.view.angles.z );
}

// local cgame commands
struct cgcmd_t {
	const char *name;
	void ( *func )();
	bool allowdemo;
};

static const cgcmd_t cgcmds[] = {
	{ "+scores", CG_ScoresOn_f, true },
	{ "-scores", CG_ScoresOff_f, true },
	{ "demoget", CG_Cmd_DemoGet_f, false },
	{ "demolist", NULL, false },
	{ "use", CG_Cmd_UseItem_f, false },
	{ "lastweapon", CG_Cmd_LastWeapon_f, false },
	{ "weapnext", CG_Cmd_NextWeapon_f, false },
	{ "weapprev", CG_Cmd_PrevWeapon_f, false },
	{ "weapon", CG_Cmd_Weapon_f, false },
	{ "viewpos", CG_Viewpos_f, true },

	{ NULL, NULL, false }
};

void CG_RegisterCGameCommands() {
	if( !cgs.demoPlaying ) {
		// add game side commands
		for( int i = 0; i < MAX_GAMECOMMANDS; i++ ) {
			const char * name = cgs.configStrings[CS_GAMECOMMANDS + i];
			if( !name[0] ) {
				continue;
			}

			// check for local command overrides
			const cgcmd_t * cmd;
			for( cmd = cgcmds; cmd->name; cmd++ ) {
				if( !Q_stricmp( cmd->name, name ) ) {
					break;
				}
			}
			if( cmd->name ) {
				continue;
			}

			Cmd_AddCommand( name, NULL );
		}
	}

	// add local commands
	for( const cgcmd_t * cmd = cgcmds; cmd->name; cmd++ ) {
		if( cgs.demoPlaying && !cmd->allowdemo ) {
			continue;
		}
		Cmd_AddCommand( cmd->name, cmd->func );
	}
}

void CG_UnregisterCGameCommands() {
	const cgcmd_t *cmd;

	if( !cgs.demoPlaying ) {
		// remove game commands
		for( int i = 0; i < MAX_GAMECOMMANDS; i++ ) {
			const char * name = cgs.configStrings[CS_GAMECOMMANDS + i];
			if( !name[0] ) {
				continue;
			}

			// check for local command overrides so we don't try
			// to unregister them twice
			for( cmd = cgcmds; cmd->name; cmd++ ) {
				if( !Q_stricmp( cmd->name, name ) ) {
					break;
				}
			}
			if( cmd->name ) {
				continue;
			}

			Cmd_RemoveCommand( name );
		}
	}

	// remove local commands
	for( cmd = cgcmds; cmd->name; cmd++ ) {
		if( cgs.demoPlaying && !cmd->allowdemo ) {
			continue;
		}
		Cmd_RemoveCommand( cmd->name );
	}
}
