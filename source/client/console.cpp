#include "client/client.h"
#include "qcommon/string.h"
#include "qcommon/utf8.h"

#include "imgui/imgui.h"

// TODO: revamp key_dest garbage
// TODO: finish cleaning up old stuff
// TODO: check if mutex is really needed

static constexpr size_t CONSOLE_LOG_SIZE = 1000 * 1000; // 1MB
static constexpr size_t CONSOLE_INPUT_SIZE = 1024;

struct HistoryEntry {
	char cmd[ CONSOLE_INPUT_SIZE ];
};

struct Console {
	String< CONSOLE_LOG_SIZE > log;

	char input[ CONSOLE_INPUT_SIZE ];

	bool at_bottom;
	bool scroll_to_bottom;
	bool visible;

	HistoryEntry input_history[ 64 ];
	size_t history_head;
	size_t history_count;
	size_t history_idx;

	qmutex_t * mutex = NULL;
};

static Console console;

static void Con_ClearScrollback() {
	console.log.clear();
}

static void Con_ClearInput() {
	console.input[ 0 ] = '\0';
	console.history_idx = 0;
}

void Con_Init() {
	Con_ClearScrollback();
	Con_ClearInput();

	console.at_bottom = true;
	console.scroll_to_bottom = false;
	console.visible = false;

	console.history_head = 0;
	console.history_count = 0;

	console.mutex = QMutex_Create();

	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole );
	Cmd_AddCommand( "clear", Con_ClearScrollback );
	// Cmd_AddCommand( "condump", Con_Dump );
}

void Con_Shutdown() {
	QMutex_Destroy( &console.mutex );

	Cmd_RemoveCommand( "toggleconsole" );
	Cmd_RemoveCommand( "clear" );
	// Cmd_RemoveCommand( "condump" );
}

void Con_ToggleConsole() {
	if( cls.state == CA_CONNECTING || cls.state == CA_CONNECTED ) {
		return;
	}

	if( console.visible ) {
		CL_SetKeyDest( cls.old_key_dest );
	} else {
		CL_SetOldKeyDest( cls.key_dest );
		CL_SetKeyDest( key_console );
	}

	Con_ClearInput();

	console.scroll_to_bottom = true;
	console.visible = !console.visible;
}

bool Con_IsVisible() {
	return console.visible;
}

void Con_Close() {
	if( console.visible ) {
		CL_SetKeyDest( cls.old_key_dest );
		console.visible = false;
	}
}

void Con_Print( const char * str ) {
	if( console.mutex == NULL )
		return;

	QMutex_Lock( console.mutex );

	// delete lines until we have enough space to add str
	size_t len = strlen( str );
	size_t trim = 0;
	while( console.log.len() - trim + len >= CONSOLE_LOG_SIZE ) {
		const char * newline = StrChrUTF8( console.log.c_str() + trim, '\n' );
		if( newline == NULL ) {
			trim = console.log.len();
			break;
		}

		trim += newline - ( console.log.c_str() + trim ) + 1;
	}

	console.log.remove( 0, trim );
	console.log.append_raw( str, len );

	if( console.at_bottom )
		console.scroll_to_bottom = true;

	QMutex_Unlock( console.mutex );
}

static void TabCompletion( char * buf, int buf_size );

static int InputCallback( ImGuiInputTextCallbackData * data ) {
	if( data->EventChar == 0 ) {
		bool dirty = false;

		if( data->EventKey == ImGuiKey_Tab ) {
			TabCompletion( data->Buf, data->BufSize );
			dirty = true;
		}
		else if( data->EventKey == ImGuiKey_UpArrow || data->EventKey == ImGuiKey_DownArrow ) {
			if( data->EventKey == ImGuiKey_UpArrow && console.history_idx < console.history_count ) {
				console.history_idx++;
				dirty = true;
			}
			if( data->EventKey == ImGuiKey_DownArrow && console.history_idx > 0 ) {
				console.history_idx--;
				dirty = true;
			}
			if( dirty ) {
				if( console.history_idx == 0 ) {
					data->Buf[ 0 ] = '\0';
				}
				else {
					size_t idx = ( console.history_head + console.history_count - console.history_idx ) % ARRAY_COUNT( console.input_history );
					strcpy( data->Buf, console.input_history[ idx ].cmd );
				}
			}
		}

		if( dirty ) {
			data->BufDirty = true;
			data->BufTextLen = strlen( data->Buf );
			data->CursorPos = strlen( data->Buf );
		}
	}

	return 0;
}

