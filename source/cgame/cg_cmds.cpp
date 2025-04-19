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

static void CG_SC_Print( const Tokenized & args ) {
	CG_LocalPrint( args.tokens[ 1 ] );
}

static void CG_SC_ChatPrint( const Tokenized & args ) {
	TempAllocator temp = cls.frame_arena.temp();

	bool teamonly = StrCaseEqual( args.tokens[ 0 ], "tch" );
	int who = SpanToInt( args.tokens[ 1 ], -1 );

	if( who < 0 || who > MAX_CLIENTS ) {
		return;
	}

	if( Cvar_Integer( "cg_chat" ) == 0 ) {
		return;
	}

	if( Cvar_Integer( "cg_chat" ) == 2 && !teamonly ) {
		return;
	}

	if( who == 0 ) {
		CG_LocalPrint( temp.sv( "Console: {}\n", args.tokens[ 2 ] ) );
		return;
	}

	Span< const char > name = PlayerName( who - 1 );
	Team team = cg_entities[ who ].current.team;
	RGB8 team_color = team == Team_None ? RGB8( 128, 128, 128 ) : CG_TeamColor( team );

	Span< const char > prefix = "";
	if( teamonly ) {
		prefix = team == Team_None ? "[SPEC] " : "[TEAM] ";
	}

	ImGuiColorToken color( team_color );
	CG_LocalPrint( temp.sv( "{}{}{}{}: {}\n",
		prefix,
		ImGuiColorToken( team_color ), name,
		ImGuiColorToken( white.srgb ), args.tokens[ 2 ]
	) );
}

static void CG_SC_CenterPrint( const Tokenized & args ) {
	CG_CenterPrint( args.tokens[ 1 ] );
}

static void CG_SC_Debug( const Tokenized & args ) {
	if( cg_showServerDebugPrints->integer != 0 ) {
		Com_GGPrint( S_COLOR_ORANGE "Debug: {}", args.tokens[ 1 ] );
	}
}

static bool demo_requested = false;
static void CG_Cmd_DemoGet_f( const Tokenized & args ) {
	if( demo_requested ) {
		Com_Printf( "Already requesting a demo\n" );
		return;
	}

	msg_t * msg = CL_AddReliableCommand( ClientCommand_DemoGetURL );
	MSG_WriteString( msg, args.tokens[ 1 ] );

	demo_requested = true;
}

