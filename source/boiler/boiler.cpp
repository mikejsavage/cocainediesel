#define BOILER_EXPORT
#include "boiler.h"
#include "steam_api/steam_api.h"
extern "C" {

static bool initialized = false;

BOILER_API int boiler_init() {
	bool status = SteamAPI_Init();
	if( !status ) {
		SteamAPI_Shutdown();
		return false;
	}
	initialized = true;
	return true;
}

BOILER_API const char * boiler_get_username() {
	if( !initialized ) {
		return "CocaineDieselPlayer";
	}
	return SteamFriends()->GetPersonaName();
}

}