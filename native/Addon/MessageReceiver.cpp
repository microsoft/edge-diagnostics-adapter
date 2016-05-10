//
// Copyright (C) Microsoft. All rights reserved.
//

#include "MessageReceiver.h"

MessageReceiver::MessageReceiver(_In_ Nan::Callback* pCallback, _In_ Nan::Callback* pProgressCallback, _In_  HWND proxyHwnd, _In_ HWND* pReceiverHwnd)
  : Nan::AsyncProgressWorker(pCallback),
    m_pProgressCallback(pProgressCallback),
    m_proxyHwnd(proxyHwnd),
    m_pReceiverHwnd(pReceiverHwnd),
    m_pExecutionProgress(nullptr)
{
    uv_mutex_init(&m_lock);
}

MessageReceiver::~MessageReceiver()
{
    if (::IsWindow(m_hWnd))
    {
        this->DestroyWindow();
    }
}

// Window Messages
LRESULT MessageReceiver::OnCopyData(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/)
{
    PCOPYDATASTRUCT pCopyDataStruct = reinterpret_cast<PCOPYDATASTRUCT>(lParam);

    // Copy the data so that we can post message to ourselves and unblock the SendMessage caller
    unique_ptr<COPYDATASTRUCT, void(*)(COPYDATASTRUCT*)> spParams(::MakeCopyDataStructCopy(pCopyDataStruct), ::FreeCopyDataStructCopy);

    PCOPYDATASTRUCT pParams = spParams.release();
    BOOL succeeded = this->PostMessageW(WM_PROCESSCOPYDATA, wParam, reinterpret_cast<LPARAM>(pParams));
    if (!succeeded)
    {
        // Take ownership if the post message fails so that we can correctly clean up the memory
        HRESULT hr = ::AtlHresultFromLastError();
        spParams.reset(pParams);
        FAIL_IF_NOT_S_OK(hr);
    }

    return 0;
}

LRESULT MessageReceiver::OnMessageFromEdge(UINT nMsg, WPARAM wParam, LPARAM lParam, _Inout_ BOOL& /*bHandled*/)
{
    CString message;
    // Scope for the copied data
    {
        // Take ownership of the copydata struct memory
        unique_ptr<COPYDATASTRUCT, void(*)(COPYDATASTRUCT*)> spParams(reinterpret_cast<PCOPYDATASTRUCT>(lParam), ::FreeCopyDataStructCopy);

        // Get the string message from the structure
        CopyDataPayload_StringMessage_Data* pMessage = reinterpret_cast<CopyDataPayload_StringMessage_Data*>(spParams->lpData);
        LPCWSTR lpString = reinterpret_cast<LPCWSTR>(reinterpret_cast<BYTE*>(pMessage)+pMessage->uMessageOffset);
        message = lpString;
    }

    HWND proxyHwnd = reinterpret_cast<HWND>(wParam);

    string utf8;
    int length = message.GetLength();
    LPWSTR buffer = message.GetBuffer();

    // Convert the message into valid UTF-8 text
    int utf8Length = ::WideCharToMultiByte(CP_UTF8, 0, buffer, length, nullptr, 0, nullptr, nullptr);
    if (utf8Length == 0)
    {
        message.ReleaseBuffer();
        ATLENSURE_RETURN_HR(false, ::GetLastError());
    }

    utf8.resize(utf8Length);
    utf8Length = ::WideCharToMultiByte(CP_UTF8, 0, buffer, length, &utf8[0], static_cast<int>(utf8.length()), nullptr, nullptr);
    message.ReleaseBuffer();
    ATLENSURE_RETURN_HR(utf8Length > 0, ::GetLastError());

    // Now that we have parsed out the arguments, tell the javascript about it
    if (m_pExecutionProgress != nullptr)
    {
        uv_mutex_lock(&m_lock);
        MessageInfo info;
        info.hwndFrom = proxyHwnd;
        info.message = utf8;
        m_messages.push_back(info);
        uv_mutex_unlock(&m_lock);

        m_pExecutionProgress->Send(nullptr, 0);
    }
    return 0;
}

// AsyncProgressWorker
void MessageReceiver::Execute(const ExecutionProgress& progress)
{
    m_pExecutionProgress = &progress;

    // Initialize COM and uninitialize when it goes out of scope
    HRESULT hrCoInit = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::shared_ptr<HRESULT> spCoInit(&hrCoInit, [](const HRESULT* hrCom) { ::CoUninitialize(); hrCom; });

    // Create our message window used to handle window messages
    HWND createdWindow = this->Create(HWND_MESSAGE);
    if (createdWindow != nullptr)
    {
        // Allow messages from the proxy
        ::ChangeWindowMessageFilterEx(m_hWnd, WM_COPYDATA, MSGFLT_ALLOW, 0);
        ::ChangeWindowMessageFilterEx(m_hWnd, Get_WM_SET_CONNECTION_HWND(), MSGFLT_ALLOW, 0);
    }

    (*m_pReceiverHwnd) = m_hWnd;

    // Send our hwnd to the proxy so it can connect back
    BOOL succeeded = ::PostMessage(m_proxyHwnd, Get_WM_SET_CONNECTION_HWND(), reinterpret_cast<WPARAM>(m_hWnd), NULL);
    ATLENSURE_RETURN_VAL(succeeded, );

    // Create a message pump
    MSG msg;
    ::PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    // Thread message loop
    BOOL getMessageRet;
    while ((getMessageRet = ::GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (getMessageRet != -1)
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }
}

void MessageReceiver::HandleProgressCallback(const char* data, size_t size)
{
    Nan::HandleScope scope;

    vector<MessageInfo> messages;
    uv_mutex_lock(&m_lock);
    messages.assign(m_messages.begin(), m_messages.end());
    m_messages.clear();
    uv_mutex_unlock(&m_lock);

    for (auto& info : messages)
    {
        CStringA id;
        id.Format("%p", info.hwndFrom);

        Local<Value> argv[] = {
            Nan::New<String>(id).ToLocalChecked(),
            Nan::New<String>(info.message).ToLocalChecked()
        };
        m_pProgressCallback->Call(2, argv);
    }
}

void MessageReceiver::Destroy()
{
    ::PostQuitMessage(0);
}
