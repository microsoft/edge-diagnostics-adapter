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


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void OnMessageReceived(const wchar_t* message);
void SendMessageToWebSocket(_In_ const wchar_t* message);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
    //Sleep(15000);
    const wstring parameter = L"--process-id=";
    wstring commandLine = GetCommandLine();
    auto valuePosition = commandLine.find(parameter.c_str());

    if (valuePosition == string::npos)
    {
        MessageBox(NULL, L"string::npos", L"info", NULL);
        throw ref new InvalidArgumentException(L"Required argument to start the application: --process-id=%processId%");
    }
        
    wstring paramValue = commandLine.substr(valuePosition + parameter.length());
    m_edgeProcessId = _wtol(paramValue.c_str()); 

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_NetworkProxy, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NetworkProxy));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
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

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NetworkProxy));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_NetworkProxy);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

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

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//TODO: remove it because it is duplicated code and include Messages.h to use original copy
#pragma pack(push, 1)
struct CopyDataPayload_StringMessage_Data
{
    UINT uMessageOffset;
};
#pragma pack(pop)

//TODO: remove it because it is duplicated code and include Messages.h to use original copy
void FreeCopyDataStructCopy(_In_ PCOPYDATASTRUCT pCopyDataStructCopy)
{
    delete[](BYTE*) pCopyDataStructCopy->lpData;
    delete pCopyDataStructCopy;
}

//TODO: remove it because it is duplicated code and include Messages.h to use original copy
PCOPYDATASTRUCT MakeCopyDataStructCopy(_In_ const PCOPYDATASTRUCT pCopyDataStruct)
{
    PCOPYDATASTRUCT const pCopyDataStructCopy = new COPYDATASTRUCT;
    pCopyDataStructCopy->dwData = pCopyDataStruct->dwData;
    pCopyDataStructCopy->cbData = pCopyDataStruct->cbData;
    pCopyDataStructCopy->lpData = new BYTE[pCopyDataStructCopy->cbData];

    ::CopyMemory(pCopyDataStructCopy->lpData, pCopyDataStruct->lpData, pCopyDataStructCopy->cbData);

    return pCopyDataStructCopy;
}

//TODO: remove it because it is duplicated code and include Messages.h to use original copy
enum CopyDataPayload_ProcSignature : ULONG_PTR
{
    StringMessage_Signature
};

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
        wstring message = wstring(lpString);

        if (message.find(L"\"method\":\"Network.enable\"") != string::npos) 
        {
            if (m_networkMonitor == nullptr)
            {
                m_networkMonitor = new NetworkMonitor(m_edgeProcessId);
            }
            m_networkMonitor->StartListeningEdgeProcess(&OnMessageReceived);
        }
        else if (message.find(L"\"method\":\"Network.disable\"") != string::npos)
        {
            m_networkMonitor->StopListeningEdgeProcess();
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
    if (hr != S_OK || FAILED(hr))
    {
        OutputDebugStringW(L"NetworkProxy::SendMessageToWebSocket-> Error copying string. \n");
        return;
    }

    ::SendMessage(m_serverHwnd, WM_COPYDATA, reinterpret_cast<WPARAM>(m_serverHwnd), reinterpret_cast<LPARAM>(&copyData));
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
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:                
                SendMessageToWebSocket(L"This is a conectivity test");                  
                /*m_networkMonitor = new NetworkMonitor(m_edgeProcessId);                
                m_networkMonitor->StartListeningEdgeProcess(&OnMessageReceived);*/
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
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

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
