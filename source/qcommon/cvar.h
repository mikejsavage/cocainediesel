#pragma once

#include "qcommon/types.h"

enum CvarFlags {
	CvarFlag_Archive        = 1 << 0,
	CvarFlag_UserInfo       = 1 << 1,
	CvarFlag_ServerInfo     = 1 << 2,
	CvarFlag_Cheat          = 1 << 3,
	CvarFlag_ReadOnly       = 1 << 4,
	CvarFlag_ServerReadOnly = 1 << 5,
	CvarFlag_Developer      = 1 << 6,
	CvarFlag_FromConfig     = 1 << 7,
};

struct Cvar {
	char * name;
	char * value;
	char * default_value;

	CvarFlags flags;
	float number;
	int integer;
	bool modified;
};

// this is set each time a CVAR_USERINFO variable is changed so
// that the client knows to send it to the server
extern bool userinfo_modified;

Cvar * NewCvar( const char * name, const char * value, u32 flags );

bool IsCvar( const char * name );
const char * Cvar_String( const char * name );
int Cvar_Integer( const char * name );

void Cvar_Set( const char * name, const char * value );
void Cvar_SetInteger( const char * name, int value );
void Cvar_ForceSet( const char * name, const char * value );

Span< const char * > TabCompleteCvar( TempAllocator * a, const char * partial );
void ResetCheatCvars();
bool Cvar_Command();

bool Cvar_CheatsAllowed();

class DynamicString;
void Cvar_WriteVariables( DynamicString * config );

void Cvar_PreInit();
void Cvar_Init();
void Cvar_Shutdown();
const char * Cvar_GetUserInfo();
const char * Cvar_GetServerInfo();
