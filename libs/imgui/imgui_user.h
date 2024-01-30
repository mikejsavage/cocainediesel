constexpr ImGuiWindowFlags ImGuiWindowFlags_Interactive = 1 << 29;

enum WindowZOrder {
	WindowZOrder_Chat,
	WindowZOrder_Scoreboard,
	WindowZOrder_Menu,
	WindowZOrder_Console,
};

namespace ImGui {
	void Begin( const char * name, WindowZOrder z_order, ImGuiWindowFlags flags );
	bool Hotkey( int key );

	ImVec2 CalcTextSize( Span< const char > str );
	void Text( Span< const char > str );
	void PushID( Span< const char > id );
};
