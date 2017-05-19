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
        int StartListeningEdgeProcess(DWORD processId , std::function<void(const wchar_t*)> callback);
        void StopListeningEdgeProcess();

    private:
        HttpListener^ _httpListener;
    };
}
