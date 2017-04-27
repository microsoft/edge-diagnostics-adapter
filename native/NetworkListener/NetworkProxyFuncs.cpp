#include "stdafx.h"
#include "NetworkProxyFuncs.h"
#include "NetworkMonitor.h"
static NetworkMonitor* _networkMonitor;
int NetworkProxyFuncs::StartListeners()
{
	_networkMonitor = new NetworkMonitor();
	_networkMonitor->StartListeningAllEdgeProcesses(nullptr);	
	return 1;
}

int NetworkProxyFuncs::StartListenersWithCallback(function<void(const wchar_t*)> callback)
{
	_networkMonitor = new NetworkMonitor();
	_networkMonitor->StartListeningAllEdgeProcesses(callback);
	return 1;
}
