void Con_Init();
void Con_Shutdown();

void Con_Print( const char * str );
void Con_Clear();
// imgui text fields don't have pageup/down callbacks so we have to do it like this
void Con_Draw( int pressed_key );

void Con_ToggleConsole();
bool Con_IsVisible();
void Con_Close();
