#pragma once

#include <Winsock2.h> 
#include <Windows.h> 
#include <Ws2spi.h> 
#include <iostream>
#include "Iphlpapi.h"
#include <Sporder.h>      // ������WSCWriteProviderOrder���� 

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Rpcrt4.lib")  // ʵ����UuidCreate���� 

BOOL InstallLayeredServiceProvider(WCHAR *pwszPathName);
BOOL UninstallLayeredServiceProvider();