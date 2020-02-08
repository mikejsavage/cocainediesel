enum WindowZOrder {
	WindowZOrder_Chat,
	WindowZOrder_Scoreboard,
	WindowZOrder_Menu,
	WindowZOrder_Console,
};

namespace ImGui {
	void Begin( const char * name, WindowZOrder z_order, ImGuiWindowFlags flags );
	bool CloseKey( int key );
};
