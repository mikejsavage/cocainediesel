#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "qcommon/gitversion.h"

constexpr u32 APP_PROTOCOL_VERSION = Hash32_CT( APP_VERSION, sizeof( APP_VERSION ) );
