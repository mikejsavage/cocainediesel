#pragma once

void InitClientPlatform();

void CreateAutoreleasePoolOnMacOS();
void ReleaseAutoreleasePoolOnMacOS();

bool OpenInWebBrowser( const char * url );