static void Con_Execute() {
	if( strlen( console.input ) != 0 ) {
		bool chat = true;
		chat = chat && cls.state == CA_ACTIVE;
		chat = chat && console.input[ 0 ] != '/' && console.input[ 0 ] != '\\';
		chat = chat && !Cmd_CheckForCommand( console.input );

		if( chat ) {
			char * p = console.input;
			while( ( p = StrChrUTF8( p, '"' ) ) != NULL )
				*p = '\'';
			Cbuf_AddText( "say \"" );
			Cbuf_AddText( console.input );
			Cbuf_AddText( "\"\n" );
		}
		else {
			const char * cmd = console.input;
			if( cmd[ 0 ] == '/' || cmd[ 0 ] == '\\' )
				cmd++;
			Cbuf_AddText( cmd );
			Cbuf_AddText( "\n" );
		}

		const HistoryEntry * last = &console.input_history[ ( console.history_head + console.history_count - 1 ) % ARRAY_COUNT( console.input_history ) ];

		if( console.history_count == 0 || strcmp( last->cmd, console.input ) != 0 ) {
			HistoryEntry * entry = &console.input_history[ ( console.history_head + console.history_count ) % ARRAY_COUNT( console.input_history ) ];
			strcpy( entry->cmd, console.input );

			if( console.history_count == ARRAY_COUNT( console.input_history ) ) {
				console.history_head++;
			}
			else {
				console.history_count++;
			}
		}
	}

	Com_Printf( "> %s\n", console.input );

	Con_ClearInput();
}

// break str into small chunks so we can print them individually because the
// renderer doesn't like large dynamic meshes
const char * NextChunkEnd( const char * str ) {
	const char * p = str;
	while( true ) {
		p = strchr( p, '\n' );
		if( p == NULL )
			break;
		p++;
		if( p - str > 512 )
			return p;
	}
	return NULL;
}

void Con_Draw() {
	QMutex_Lock( console.mutex );

	u32 bg = IM_COL32( 27, 27, 27, 224 );

	ImGui::PushFont( cls.console_font );
	ImGui::PushStyleColor( ImGuiCol_FrameBg, bg );
	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0, 0, 0, 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 8, 4 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );

	// make a fullscreen window so you can't interact with menus while console is open
	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width, frame_static.viewport_height ) );
	ImGui::Begin( "console", WindowZOrder_Console, ImGuiWindowFlags_NoDecoration );

	{
		ImGui::PushStyleColor( ImGuiCol_ChildBg, bg );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8, 4 ) );
		ImGui::BeginChild( "consoletext", ImVec2( 0, frame_static.viewport_height * 0.4 - ImGui::GetFrameHeightWithSpacing() - 3 ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );
		{
			ImGui::PushTextWrapPos( 0 );
			const char * p = console.log.c_str();
			while( p != NULL ) {
				const char * end = NextChunkEnd( p );
				ImGui::TextUnformatted( p, end );
				p = end;
			}
			ImGui::PopTextWrapPos();

			if( console.scroll_to_bottom )
				ImGui::SetScrollHereY( 1.0f );
			console.scroll_to_bottom = false;

			if( ImGui::IsKeyPressed( K_PGUP ) || ImGui::IsKeyPressed( K_PGDN ) ) {
				float scroll = ImGui::GetScrollY();
				float page = ImGui::GetWindowSize().y - ImGui::GetTextLineHeight();
				scroll += page * ( ImGui::IsKeyPressed( K_PGUP ) ? -1 : 1 );
				scroll = bound( 0.0f, scroll, ImGui::GetScrollMaxY() );
				ImGui::SetScrollY( scroll );
			}

			console.at_bottom = ImGui::GetScrollY() == ImGui::GetScrollMaxY();
		}

		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();

		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 1 ) );
		ImGui::Separator();
		ImGui::PopStyleVar();

		ImGuiInputTextFlags input_flags = 0;
		input_flags |= ImGuiInputTextFlags_CallbackCharFilter;
		input_flags |= ImGuiInputTextFlags_CallbackCompletion;
		input_flags |= ImGuiInputTextFlags_CallbackHistory;
		input_flags |= ImGuiInputTextFlags_EnterReturnsTrue;

		ImGui::PushItemWidth( ImGui::GetWindowWidth() );
		ImGui::SetKeyboardFocusHere();
		bool enter = ImGui::InputText( "##consoleinput", console.input, sizeof( console.input ), input_flags, InputCallback );
		// can't drag the scrollbar without this
		if( !ImGui::IsAnyItemActive() )
			ImGui::SetKeyboardFocusHere();
		ImGui::PopItemWidth();

		if( enter ) {
			Con_Execute();
		}

		ImVec2 top_left = ImGui::GetCursorPos();
		top_left.y -= 1;
		ImVec2 bottom_right = top_left;
		bottom_right.x += ImGui::GetWindowWidth();
		bottom_right.y += 2;
		ImGui::GetWindowDrawList()->AddRectFilled( top_left, bottom_right, ImGui::GetColorU32( ImGuiCol_Separator ) );
	}

	if( ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) && ImGui::IsKeyPressed( K_ESCAPE, false ) ) {
		Con_Close();
	}

	ImGui::End();
	ImGui::PopStyleVar( 3 );
	ImGui::PopStyleColor( 2 );
	ImGui::PopFont();

	QMutex_Unlock( console.mutex );
}

