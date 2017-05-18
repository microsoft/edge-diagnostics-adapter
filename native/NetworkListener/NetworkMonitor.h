#pragma once

#include "HttpListener.h"
#include <functional>  

using namespace std;

namespace NetworkProxyLibrary
{
    class NetworkMonitor
    {
    public:
        NetworkMonitor();
        ~NetworkMonitor();
        // temporary methods to start processes not knowing which is the process ID for the edge pages
        int StartListeningAllEdgeProcesses(std::function<void(const wchar_t*)> callback);
        void StopListeningEdgeProcesses();

    private:
        HttpListener^ _httpListeners[10];

    };
}
