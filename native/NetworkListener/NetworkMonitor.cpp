#include "stdafx.h"
#include "NetworkMonitor.h"
#include "HttpListener.h"
#include <string>
#include <functional>

using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::System::Diagnostics;
using namespace Windows::Web::Http::Diagnostics;
using namespace Windows::Foundation::Collections;

using namespace NetworkProxyLibrary;

NetworkMonitor::NetworkMonitor(DWORD processId)
{
    _processId = processId;
}

NetworkMonitor::~NetworkMonitor()
{
    if (_httpListener != nullptr)
    {
        _httpListener->StopListening();
    }
}

int NetworkProxyLibrary::NetworkMonitor::StartListeningEdgeProcess(std::function<void(const wchar_t*)> callback)
{
    if (_httpListener != nullptr) 
    {
        _httpListener->StartListening(callback);
        return 1;
    }

    const wchar_t* edgeName = L"EdgeCP";
    ProcessDiagnosticInfo^ edgeProcessInfo = nullptr;
    IVectorView<ProcessDiagnosticInfo^>^ processes = ProcessDiagnosticInfo::GetForProcesses();

    for (ProcessDiagnosticInfo^ info : processes)
    {
        const wchar_t* name = info->ExecutableFileName->Data();

        if (info->ProcessId == _processId && std::wcsstr(name, edgeName) != nullptr)
        {
            edgeProcessInfo = info;
            break;
        }
    }

    if (edgeProcessInfo == nullptr)
    {
        String^ exceptionMessage = "Not found Edge process with id: " + _processId.ToString();
        throw ref new Exception(-1, exceptionMessage);
    }

    HttpDiagnosticProvider^ diagnosticProvider = HttpDiagnosticProvider::CreateFromProcessDiagnosticInfo(edgeProcessInfo);
    _httpListener = ref new HttpListener(diagnosticProvider, edgeProcessInfo->ProcessId);
    _httpListener->StartListening(callback);

    return 1;
}

void NetworkMonitor::StopListeningEdgeProcess() 
{
    if (_httpListener != nullptr)
    {
        _httpListener->StopListening();
    }
}