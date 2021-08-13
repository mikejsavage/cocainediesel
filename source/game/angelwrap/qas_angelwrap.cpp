/*
Copyright (C) 2008 German Garcia

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

#include "qas_local.h"
#include "addon/addon_math.h"
#include "addon/addon_scriptarray.h"
#include "addon/addon_string.h"
#include "addon/addon_vec3.h"
#include "addon/addon_cvar.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"

#include <list>
#include <map>

static void *qasAlloc( size_t size ) {
	return G_Malloc( size );
}

static void qasFree( void *mem ) {
	G_Free( mem );
}

// ============================================================================

// list of contexts in the same engine
typedef std::list<asIScriptContext *> qasContextList;

// engine -> contexts key/value pairs
typedef std::map<asIScriptEngine *, qasContextList> qasEngineContextMap;

qasEngineContextMap contexts;

// ============================================================================

static void qasMessageCallback( const asSMessageInfo *msg ) {
	const char *msg_type;

	switch( msg->type ) {
		case asMSGTYPE_ERROR:
			msg_type = S_COLOR_RED "ERROR: ";
			break;
		case asMSGTYPE_WARNING:
			msg_type = S_COLOR_YELLOW "WARNING: ";
			break;
		case asMSGTYPE_INFORMATION:
		default:
			msg_type = S_COLOR_CYAN "ANGELSCRIPT: ";
			break;
	}

	Com_Printf( "%s%s %d:%d: %s\n", msg_type, msg->section, msg->row, msg->col, msg->message );
}

static void qasExceptionCallback( asIScriptContext *ctx ) {
	int line, col;
	asIScriptFunction *func;
	const char *sectionName, *exceptionString, *funcDecl;

	line = ctx->GetExceptionLineNumber( &col, &sectionName );
	func = ctx->GetExceptionFunction();
	exceptionString = ctx->GetExceptionString();
	funcDecl = ( func ? func->GetDeclaration( true ) : "" );

	Com_Printf( S_COLOR_RED "ASModule::ExceptionCallback:\n%s %d:%d %s: %s\n", sectionName, line, col, funcDecl, exceptionString );
}

asIScriptEngine *qasCreateEngine( bool *asMaxPortability ) {
	asIScriptEngine *engine;

	// register the global memory allocation and deallocation functions
	asSetGlobalMemoryFunctions( qasAlloc, qasFree );

	// always new

	// ask for angelscript initialization and script engine creation
	engine = asCreateScriptEngine( ANGELSCRIPT_VERSION );
	if( !engine ) {
		return NULL;
	}

	if( strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) ) {
		Com_Printf( "* angelscript library with AS_MAX_PORTABILITY detected\n" );
		engine->Release();
		return NULL;
	}

	*asMaxPortability = false;

	// The script compiler will write any compiler messages to the callback.
	engine->SetMessageCallback( asFUNCTION( qasMessageCallback ), 0, asCALL_CDECL );
	engine->SetEngineProperty( asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, 1 );
	engine->SetDefaultAccessMask( 0xFFFFFFFF );

	PreRegisterMathAddon( engine );
	PreRegisterScriptArray( engine, true );
	PreRegisterStringAddon( engine );
	PreRegisterVec3Addon( engine );
	PreRegisterCvarAddon( engine );

	RegisterMathAddon( engine );
	RegisterScriptArray( engine, true );
	RegisterStringAddon( engine );
	RegisterVec3Addon( engine );
	RegisterCvarAddon( engine );

	engine->SetDefaultAccessMask( 0x1 );

	return engine;
}

void qasReleaseEngine( asIScriptEngine *engine ) {
	if( !engine ) {
		return;
	}

	// release all contexts linked to this engine
	qasContextList &ctxList = contexts[engine];
	for( qasContextList::iterator it = ctxList.begin(); it != ctxList.end(); it++ ) {
		asIScriptContext *ctx = *it;
		ctx->Release();
	}
	ctxList.clear();

	qasEngineContextMap::iterator it = contexts.find( engine );
	if( it != contexts.end() ) {
		contexts.erase( it );
	}

	engine->Release();
}

static asIScriptContext *qasCreateContext( asIScriptEngine *engine ) {
	asIScriptContext *ctx;
	int error;

	if( engine == NULL ) {
		return NULL;
	}

	// always new
	ctx = engine->CreateContext();
	if( !ctx ) {
		return NULL;
	}

	// We don't want to allow the script to hang the application, e.g. with an
	// infinite loop, so we'll use the line callback function to set a timeout
	// that will abort the script after a certain time. Before executing the
	// script the timeOut variable will be set to the time when the script must
	// stop executing.

	error = ctx->SetExceptionCallback( asFUNCTION( qasExceptionCallback ), NULL, asCALL_CDECL );
	if( error < 0 ) {
		ctx->Release();
		return NULL;
	}

	qasContextList &ctxList = contexts[engine];
	ctxList.push_back( ctx );

	return ctx;
}

void qasReleaseContext( asIScriptContext *ctx ) {
	if( !ctx ) {
		return;
	}

	asIScriptEngine *engine = ctx->GetEngine();
	qasContextList &ctxList = contexts[engine];
	ctxList.remove( ctx );

	ctx->Release();
}

asIScriptContext *qasAcquireContext( asIScriptEngine *engine ) {
	if( !engine ) {
		return NULL;
	}

	// try to reuse any context linked to this engine
	qasContextList &ctxList = contexts[engine];
	for( qasContextList::iterator it = ctxList.begin(); it != ctxList.end(); it++ ) {
		asIScriptContext *ctx = *it;
		if( ctx->GetState() == asEXECUTION_FINISHED ) {
			return ctx;
		}
	}

	// if no context was available, create a new one
	return qasCreateContext( engine );
}

asIScriptContext *qasGetActiveContext() {
	return asGetActiveContext();
}

/*************************************
* Scripts
**************************************/

