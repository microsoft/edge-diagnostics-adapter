// NetworkProxy.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Strsafe.h"
#include "NetworkProxy.h"
#include "NetworkMonitor.h";
#include <functional>
#include <memory>
#include <string>

using namespace std;
using namespace NetworkProxyLibrary;
using namespace Windows::Data;


/*************************************************************/
#define WM_COPYDATA 0x004A
#define WM_PROCESSCOPYDATA WM_USER + 1

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

NetworkMonitor* m_networkMonitor;
HWND m_serverHwnd;
DWORD m_edgeProcessId;
HWND m_hMainWnd;


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void OnMessageReceived(const wchar_t* message);
void SendMessageToWebSocket(_In_ const wchar_t* message);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    const wstring PIdParam = L"--process-id=";
    wstring commandLine = GetCommandLine();
    auto startPIdParam = commandLine.find(PIdParam.c_str());

    const wstring launchParam = L"--startlistener";
    bool isAutoLaunchActive = commandLine.find(launchParam.c_str()) != string::npos;

    if (startPIdParam == string::npos)
    {
        throw ref new InvalidArgumentException(L"Required argument to start the application: --process-id=%processId%");
    }

    auto endPIdParam = commandLine.find(L"-", startPIdParam + PIdParam.length());
    wstring paramValue;
    if (endPIdParam == string::npos)
    {
        paramValue = commandLine.substr(startPIdParam + PIdParam.length());
    }
    else
    {
        auto length = endPIdParam - (startPIdParam + PIdParam.length());
        paramValue = commandLine.substr(startPIdParam + PIdParam.length(), length);
    }
    m_edgeProcessId = _wtol(paramValue.c_str());

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_NetworkProxy, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NetworkProxy));

    ShowWindow(m_hMainWnd, SW_HIDE);

    if (isAutoLaunchActive)
    {
        m_networkMonitor = new NetworkMonitor(m_edgeProcessId);
        m_networkMonitor->StartListeningEdgeProcess(&OnMessageReceived);
    }

    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

void OnMessageReceived(const wchar_t* message)
{
    SendMessageToWebSocket(message);
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NetworkProxy));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_NetworkProxy);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable   

    HWND hWnd = CreateWindowW(szWindowClass, nullptr, 0,
        0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    m_hMainWnd = hWnd;

    return TRUE;
}

#pragma pack(push, 1)
struct CopyDataPayload_StringMessage_Data
{
    UINT uMessageOffset;
};
#pragma pack(pop)

void FreeCopyDataStructCopy(_In_ PCOPYDATASTRUCT pCopyDataStructCopy)
{
    delete[](BYTE*) pCopyDataStructCopy->lpData;
    delete pCopyDataStructCopy;
}

PCOPYDATASTRUCT MakeCopyDataStructCopy(_In_ const PCOPYDATASTRUCT pCopyDataStruct)
{
    PCOPYDATASTRUCT const pCopyDataStructCopy = new COPYDATASTRUCT;
    pCopyDataStructCopy->dwData = pCopyDataStruct->dwData;
    pCopyDataStructCopy->cbData = pCopyDataStruct->cbData;
    pCopyDataStructCopy->lpData = new BYTE[pCopyDataStructCopy->cbData];

    ::CopyMemory(pCopyDataStructCopy->lpData, pCopyDataStruct->lpData, pCopyDataStructCopy->cbData);

    return pCopyDataStructCopy;
}

enum CopyDataPayload_ProcSignature : ULONG_PTR
{
    StringMessage_Signature
};

void SendEmptyResponse(int responseId)
{
    JsonObject^ response = ref new JsonObject();
    response->Insert("id", JsonValue::CreateNumberValue(responseId));
    response->Insert("result", ref new JsonObject());
    SendMessageToWebSocket(response->ToString()->Data());
}

