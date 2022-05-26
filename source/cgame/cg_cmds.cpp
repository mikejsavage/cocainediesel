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
	bool teamonly = StrCaseEqual( Cmd_Argv( 0 ), "tch" );
	int who = atoi( Cmd_Argv( 1 ) );

	if( who < 0 || who > MAX_CLIENTS ) {
		return;
	}

	if( Cvar_Integer( "cg_chat" ) == 0 ) {
		return;
	}

	if( Cvar_Integer( "cg_chat" ) == 2 && !teamonly ) {
		return;
	}

	const char * text = Cmd_Argv( 2 );

	if( who == 0 ) {
		CG_LocalPrint( "Console: %s\n", text );
		return;
	}

	const char * name = PlayerName( who - 1 );
	Team team = cg_entities[ who ].current.team;
	RGB8 team_color = team == Team_None ? RGB8( 128, 128, 128 ) : CG_TeamColor( team );

	const char * prefix = "";
	if( teamonly ) {
		prefix = team == Team_None ? "[SPEC] " : "[TEAM] ";
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

static bool demo_requested = false;
static void CG_Cmd_DemoGet_f() {
	if( demo_requested ) {
		Com_Printf( "Already requesting a demo\n" );
		return;
	}

	msg_t * args = CL_AddReliableCommand( ClientCommand_DemoGetURL );
	MSG_WriteString( args, Cmd_Argv( 1 ) );

	demo_requested = true;
}

static void CG_SC_DownloadDemo() {
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

	Loadout loadout = { };

	if( Cmd_Argc() != ARRAY_COUNT( loadout.weapons ) + 3 )
		return;

	for( size_t i = 0; i < ARRAY_COUNT( loadout.weapons ); i++ ) {
		int weapon = atoi( Cmd_Argv( i + 1 ) );
		if( weapon <= Weapon_None || weapon >= Weapon_Count )
			return;
		loadout.weapons[ i ] = WeaponType( weapon );
	}

	int perk = atoi( Cmd_Argv( ARRAY_COUNT( loadout.weapons ) + 1 ) );
	if( perk < Perk_None || perk >= Perk_Count )
		return;
	loadout.perk = PerkType( perk );

	int gadget = atoi( Cmd_Argv( ARRAY_COUNT( loadout.weapons ) + 2 ) );
	if( gadget <= Gadget_None || gadget >= Gadget_Count )
		return;
	loadout.gadget = GadgetType( gadget );

	UI_ShowLoadoutMenu( loadout );
}

static void CG_SC_SaveLoadout() {
	if( !cgs.demoPlaying ) {
		Cvar_Set( "cg_loadout", Cmd_Args() );
	}
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
	{ "downloaddemo", CG_SC_DownloadDemo },
	{ "changeloadout", CG_SC_ChangeLoadout },
	{ "saveloadout", CG_SC_SaveLoadout },
};

void CG_GameCommand( const char * command ) {
	Cmd_TokenizeString( command );
	const char * name = Cmd_Argv( 0 );

	for( ServerCommand cmd : server_commands ) {
		if( StrEqual( name, cmd.name ) ) {
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
	for( WeaponType i = Weapon_None; i < Weapon_Count; i++ ) {
		const WeaponDef * weapon = GS_GetWeaponDef( i );
		if( StrCaseEqual( weapon->short_name, name ) && GS_CanEquip( &cg.predictedPlayerState, i ) ) {
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

	const SyncPlayerState * ps = &cg.predictedPlayerState;

	int num_weapons = int( ARRAY_COUNT( ps->weapons ) );

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
	const char * name;
	void ( *func )();
	bool allowdemo;
};

struct ClientToServerCommand {
	const char * name;
	ClientCommandType command;
};

static const cgcmd_t cgcmds[] = {
	{ "+scores", CG_ScoresOn_f, true },
	{ "-scores", CG_ScoresOff_f, true },
	{ "demoget", CG_Cmd_DemoGet_f, false },
	{ "use", CG_Cmd_UseItem_f, false },
	{ "lastweapon", CG_Cmd_LastWeapon_f, false },
	{ "weapnext", CG_Cmd_NextWeapon_f, false },
	{ "weapprev", CG_Cmd_PrevWeapon_f, false },
	{ "weapon", CG_Cmd_Weapon_f, false },
	{ "viewpos", CG_Viewpos_f, true },
};

static const ClientToServerCommand game_commands_no_args[] = {
	{ "noclip", ClientCommand_Noclip },
	{ "suicide", ClientCommand_Suicide },
	{ "spectate", ClientCommand_Spectate },
	{ "chasenext", ClientCommand_ChaseNext },
	{ "chaseprev", ClientCommand_ChasePrev },
	{ "togglefreefly", ClientCommand_ToggleFreeFly },
	{ "timeout", ClientCommand_Timeout },
	{ "timein", ClientCommand_Timein },
	{ "demolist", ClientCommand_DemoList },
	{ "ready", ClientCommand_Ready },
	{ "unready", ClientCommand_Unready },
	{ "toggleready", ClientCommand_ToggleReady },
	{ "spray", ClientCommand_Spray },
	{ "drop", ClientCommand_DropBomb },
	{ "loadoutmenu", ClientCommand_LoadoutMenu },
};

static const ClientToServerCommand game_commands_yes_args[] = {
	{ "position", ClientCommand_Position },
	{ "say", ClientCommand_Say },
	{ "say_team", ClientCommand_SayTeam },
	{ "callvote", ClientCommand_Callvote },
	{ "vote_yes", ClientCommand_VoteYes },
	{ "vote_no", ClientCommand_VoteNo },
	{ "op", ClientCommand_Operator },
	{ "opcall", ClientCommand_OpCall },
	{ "join", ClientCommand_Join },
	{ "vsay", ClientCommand_Vsay },
	{ "setloadout", ClientCommand_SetLoadout },
};

static void ReliableCommandNoArgs() {
	for( ClientToServerCommand cmd : game_commands_no_args ) {
		if( StrCaseEqual( cmd.name, Cmd_Argv( 0 ) ) ) {
			CL_AddReliableCommand( cmd.command );
			return;
		}
	}

	assert( false );
}

static void ReliableCommandYesArgs() {
	for( ClientToServerCommand cmd : game_commands_yes_args ) {
		if( StrCaseEqual( cmd.name, Cmd_Argv( 0 ) ) ) {
			msg_t * args = CL_AddReliableCommand( cmd.command );
			MSG_WriteString( args, Cmd_Args() );
			return;
		}
	}

	assert( false );
}

void CG_RegisterCGameCommands() {
	for( cgcmd_t cmd : cgcmds ) {
		if( cgs.demoPlaying && !cmd.allowdemo ) {
			continue;
		}
		AddCommand( cmd.name, cmd.func );
	}

	if( cgs.demoPlaying ) {
		return;
	}

	for( ClientToServerCommand cmd : game_commands_no_args ) {
		AddCommand( cmd.name, ReliableCommandNoArgs );
	}

	for( ClientToServerCommand cmd : game_commands_yes_args ) {
		AddCommand( cmd.name, ReliableCommandYesArgs );
	}
}

void CG_UnregisterCGameCommands() {
	for( const cgcmd_t & cmd : cgcmds ) {
		if( cgs.demoPlaying && !cmd.allowdemo ) {
			continue;
		}
		RemoveCommand( cmd.name );
	}

	if( cgs.demoPlaying ) {
		return;
	}

	for( ClientToServerCommand cmd : game_commands_no_args ) {
		RemoveCommand( cmd.name );
	}

	for( ClientToServerCommand cmd : game_commands_yes_args ) {
		RemoveCommand( cmd.name );
	}
}
