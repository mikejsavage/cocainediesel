#include "client/client.h"
#include "client/renderer/renderer.h"
#include "qcommon/string.h"
#include "qcommon/utf8.h"
#include "qcommon/threads.h"

#include "imgui/imgui.h"

// TODO: revamp key_dest garbage
// TODO: finish cleaning up old stuff

static constexpr size_t CONSOLE_LOG_SIZE = 1000 * 1000; // 1MB
static constexpr size_t CONSOLE_INPUT_SIZE = 1024;

struct HistoryEntry {
	char cmd[ CONSOLE_INPUT_SIZE ];
};

struct Console {
	String< CONSOLE_LOG_SIZE > log;
	Mutex * log_mutex = NULL;

	char input[ CONSOLE_INPUT_SIZE ];

	bool at_bottom;
	bool scroll_to_bottom;
	bool visible;

	HistoryEntry input_history[ 64 ];
	size_t history_head;
	size_t history_count;
	size_t history_idx;
};

static Console console;

static void Con_ClearInput() {
	console.input[ 0 ] = '\0';
	console.history_idx = 0;
}

void Con_Init() {
	console.log_mutex = NewMutex();

	console.log.clear();
	Con_ClearInput();

	console.at_bottom = true;
	console.scroll_to_bottom = false;
	console.visible = false;

	console.history_head = 0;
	console.history_count = 0;
}

void Con_Shutdown() {
	DeleteMutex( console.log_mutex );
}

