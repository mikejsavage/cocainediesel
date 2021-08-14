#pragma once

#include "qcommon/types.h"

bool Cvar_Initialized();

// this is set each time a CVAR_USERINFO variable is changed so
// that the client knows to send it to the server
extern bool userinfo_modified;

cvar_t *Cvar_Get( const char *var_name, const char *value, cvar_flag_t flags );
cvar_t *Cvar_Set( const char *var_name, const char *value );
cvar_t *Cvar_ForceSet( const char *var_name, const char *value );
cvar_t *Cvar_FullSet( const char *var_name, const char *value, cvar_flag_t flags, bool overwrite_flags );
void Cvar_SetValue( const char *var_name, float value );
float Cvar_Value( const char *var_name );
const char *Cvar_String( const char *var_name );
int Cvar_Integer( const char *var_name );
cvar_t *Cvar_Find( const char *var_name );
int Cvar_CompleteCountPossible( const char *partial );
const char **Cvar_CompleteBuildList( const char *partial );
char *Cvar_TabComplete( const char *partial );
void Cvar_GetLatchedVars( cvar_flag_t flags );
void Cvar_FixCheatVars();
bool Cvar_Command();

bool Cvar_CheatsAllowed();

class DynamicString;
void        Cvar_WriteVariables( DynamicString * config );

void Cvar_PreInit();
void Cvar_Init();
void Cvar_Shutdown();
char *Cvar_Userinfo();
char *Cvar_Serverinfo();
