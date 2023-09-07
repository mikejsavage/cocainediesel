// include base.h instead of this
// scoped wrappers around the tracy C API, because it compiles faster than the C++ API

#include "tracy/tracy/TracyC.h"

extern bool tracy_is_active;

#define TracyZoneScoped TracyCZone( ___tracy_scoped_zone, tracy_is_active ? 1 : 0 ); defer { TracyCZoneEnd( ___tracy_scoped_zone ); }
#define TracyZoneScopedN( name ) TracyCZoneN( ___tracy_scoped_zone, name, tracy_is_active ? 1 : 0 ); defer { TracyCZoneEnd( ___tracy_scoped_zone ); }
#define TracyZoneText( str, len ) TracyCZoneText( ___tracy_scoped_zone, str, len )

#if TRACY_ENABLE
#define TracyPlotSample TracyCPlot
#else
#define TracyPlotSample( label, value ) ( void ) sizeof( value )
#endif