void OnMessageFromWebSocket(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    m_serverHwnd = reinterpret_cast<HWND>(wParam);
    // Scope for the copied data
    {
        PCOPYDATASTRUCT pCopyDataStruct = reinterpret_cast<PCOPYDATASTRUCT>(lParam);

        // Copy the data so that we can handle the message and unblock the SendMessage caller
        unique_ptr<COPYDATASTRUCT, void(*)(COPYDATASTRUCT*)> spParams(::MakeCopyDataStructCopy(pCopyDataStruct), ::FreeCopyDataStructCopy);

        PCOPYDATASTRUCT pParams = spParams.release();

        // Get the string message from the structure
        CopyDataPayload_StringMessage_Data* pMessage = reinterpret_cast<CopyDataPayload_StringMessage_Data*>(pParams->lpData);
        LPCWSTR lpString = reinterpret_cast<LPCWSTR>(reinterpret_cast<BYTE*>(pMessage) + pMessage->uMessageOffset);

        String^ message = ref new String(lpString);
        JsonObject^ jsonMessage;
        bool messageParsed = JsonObject::TryParse(message, &jsonMessage);

        if (messageParsed)
        {
            int id = (int)jsonMessage->GetNamedNumber("id", 0);
            auto method = jsonMessage->GetNamedString("method", "");

            if (method == "Network.enable")
            {
                if (m_networkMonitor == nullptr)
                {
                    m_networkMonitor = new NetworkMonitor(m_edgeProcessId);
                }
                m_networkMonitor->StartListeningEdgeProcess(&OnMessageReceived);
                SendEmptyResponse(id);
            }
            else if (method == "Network.disable")
            {
                m_networkMonitor->StopListeningEdgeProcess();
                SendEmptyResponse(id);
            }
            else
            {
                m_networkMonitor->ProcessRequest(jsonMessage);
            }            
        }
    }
}

void SendMessageToWebSocket(_In_ const wchar_t* message)
{
    if (m_serverHwnd == nullptr)
    {
        OutputDebugStringW(L"NetworkProxy::SendMessageToWebSocket-> Pointer to the wecksocket window is null. \n");
        return;
    }

    const size_t ucbParamsSize = sizeof(CopyDataPayload_StringMessage_Data);
    const size_t ucbStringSize = sizeof(WCHAR) * (::wcslen(message) + 1);
    const size_t ucbBufferSize = ucbParamsSize + ucbStringSize;
    std::unique_ptr<BYTE> pBuffer;
    try
    {
        pBuffer.reset(new BYTE[ucbBufferSize]);
    }
    catch (std::bad_alloc&)
    {
        OutputDebugStringW(L"NetworkProxy::SendMessageToWebSocket-> Out of memory exception. \n");
        return;
    }

    COPYDATASTRUCT copyData;
    copyData.dwData = CopyDataPayload_ProcSignature::StringMessage_Signature;
    copyData.cbData = static_cast<DWORD>(ucbBufferSize);
    copyData.lpData = pBuffer.get();

    CopyDataPayload_StringMessage_Data* pData = reinterpret_cast<CopyDataPayload_StringMessage_Data*>(pBuffer.get());
    pData->uMessageOffset = static_cast<UINT>(ucbParamsSize);

    HRESULT hr = ::StringCbCopyEx(reinterpret_cast<LPWSTR>(pBuffer.get() + pData->uMessageOffset), ucbStringSize, message, NULL, NULL, STRSAFE_IGNORE_NULLS);
    if (hr != S_OK)
    {
        OutputDebugStringW(L"NetworkProxy::SendMessageToWebSocket-> Error copying string. \n");
        return;
    }

    ::SendMessage(m_serverHwnd, WM_COPYDATA, reinterpret_cast<WPARAM>(m_hMainWnd), reinterpret_cast<LPARAM>(&copyData));
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
    }
    break;
    case WM_PAINT:
    {
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_COPYDATA:
        OnMessageFromWebSocket(message, wParam, lParam);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

