// include base.h instead of this
// scoped wrappers around the tracy C API, because it compiles faster than the C++ API

#include "tracy/TracyC.h"

#define TracyZoneScoped TracyCZone( ___tracy_scoped_zone, 1 ); defer { TracyCZoneEnd( ___tracy_scoped_zone ); }
#define TracyZoneScopedN( name ) TracyCZoneN( ___tracy_scoped_zone, name, 1 ); defer { TracyCZoneEnd( ___tracy_scoped_zone ); }
#define TracyZoneText( str, len ) TracyCZoneText( ___tracy_scoped_zone, str, len )
