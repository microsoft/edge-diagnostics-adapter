//
// Copyright (C) Microsoft. All rights reserved.
//

#pragma once

#include "Proxy_h.h"
#include "WebSocketHandler.h"
#include <memory>
#include <iostream>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

class EdgeDiagnosticsAdapter :
	public CWindowImpl < EdgeDiagnosticsAdapter >
{
public:
	EdgeDiagnosticsAdapter(_In_ LPCWSTR rootPath);
	EdgeDiagnosticsAdapter(_In_ LPCWSTR rootPath, _In_ string port);
	~EdgeDiagnosticsAdapter();
	bool IsServerRunning;

	static const CStringA s_Protocol_Version;
	static const string s_Default_Port;

	BEGIN_MSG_MAP(WebSocketClient)
		MESSAGE_HANDLER(WM_COPYDATA, OnCopyData);
		MESSAGE_HANDLER(WM_PROCESSCOPYDATA, OnMessageFromIE)
		MESSAGE_HANDLER(WM_TEST_TIMEOUT, OnTestTimeout)
		MESSAGE_HANDLER(WM_TEST_START, OnTestStart)
	END_MSG_MAP()

    // Window Messages
	LRESULT OnCopyData(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
	LRESULT OnMessageFromIE(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
	LRESULT OnTestTimeout(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
	LRESULT OnTestStart(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
private:
    shared_ptr<WebSocketHandler> m_webSocketHander;
};
