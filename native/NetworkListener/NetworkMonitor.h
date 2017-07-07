#pragma once

#include "HttpListener.h"
#include <functional>  

using namespace std;

namespace NetworkProxyLibrary
{
    class NetworkMonitor
    {
    public:
        NetworkMonitor(DWORD processId);
        ~NetworkMonitor();
        int StartListeningEdgeProcess(std::function<void(const wchar_t*)> callback);
        void StopListeningEdgeProcess();
        void ProcessRequest(JsonObject^ request);

    private:
        HttpListener^ _httpListener;
        DWORD _processId;
    };
}