static void CG_SC_DownloadDemo( const Tokenized & args ) {
	if( cgs.demoPlaying ) {
		// ignore download commands coming from demo files
		return;
	}

	if( !demo_requested ) {
		Com_Printf( "Warning: demoget when not requested, ignored\n" );
		return;
	}

	demo_requested = false;

	Span< const char > filename = args.tokens[ 1 ];
	Span< const char > extension = FileExtension( filename );
	if( !COM_ValidateRelativeFilename( filename ) || extension != APP_DEMO_EXTENSION_STR ) {
		Com_Printf( "Warning: demoget: Invalid filename, ignored\n" );
		return;
	}

	CL_DownloadFile( filename, []( Span< const char > filename, Span< const u8 > data ) {
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

static void CG_SC_ChangeLoadout( const Tokenized & args ) {
	if( cgs.demoPlaying )
		return;

	Loadout loadout = { };

	for( size_t i = 0; i < ARRAY_COUNT( loadout.weapons ); i++ ) {
		u64 weapon = SpanToU64( args.tokens[ i + 1 ], Weapon_Count );
		if( weapon == Weapon_None || weapon >= Weapon_Count )
			return;
		loadout.weapons[ i ] = WeaponType( weapon );
	}

	u64 perk = SpanToU64( args.tokens[ ARRAY_COUNT( loadout.weapons ) + 1 ], Perk_Count );
	if( perk >= Perk_Count )
		return;
	loadout.perk = PerkType( perk );

	u64 gadget = SpanToU64( args.tokens[ ARRAY_COUNT( loadout.weapons ) + 2 ], Gadget_Count );
	if( gadget == Gadget_None || gadget >= Gadget_Count )
		return;
	loadout.gadget = GadgetType( gadget );

	UI_ShowLoadoutMenu( loadout );
}

static void CG_SC_SaveLoadout( const Tokenized & args ) {
	if( !cgs.demoPlaying ) {
		Cvar_Set( "cg_loadout", args.all_but_first );
	}
}

struct ServerCommand {
	Span< const char > name;
	void ( *func )( const Tokenized & args );
	Optional< size_t > num_args = NONE;
};

static const ServerCommand server_commands[] = {
	{ "pr", CG_SC_Print, 1 },
	{ "ch", CG_SC_ChatPrint, 2 },
	{ "tch", CG_SC_ChatPrint, 2 },
	{ "cp", CG_SC_CenterPrint, 1 },
	{ "obry", CG_SC_Obituary, 6 },
	{ "debug", CG_SC_Debug, 1 },
	{ "downloaddemo", CG_SC_DownloadDemo, 1 },
	{ "changeloadout", CG_SC_ChangeLoadout, ARRAY_COUNT( &Loadout::weapons ) + 2 },
	{ "saveloadout", CG_SC_SaveLoadout },
};

void CG_GameCommand( const char * command ) {
	TempAllocator temp = cls.frame_arena.temp();
	Tokenized args = Tokenize( &temp, MakeSpan( command ) );
	Assert( args.tokens.n > 0 );

	for( ServerCommand cmd : server_commands ) {
		if( StrEqual( args.tokens[ 0 ], cmd.name ) ) {
			if( !cmd.num_args.exists || args.tokens.n == cmd.num_args.value + 1 ) {
				cmd.func( args );
				return;
			}
		}
	}

	Assert( is_public_build );
}

/*
==========================================================================

CGAME COMMANDS

==========================================================================
*/

static void SwitchWeapon( WeaponType weapon ) {
	cl.weaponSwitch = weapon;
}

static void CG_Cmd_UseWeapon_f( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "Usage: use <weapon>\n" );
		return;
	}

	for( WeaponType i = Weapon_None; i < Weapon_Count; i++ ) {
		const WeaponDef * weapon = GS_GetWeaponDef( i );
		if( StrCaseEqual( weapon->name, args.tokens[ 1 ] ) && GS_CanEquip( &cg.predictedPlayerState, i ) ) {
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
	if( weapon != Weapon_None && GS_GetWeaponDef( weapon )->category != WeaponCategory_Melee ) {
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

	if( GS_GetWeaponDef( cg.predictedPlayerState.weapon )->category == WeaponCategory_Melee ) {
		SwitchWeapon( cg.predictedPlayerState.last_weapon );
	} else {
		ScrollWeapon( -1 );
	}
}

static void CG_Cmd_LastWeapon_f() {
	SwitchWeapon( cg.predictedPlayerState.last_weapon );
}

static void CG_Cmd_Weapon_f( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "Usage: weapon <slot>\n" );
		return;
	}

	u64 slot;
	if( !TrySpanToU64( args.tokens[ 1 ], &slot ) || slot < 1 || slot > ARRAY_COUNT( cg.predictedPlayerState.weapons ) )
		return;

	WeaponType weapon = cg.predictedPlayerState.weapons[ slot - 1 ].weapon;
	if( weapon != Weapon_None ) {
		SwitchWeapon( weapon );
	}
}

static void CG_Viewpos_f() {
	Com_Printf( "\"origin\" \"%i %i %i\"\n", (int)cg.view.origin.x, (int)cg.view.origin.y, (int)cg.view.origin.z );
	Com_Printf( "\"angles\" \"%i %i %i\"\n", (int)cg.view.angles.pitch, (int)cg.view.angles.yaw, (int)cg.view.angles.roll );
}

// local cgame commands
struct cgcmd_t {
	Span< const char > name;
	void ( *func )( const Tokenized & args );
	bool allowdemo;
};

struct ClientToServerCommand {
	Span< const char > name;
	ClientCommandType command;
};

static const cgcmd_t cgcmds[] = {
	{ "+scores", []( const Tokenized & args ) { []() { cg.showScoreboard = true; }(); }, true },
	{ "-scores", []( const Tokenized & args ) { []() { cg.showScoreboard = false; }(); }, true },
	{ "demoget", CG_Cmd_DemoGet_f, false },
	{ "use", CG_Cmd_UseWeapon_f, false },
	{ "lastweapon", []( const Tokenized & args ) { CG_Cmd_LastWeapon_f(); }, false },
	{ "weapnext", []( const Tokenized & args ) { CG_Cmd_NextWeapon_f(); }, false },
	{ "weapprev", []( const Tokenized & args ) { CG_Cmd_PrevWeapon_f(); }, false },
	{ "weapon", CG_Cmd_Weapon_f, false },
	{ "viewpos", []( const Tokenized & args ) { CG_Viewpos_f(); }, true },
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
	{ "vote_yes", ClientCommand_VoteYes },
	{ "vote_no", ClientCommand_VoteNo },
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
	{ "join", ClientCommand_Join },
	{ "vsay", ClientCommand_Vsay },
	{ "setloadout", ClientCommand_SetLoadout },
};

static void ReliableCommandNoArgs( const Tokenized & args ) {
	for( ClientToServerCommand cmd : game_commands_no_args ) {
		if( StrCaseEqual( cmd.name, args.tokens[ 0 ] ) ) {
			CL_AddReliableCommand( cmd.command );
			return;
		}
	}

	Assert( false );
}

static void ReliableCommandYesArgs( const Tokenized & args ) {
	for( ClientToServerCommand cmd : game_commands_yes_args ) {
		if( StrCaseEqual( cmd.name, args.tokens[ 0 ] ) ) {
			msg_t * msg = CL_AddReliableCommand( cmd.command );
			MSG_WriteString( msg, args.all_but_first );
			return;
		}
	}

	Assert( false );
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