static void Con_DisplayList( const char ** list ) {
	for( size_t i = 0; list[ i ] != NULL; i++ ) {
		Com_Printf( "%s ", list[ i ] );
	}
	Com_Printf( "\n" );
}

static size_t CommonPrefixLength( const char * a, const char * b ) {
	size_t n = min( strlen( a ), strlen( b ) );
	size_t len = 0;
	for( size_t i = 0; i < n; i++ ) {
		if( a[ i ] != b[ i ] )
			break;
		len++;
	}
	return len;
}

static void TabCompletion( char * buf, int buf_size ) {
	char * input = buf;
	if( *input == '\\' || *input == '/' )
		input++;
	if( strlen( input ) == 0 )
		return;

	// Count number of possible matches
	int c = Cmd_CompleteCountPossible( input );
	int v = Cvar_CompleteCountPossible( input );
	int a = Cmd_CompleteAliasCountPossible( input );
	int ca = 0;

	const char ** completion_lists[ 4 ] = { };

	const char * completion = NULL;

	if( !( c + v + a ) ) {
		// now see if there's any valid cmd in there, providing
		// a list of matching arguments
		completion_lists[3] = Cmd_CompleteBuildArgList( input );
		if( !completion_lists[3] ) {
			// No possible matches, let the user know they're insane
			Com_Printf( "\nNo matching aliases, commands, or cvars were found.\n" );
			return;
		}

		// count the number of matching arguments
		while( completion_lists[3][ca] != NULL )
			ca++;
		if( ca == 0 ) {
			// the list is empty, although non-NULL list pointer suggests that the command
			// exists, so clean up and exit without printing anything
			Mem_TempFree( completion_lists[3] );
			return;
		}
	}

	if( c != 0 ) {
		completion_lists[0] = Cmd_CompleteBuildList( input );
		completion = *completion_lists[0];
	}
	if( v != 0 ) {
		completion_lists[1] = Cvar_CompleteBuildList( input );
		completion = *completion_lists[1];
	}
	if( a != 0 ) {
		completion_lists[2] = Cmd_CompleteAliasBuildList( input );
		completion = *completion_lists[2];
	}
	if( ca != 0 ) {
		input = StrChrUTF8( input, ' ' ) + 1;
		completion = *completion_lists[3];
	}

	size_t common_prefix_len = SIZE_MAX;
	for( size_t i = 0; i < ARRAY_COUNT( completion_lists ); i++ ) {
		if( completion_lists[ i ] == NULL )
			continue;
		const char ** candidate = &completion_lists[ i ][ 0 ];
		while( *candidate != NULL ) {
			common_prefix_len = min( common_prefix_len, CommonPrefixLength( completion, *candidate ) );
			candidate++;
		}
	}

	int total_candidates = c + v + a + ca;
	if( total_candidates > 1 ) {
		if( c != 0 ) {
			Com_Printf( S_COLOR_RED "%i possible command%s%s\n", c, ( c > 1 ) ? "s: " : ":", S_COLOR_WHITE );
			Con_DisplayList( completion_lists[0] );
		}
		if( v != 0 ) {
			Com_Printf( S_COLOR_CYAN "%i possible variable%s%s\n", v, ( v > 1 ) ? "s: " : ":", S_COLOR_WHITE );
			Con_DisplayList( completion_lists[1] );
		}
		if( a != 0 ) {
			Com_Printf( S_COLOR_MAGENTA "%i possible alias%s%s\n", a, ( a > 1 ) ? "es: " : ":", S_COLOR_WHITE );
			Con_DisplayList( completion_lists[2] );
		}
		if( ca != 0 ) {
			Com_Printf( S_COLOR_GREEN "%i possible argument%s%s\n", ca, ( ca > 1 ) ? "s: " : ":", S_COLOR_WHITE );
			Con_DisplayList( completion_lists[3] );
		}
	}

	if( completion != NULL ) {
		size_t to_copy = min( common_prefix_len + 1, buf_size - ( input - console.input ) );
		Q_strncpyz( input, completion, to_copy );
		if( total_candidates == 1 )
			Q_strncatz( buf, " ", buf_size );
	}

	for( size_t i = 0; i < ARRAY_COUNT( completion_lists ); i++ ) {
		Mem_TempFree( completion_lists[i] );
	}
}
