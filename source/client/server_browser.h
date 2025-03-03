#pragma once

#include "qcommon/types.h"
#include "qcommon/net.h"

struct ServerBrowserEntry {
	NetAddress address;
	u64 id;
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
void ParseGameServerResponse( msg_t * msg, const NetAddress & address );