void Con_ToggleConsole() {
	if( cls.state == CA_CONNECTING || cls.state == CA_CONNECTED ) {
		return;
	}

	if( console.visible ) {
		CL_SetKeyDest( cls.old_key_dest );
	}
	else {
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
	if( console.log_mutex == NULL )
		return;

	Lock( console.log_mutex );
	defer { Unlock( console.log_mutex ); };

	// delete lines until we have enough space to add str
	size_t len = strlen( str );
	size_t trim = 0;
	while( console.log.length() - trim + len >= CONSOLE_LOG_SIZE ) {
		const char * newline = strchr( console.log.c_str() + trim, '\n' );
		if( newline == NULL ) {
			trim = console.log.length();
			break;
		}

		trim += newline - ( console.log.c_str() + trim ) + 1;
	}

	console.log.remove( 0, trim );
	console.log += S_COLOR_WHITE;
	console.log.append_raw( str, len );

	if( console.at_bottom ) {
		console.scroll_to_bottom = true;
	}
}

static void PrintCompletions( Span< const char * > completions, const char * color, const char * type ) {
	if( completions.n == 0 )
		return;

	Com_Printf( "%s%zu possible %s%s\n", color, completions.n, type, completions.n > 1 ? "s" : "" );
	for( const char * completion : completions ) {
		Com_Printf( "%s ", completion );
	}
	Com_Printf( "\n" );
}

static size_t CommonPrefixLength( const char * a, Span< const char > b ) {
	size_t n = Min2( strlen( a ), b.n );
	for( size_t i = 0; i < n; i++ ) {
		if( tolower( a[ i ] ) != tolower( b[ i ] ) ) {
			return i;
		}
	}
	return n;
}

static Span< const char > FindCommonPrefix( Span< const char > prefix, Span< const char * > strings ) {
	for( const char * str : strings ) {
		if( prefix.ptr == NULL ) {
			prefix = MakeSpan( str );
		}
		prefix.n = CommonPrefixLength( str, prefix );
	}

	return prefix;
}

static void TabCompletion( char * buf, size_t buf_size ) {
	char * input = buf;
	if( *input == '\\' || *input == '/' )
		input++;
	if( strlen( input ) == 0 )
		return;

	TempAllocator temp = cls.frame_arena.temp();

	Span< const char * > cvars, commands, arguments;
	char * space = strchr( input, ' ' );

	if( space == NULL ) {
		cvars = TabCompleteCvar( &temp, input );
		commands = TabCompleteCommand( &temp, input );
		if( cvars.n == 0 && commands.n == 0 ) {
			Com_Printf( "No matching commands or cvars were found.\n" );
			return;
		}

		Span< const char > shared_prefix;
		shared_prefix = FindCommonPrefix( shared_prefix, cvars );
		shared_prefix = FindCommonPrefix( shared_prefix, commands );

		Q_strncpyz( buf, temp( "{}", shared_prefix ), buf_size );
		if( cvars.n + commands.n == 1 ) {
			Q_strncatz( buf, " ", buf_size );
		}
		else {
			PrintCompletions( commands, S_COLOR_RED, "command" );
			PrintCompletions( cvars, S_COLOR_RED, "cvar" );
		}
	}
	else {
		arguments = TabCompleteArgument( &temp, input );
		if( arguments.n == 0 ) {
			return;
		}

		if( arguments.n > 1 ) {
			PrintCompletions( arguments, S_COLOR_GREEN, "argument" );
		}

		Span< const char > shared_prefix = FindCommonPrefix( Span< const char >(), arguments );
		while( *space == ' ' )
			space++;
		Q_strncpyz( space, temp( "{}", shared_prefix ), buf_size - ( space - buf ) );
	}
}

static int InputCallback( ImGuiInputTextCallbackData * data ) {
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

	return 0;
}

static void Con_Execute() {
	if( StrEqual( console.input, "" ) )
		return;

	TempAllocator temp = cls.frame_arena.temp();

	Com_Printf( "> %s\n", console.input );

	const char * skip_slash = console.input;
	if( skip_slash[ 0 ] == '/' || skip_slash[ 0 ] == '\\' )
		skip_slash++;

	bool try_chat = cls.state == CA_ACTIVE;
	bool executed = Cbuf_ExecuteLine( MakeSpan( skip_slash ), !try_chat );
	if( !executed && try_chat ) {
		Cbuf_ExecuteLine( temp( "say {}", console.input ) );
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
			Lock( console.log_mutex );
			defer { Unlock( console.log_mutex ); };

			ImGui::PushTextWrapPos( 0 );
			const char * p = console.log.c_str();
			while( p != NULL ) {
				const char * end = NextChunkEnd( p );
				ImGui::TextUnformatted( p, end );
				p = end;
			}
			ImGui::PopTextWrapPos();

			if( console.scroll_to_bottom ) {
				ImGui::SetScrollHereY( 1.0f );
				console.scroll_to_bottom = false;
			}

			if( ImGui::IsKeyPressed( K_PGUP ) || ImGui::IsKeyPressed( K_PGDN ) ) {
				float scroll = ImGui::GetScrollY();
				float page = ImGui::GetWindowSize().y - ImGui::GetTextLineHeight();
				scroll += page * ( ImGui::IsKeyPressed( K_PGUP ) ? -1 : 1 );
				scroll = Clamp( 0.0f, scroll, ImGui::GetScrollMaxY() );
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
		input_flags |= ImGuiInputTextFlags_CallbackCompletion;
		input_flags |= ImGuiInputTextFlags_CallbackHistory;
		input_flags |= ImGuiInputTextFlags_EnterReturnsTrue;

		ImGui::PushItemWidth( ImGui::GetWindowWidth() );
		// don't steal focus if the user is dragging the scrollbar
		if( !ImGui::IsAnyItemActive() )
			ImGui::SetKeyboardFocusHere();
		bool enter = ImGui::InputText( "##consoleinput", console.input, sizeof( console.input ), input_flags, InputCallback );
		ImGui::PopItemWidth();

		if( enter ) {
			Con_Execute();
			console.scroll_to_bottom = true;
		}

		ImVec2 top_left = ImGui::GetCursorPos();
		top_left.y -= 1;
		ImVec2 bottom_right = top_left;
		bottom_right.x += ImGui::GetWindowWidth();
		bottom_right.y += 2;
		ImGui::GetWindowDrawList()->AddRectFilled( top_left, bottom_right, ImGui::GetColorU32( ImGuiCol_Separator ) );
	}

	if( ImGui::Hotkey( K_ESCAPE ) ) {
		Con_Close();
	}

	ImGui::End();
	ImGui::PopStyleVar( 3 );
	ImGui::PopStyleColor( 2 );
	ImGui::PopFont();
}
