// CppHttpDiagnosticProviderPoC.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "CppHttpDiagnosticProviderPoC.h"
#include "NetworkMonitor.h";
#include <functional>
#include <memory>

#include <functional>

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

NetworkMonitor* networkMonitor;
HINSTANCE hinstDLL;
BOOL fFreeDLL;

HWND m_serverHwnd;


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void OnMessageReceived(const wchar_t* message);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CPPHTTPDIAGNOSTICPROVIDERPOC, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CPPHTTPDIAGNOSTICPROVIDERPOC));

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


void StartListeningEdge()
{
    networkMonitor = new NetworkMonitor();
    networkMonitor->StartListeningAllEdgeProcesses(&OnMessageReceived);	
}

void OnMessageReceived(const wchar_t* message)
{
	auto msg = message;
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
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CPPHTTPDIAGNOSTICPROVIDERPOC));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CPPHTTPDIAGNOSTICPROVIDERPOC);
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

void OnMessageFromWebSocket(UINT nMsg, WPARAM wParam, LPARAM lParam)
{       
    // CString message;
    m_serverHwnd = reinterpret_cast<HWND>(wParam);
    // Scope for the copied data
    {
        // Take ownership of the copydata struct memory
        //unique_ptr<COPYDATASTRUCT, void(*)(COPYDATASTRUCT*)> spParams(reinterpret_cast<PCOPYDATASTRUCT>(lParam), ::FreeCopyDataStructCopy);

        PCOPYDATASTRUCT pCopyDataStruct = reinterpret_cast<PCOPYDATASTRUCT>(lParam);
        
        // Copy the data so that we can post message to ourselves and unblock the SendMessage caller
        unique_ptr<COPYDATASTRUCT, void(*)(COPYDATASTRUCT*)> spParams(::MakeCopyDataStructCopy(pCopyDataStruct), ::FreeCopyDataStructCopy);
        
        PCOPYDATASTRUCT pParams = spParams.release();

        // Get the string message from the structure
        CopyDataPayload_StringMessage_Data* pMessage = reinterpret_cast<CopyDataPayload_StringMessage_Data*>(pParams->lpData);
        LPCWSTR lpString = reinterpret_cast<LPCWSTR>(reinterpret_cast<BYTE*>(pMessage) + pMessage->uMessageOffset);
        // message = lpString;
    }
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
                //DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
				// Windows::System::Launcher::LaunchUriAsync(ref new Windows::Foundation::Uri(L"https://blogs.windows.com/buildingapps//"));
                //::MessageBox(nullptr, L"IDM_ABOUT", L"Message", 0);
				//StartListeningEdge();
                ::SendMessage(m_serverHwnd, WM_COPYDATA, reinterpret_cast<WPARAM>(m_serverHwnd), NULL);
                //::SendMessage(m_serverHwnd, WM_PROCESSCOPYDATA, reinterpret_cast<WPARAM>(m_serverHwnd), NULL);
                
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
    case WM_PROCESSCOPYDATA:
        ::MessageBox(nullptr, L"WM_NETWORKPROXY", L"Message", 0);
        break;
    case WM_COPYDATA:
        OnMessageFromWebSocket(message, wParam, lParam);
        //::MessageBox(nullptr, L"WM_COPYDATA", L"Message", 0);

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