asIScriptModule *qasLoadScriptProject( asIScriptEngine *engine, const char *filename ) {
	DynamicString path( sys_allocator, "{}/base/progs/gametypes/{}.gt", RootDirPath(), filename );
	char * contents = ReadFileString( sys_allocator, path.c_str() );
	defer { FREE( sys_allocator, contents ); };
	if( contents == NULL ) {
		Com_Printf( "qasLoadScriptProject: Couldn't read '%s'.\n", path.c_str() );
		return NULL;
	}

	asIScriptModule * mod = engine->GetModule( GAMETYPE_SCRIPTS_MODULE_NAME, asGM_CREATE_IF_NOT_EXISTS );
	if( mod == NULL ) {
		Com_Printf( S_COLOR_RED "qasBuildGameScript: GetModule '%s' failed\n", GAMETYPE_SCRIPTS_MODULE_NAME );
		return NULL;
	}

	bool ok = false;
	defer {
		if( !ok ) {
			engine->DiscardModule( GAMETYPE_SCRIPTS_MODULE_NAME );
		}
	};

	Span< const char > cursor = MakeSpan( contents );
	while( true ) {
		Span< const char > section = ParseToken( &cursor, Parse_DontStopOnNewLine );
		if( section.ptr == NULL )
			break;

		DynamicString section_path( sys_allocator, "{}/base/progs/gametypes/{}", RootDirPath(), section );
		char * section_contents = ReadFileString( sys_allocator, section_path.c_str() );
		defer { FREE( sys_allocator, section_contents ); };
		if( section_contents == NULL ) {
			Com_Printf( "Couldn't read script section: '%s'\n", section_path.c_str() );
			return NULL;
		}

		int error = mod->AddScriptSection( section_path.c_str(), section_contents );
		if( error != 0 ) {
			Com_GGPrint( S_COLOR_RED "* Failed to add the script section {} with error {}\n", section, error );
			return NULL;
		}
	}

	int error = mod->Build();
	if( error != 0 ) {
		Com_Printf( S_COLOR_RED "* Failed to build script '%s'\n", filename );
		return NULL;
	}

	ok = true;
	return mod;
}

/*************************************
* Array tools
**************************************/

CScriptArrayInterface *qasCreateArrayCpp( unsigned int length, void *ot ) {
	return QAS_NEW( CScriptArray )( length, static_cast<asIObjectType *>( ot ) );
}

void qasReleaseArrayCpp( CScriptArrayInterface *arr ) {
	arr->Release();
}

/*************************************
* Strings
**************************************/

asstring_t *qasStringFactoryBuffer( const char *buffer, unsigned int length ) {
	return objectString_FactoryBuffer( buffer, length );
}

void qasStringRelease( asstring_t *str ) {
	objectString_Release( str );
}

asstring_t *qasStringAssignString( asstring_t *self, const char *string, unsigned int strlen ) {
	return objectString_AssignString( self, string, strlen );
}
