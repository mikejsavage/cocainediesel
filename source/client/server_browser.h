#pragma once

#include "qcommon/types.h"

struct ServerBrowserEntry {
	netadr_t address;

	char name[ 128 ];
	char map[ 64 ];
	int ping;
	int num_players;
	int max_players;
};

void InitServerBrowser();
void ShutdownServerBrowser();

Span< ServerBrowserEntry > GetServerBrowserEntries();
void RefreshServerBrowser();

void ServerBrowserFrame();
