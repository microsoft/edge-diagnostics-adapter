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

NetworkMonitor::NetworkMonitor()
{
}

NetworkMonitor::~NetworkMonitor()
{
}

int NetworkMonitor::StartListeningAllEdgeProcesses(std::function<void(const wchar_t*)> callback)
{
	ProcessDiagnosticInfo^ edgeProcesses[10];
	IVectorView<ProcessDiagnosticInfo^>^ processes = ProcessDiagnosticInfo::GetForProcesses();
	auto nProcesses = processes->Size;
	auto count = 0;
	for (size_t i = 0; i < nProcesses; i++)
	{
		ProcessDiagnosticInfo^ info = processes->GetAt(i);
		const wchar_t* name = info->ExecutableFileName->Data();
		const wchar_t* edgeName = L"EdgeCP";
		if (std::wcsstr(name, edgeName) != nullptr)
		{
			edgeProcesses[count] = info;
			count++;
		}
	}

	auto processCounter = 0;

	while (edgeProcesses[processCounter] != nullptr)
	{
		try
		{
			auto processInfo = edgeProcesses[processCounter];
			HttpDiagnosticProvider^ diagnosticProvider = HttpDiagnosticProvider::CreateFromProcessDiagnosticInfo(processInfo);
			_httpListeners[processCounter] = ref new HttpListener(diagnosticProvider, processInfo->ProcessId);
			processCounter++;
		}
		catch (const std::exception& ex)
		{
			auto t = ex;
		}
	}

	processCounter = 0;
	while (edgeProcesses[processCounter] != nullptr)
	{
		_httpListeners[processCounter]->StartListening(callback);
		processCounter++;
	}

	return processCounter;
}