#pragma once

#include "qcommon/base.h"
#include "gameshared/q_shared.h"

#if !PUBLIC_BUILD

#define AT_STARTUP( code ) \
	namespace COUNTER_NAME( StartupCode ) { \
		static struct AtStartup { \
			AtStartup() { \
				code; \
			} \
		} AtStartupInstance; \
	}

#define SELFTESTS( name, body ) \
	namespace { \
		AT_STARTUP( \
			TracyZoneScoped; \
			constexpr const char * selftest_block_name = name; \
			body; \
		) \
	}

#define TEST( name, p ) \
	if( !( p ) ) { \
		Com_Printf( S_COLOR_RED "Self-test failed: %s > %s: %s", selftest_block_name, name, #p ); \
	}

#else

#define SELFTESTS( name, body )
#define TEST( p )

#endif
