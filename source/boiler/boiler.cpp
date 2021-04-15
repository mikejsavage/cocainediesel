#define BOILER_EXPORT
#include "boiler.h"
#include "steam_api/steam_api.h"
extern "C" {

void BOILER_API boiler_init() {
	bool status = SteamAPI_Init();
}

}