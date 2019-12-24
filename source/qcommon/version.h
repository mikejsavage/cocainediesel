#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "gitversion.h"

constexpr int APP_PROTOCOL_VERSION = int( Hash32_CT( APP_VERSION, sizeof( APP_VERSION ) ) % S32_MAX );
