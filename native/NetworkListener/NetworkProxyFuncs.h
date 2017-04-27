#pragma once

#include "NetworkMonitor.h"

class NetworkProxyFuncs
{
public:
	__declspec(dllexport)  static int  StartListeners();
	__declspec(dllexport)  static int  StartListenersWithCallback(function<void(const wchar_t*)> callback);
};

