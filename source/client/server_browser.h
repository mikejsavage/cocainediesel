#pragma once

#include "qcommon/types.h"
#include "qcommon/qcommon.h"

struct ServerBrowserEntry {
	netadr_t address;

	bool have_details;
	char name[ 128 ];
	char map[ 64 ];
	int ping;
	int num_players;
	int max_players;
};

void InitServerBrowser();
void ShutdownServerBrowser();

Span< const ServerBrowserEntry > GetServerBrowserEntries();
void RefreshServerBrowser();

void ServerBrowserFrame();

struct msg_t;
void ParseMasterServerResponse( msg_t * msg, bool allow_ipv6 );
void ParseGameServerResponse( msg_t * msg, netadr_t address );
