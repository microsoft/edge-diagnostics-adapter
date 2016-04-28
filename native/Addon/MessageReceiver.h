//
// Copyright (C) Microsoft. All rights reserved.
//

#pragma once

#include "stdafx.h"

class MessageReceiver :
	public Nan::AsyncProgressWorker,
	public CWindowImpl<MessageReceiver>
{
public:
    MessageReceiver(_In_ Nan::Callback* pCallback, _In_ Nan::Callback* pProgressCallback, _In_ HWND proxyHwnd, _In_ HWND* pReceiverHwnd);
    ~MessageReceiver();
    
    BEGIN_MSG_MAP(MessageReceiver)
		MESSAGE_HANDLER(WM_COPYDATA, OnCopyData)
		MESSAGE_HANDLER(WM_PROCESSCOPYDATA, OnMessageFromEdge)
	END_MSG_MAP()
    
    // Window Messages
	LRESULT OnCopyData(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
	LRESULT OnMessageFromEdge(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/);
    
	// AsyncProgressWorker  
	void Execute(const ExecutionProgress& progress);
	void HandleProgressCallback(const char* data, size_t size);
	void Destroy();
	
private:
	struct MessageInfo {
		HWND hwndFrom;
		string message;
	};
	
    HWND m_proxyHwnd;
    HWND* m_pReceiverHwnd;
    Nan::Callback* m_pProgressCallback;
	const AsyncProgressWorker::ExecutionProgress* m_pExecutionProgress;
};
