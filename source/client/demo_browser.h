#pragma once

#include "qcommon/types.h"

struct DemoBrowserEntry {
	char * path;

	bool have_details;
	char server[ 64 ];
	char map[ 64 ];
	char date[ 32 ];
	char version[ 32 ];
};

void InitDemoBrowser();
void ShutdownDemoBrowser();

Span< const DemoBrowserEntry > GetDemoBrowserEntries();
void DemoBrowserFrame();
void RefreshDemoBrowser();
