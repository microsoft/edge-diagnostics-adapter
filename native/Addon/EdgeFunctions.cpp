//
// Copyright (C) Microsoft. All rights reserved.
//

#include "EdgeFunctions.h"
#include "MessageReceiver.h"
#include "Proxy_h.h"
#include <VersionHelpers.h>
#include <comdef.h>
#include <Strsafe.h>
#include <Shellapi.h>
#include <Shlobj.h>
#include <Aclapi.h>
#include <Sddl.h>
#include <sstream>

using v8::FunctionTemplate;

bool isInitialized = false;
bool isMessageReceiverCreated = false;
Nan::Persistent<Function> messageCallbackHandle;
Nan::Persistent<Function> logCallbackHandle;
CStringA m_rootPath;
HWND m_messageReceiverHwnd;
CHandle m_hChromeProcess;

NAN_MODULE_INIT(InitAll)
{
    Nan::Set(target, Nan::New("initialize").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(initialize)).ToLocalChecked());
    Nan::Set(target, Nan::New("getEdgeInstances").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(getEdgeInstances)).ToLocalChecked());
    Nan::Set(target, Nan::New("setSecurityACLs").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(setSecurityACLs)).ToLocalChecked());
    Nan::Set(target, Nan::New("openEdge").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(openEdge)).ToLocalChecked());
    Nan::Set(target, Nan::New("killAll").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(killAll)).ToLocalChecked());
    Nan::Set(target, Nan::New("serveChromeDevTools").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(serveChromeDevTools)).ToLocalChecked());
    Nan::Set(target, Nan::New("connectTo").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(connectTo)).ToLocalChecked());
    Nan::Set(target, Nan::New("injectScriptTo").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(injectScriptTo)).ToLocalChecked());
    Nan::Set(target, Nan::New("forwardTo").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(forwardTo)).ToLocalChecked());
    Nan::Set(target, Nan::New("createNetworkProxyFor").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(createNetworkProxyFor)).ToLocalChecked());
}

NODE_MODULE(Addon, InitAll)

inline void EnsureInitialized()
{
    if (!isInitialized)
    {
        Nan::ThrowTypeError("Not initialized - you must call initialize(...) before using the adapter.");
        return;
    }
}

void Log(_In_ const char* message)
{
    Isolate* isolate = Isolate::GetCurrent();

    Local<Value> log[1] = { Nan::New<String>(message).ToLocalChecked() };
    Local<Function>::New(isolate, logCallbackHandle)->Call(Nan::GetCurrentContext()->Global(), 1, log);
}

inline void EXIT_IF_NOT_S_OK(_In_ HRESULT hr)
{
    if (hr != S_OK)
    {
        _com_error err(hr);
        CStringA error;
        error.Format("ERROR: HRESULT 0x%08x : %s", hr, err.ErrorMessage());
        Log(error.GetString());

        return;
    }
}

void SendMessageToInstance(_In_ HWND instanceHwnd, _In_ CStringA& utf8)
{
    CString message = Helpers::UTF8toUTF16(utf8);

    const size_t ucbParamsSize = sizeof(CopyDataPayload_StringMessage_Data);
    const size_t ucbStringSize = sizeof(WCHAR) * (::wcslen(message) + 1);
    const size_t ucbBufferSize = ucbParamsSize + ucbStringSize;
    std::unique_ptr<BYTE> pBuffer;
    pBuffer.reset(new BYTE[ucbBufferSize]);

    COPYDATASTRUCT copyData;
    copyData.dwData = CopyDataPayload_ProcSignature::StringMessage_Signature;
    copyData.cbData = static_cast<DWORD>(ucbBufferSize);
    copyData.lpData = pBuffer.get();

    CopyDataPayload_StringMessage_Data* pData = reinterpret_cast<CopyDataPayload_StringMessage_Data*>(pBuffer.get());
    pData->uMessageOffset = static_cast<UINT>(ucbParamsSize);

    HRESULT hr = ::StringCbCopyEx(reinterpret_cast<LPWSTR>(pBuffer.get() + pData->uMessageOffset), ucbStringSize, message, NULL, NULL, STRSAFE_IGNORE_NULLS);
    EXIT_IF_NOT_S_OK(hr);

    ::SendMessage(instanceHwnd, WM_COPYDATA, reinterpret_cast<WPARAM>(m_messageReceiverHwnd), reinterpret_cast<LPARAM>(&copyData));
}

NAN_METHOD(initialize)
{
    //::MessageBox(nullptr, L"Attach", L"Attach", 0);

    if (isInitialized)
    {
        Nan::ThrowTypeError("Already initialized - you cannot call initialize(...) more than once.");
        return;
    }

    if (info.Length() < 3 || !info[0]->IsString() || !info[1]->IsFunction() || !info[2]->IsFunction())
    {
        Nan::ThrowTypeError("Incorrect arguments - initialize(rootPath: string, onEdgeMessage: (msg: string) => void, onLogMessage: (msg: string) => void): boolean");
        return;
    }

    String::Utf8Value path(info[0]->ToString());
    m_rootPath = (char*)*path;
    messageCallbackHandle.Reset(info[1].As<Function>());
    logCallbackHandle.Reset(info[2].As<Function>());

    m_messageReceiverHwnd = nullptr;

    HRESULT hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    isInitialized = (hr == S_OK || hr == S_FALSE);

    info.GetReturnValue().Set(isInitialized);
}

NAN_METHOD(getEdgeInstances)
{
    EnsureInitialized();
    if (info.Length() > 0)
    {
        Nan::ThrowTypeError("Incorrect arguments - getEdgeInstances(): { id: string, url: string, title: string, processName: string }[]");
        return;
    }

    struct Info {
        HWND hwnd;
        CStringA title;
        CStringA url;
        CStringA processName;
    };

    vector<Info> instances;

    Helpers::EnumWindowsHelper([&](HWND hwndTop) -> BOOL
    {
        Helpers::EnumChildWindowsHelper(hwndTop, [&](HWND hwnd) -> BOOL
        {
            if (Helpers::IsWindowClass(hwnd, L"Internet Explorer_Server"))
            {
                bool isEdgeContentProcess = false;

                DWORD processId;
                ::GetWindowThreadProcessId(hwnd, &processId);

                CString actualProcessName;
                CHandle handle(::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId));
                if (handle)
                {
                    DWORD length = ::GetModuleFileNameEx(handle, nullptr, actualProcessName.GetBufferSetLength(MAX_PATH), MAX_PATH);
                    actualProcessName.ReleaseBuffer(length);
                    isEdgeContentProcess = (actualProcessName.Find(L"MicrosoftEdgeCP.exe") == actualProcessName.GetLength() - 19);

                    actualProcessName = ::PathFindFileNameW(actualProcessName);
                }

                CStringA processName = Helpers::UTF16toUTF8(actualProcessName);

                if (isEdgeContentProcess)
                {
                    CComPtr<IHTMLDocument2> spDocument;
                    HRESULT hr = Helpers::GetDocumentFromHwnd(hwnd, spDocument);
                    if (hr == S_OK)
                    {
                        CComBSTR url;
                        hr = spDocument->get_URL(&url);
                        if (hr != S_OK)
                        {
                            url = L"unknown";
                        }

                        CComBSTR title;
                        hr = spDocument->get_title(&title);
                        if (hr != S_OK)
                        {
                            title = L"";
                        }

                        Info i;
                        i.hwnd = hwnd;
                        i.url = Helpers::UTF16toUTF8(CString(url));
                        i.title = Helpers::UTF16toUTF8(CString(title));
                        i.processName = processName;
                        instances.push_back(i);
                    }
                }
            }

            return TRUE;
        });

        return TRUE;
    });

    int length = (int)instances.size();
    Local<Array> arr = Nan::New<Array>(length);
    for (int i = 0; i < length; i++)
    {
        CStringA id;
        id.Format("%p", instances[i].hwnd);

        Local<Object> obj = Nan::New<Object>();
        Nan::Set(obj, Nan::New("id").ToLocalChecked(), Nan::New<String>(id).ToLocalChecked());        
        Nan::Set(obj, Nan::New("url").ToLocalChecked(), Nan::New<String>(CStringA(instances[i].url)).ToLocalChecked());
        Nan::Set(obj, Nan::New("title").ToLocalChecked(), Nan::New<String>(CStringA(instances[i].title)).ToLocalChecked());
        Nan::Set(obj, Nan::New("processName").ToLocalChecked(), Nan::New<String>(CStringA(instances[i].processName)).ToLocalChecked());        

        Nan::Set(arr, i, obj);
    }

    info.GetReturnValue().Set(arr);
}

NAN_METHOD(setSecurityACLs)
{
    EnsureInitialized();
    if (info.Length() < 1 || !info[0]->IsString())
    {
        Nan::ThrowTypeError("Incorrect arguments - setSecurityACLs(filePath: string): boolean");
        return;
    }

    info.GetReturnValue().Set(false);

    String::Utf8Value path(info[0]->ToString());
    CStringA givenPath((char*)*path);
    CString fullPath = Helpers::UTF8toUTF16(givenPath);

    // Check to make sure that the dll has the ACLs to load in an appcontainer
    // We're doing this here as the adapter has no setup script and should be xcopy deployable/removeable
    PACL pOldDACL = NULL, pNewDACL = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    EXPLICIT_ACCESS ea;
    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;

    // The check is done on the folder and should be inherited to all objects
    DWORD dwRes = GetNamedSecurityInfo(fullPath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pOldDACL, NULL, &pSD);

    // Get the SID for "ALL APPLICATION PACAKGES" since it is localized
    PSID pAllAppPackagesSID = NULL;
    bool bResult = ConvertStringSidToSid(L"S-1-15-2-1", &pAllAppPackagesSID);

    if (bResult)
    {
        // Initialize an EXPLICIT_ACCESS structure for the new ACE.
        ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
        ea.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = (LPTSTR)pAllAppPackagesSID;

        // Create a new ACL that merges the new ACE into the existing DACL.
        dwRes = SetEntriesInAcl(1, &ea, pOldDACL, &pNewDACL);
        if (dwRes == ERROR_SUCCESS)
        {
            dwRes = SetNamedSecurityInfo(fullPath.GetBuffer(), SE_FILE_OBJECT, si, NULL, NULL, pNewDACL, NULL);
            fullPath.ReleaseBuffer();
            if (dwRes == ERROR_SUCCESS)
            {
                info.GetReturnValue().Set(true);
            }
            else if (dwRes == ERROR_ACCESS_DENIED)
			{
                stringstream msg;
				msg << "You do not have the required modify permission " << (char*)*path << " on to allow access to from Edge.";
				msg << "\n";
				msg << "You can either:";
				msg << "\n 1) Grant yourself the modify permission on the parent folder (" << (char*)*path << ").";
				msg << "\n 2) Grant the Read & Execute permissions on the parent folder (" << (char*)*path << ") to the user " << (LPTSTR)pAllAppPackagesSID << ".";
				msg << "\n 3) Run this again as an administrator.";
				msg << "\n";

                const std::string& tmp = msg.str();
                Log(tmp.c_str());
			}
            else
            {
                // The ACL was not set, this isn't fatal as it only impacts IE in EPM and Edge and the user can set it manually
                Log("ERROR: Could not set ACL to allow access to Edge.\nYou can set the ACL manually by adding Read & Execute permissions for 'All APPLICATION PACAKGES' to each dll.");
            }
        }
    }
    else
    {
        Log("ERROR: Failed to get the SID for ALL_APP_PACKAGES.");
        Log("ERROR: Win32 error code: " + GetLastError());
    }

    if (pAllAppPackagesSID != NULL)
    {
        ::LocalFree(pAllAppPackagesSID);
    }

    if (pSD != NULL)
    {
        ::LocalFree((HLOCAL)pSD);
    }
    if (pNewDACL != NULL)
    {
        ::LocalFree((HLOCAL)pNewDACL);
    }
}

NAN_METHOD(openEdge)
{
    EnsureInitialized();
    if (info.Length() < 1 || !info[0]->IsString())
    {
        Nan::ThrowTypeError("Incorrect arguments - openEdge(url: string): boolean");
        return;
    }

    info.GetReturnValue().Set(false);

    String::Utf8Value openUrl(info[0]->ToString());
    CStringA givenUrl((char*)*openUrl);

    CString url = Helpers::UTF8toUTF16(givenUrl);
    if (url.GetLength() == 0)
    {
        url = L"https://www.bing.com";
    }

    HRESULT hr = Helpers::OpenUrlInMicrosoftEdge(url);
    if (SUCCEEDED(hr)) // S_FALSE is a valid return code
    {
        info.GetReturnValue().Set(true);
    }
    else
    {
        Log("ERROR: Failed to launch Microsoft Edge");
    }
}

NAN_METHOD(killAll)
{
    EnsureInitialized();
    if (info.Length() < 1 || !info[0]->IsString())
    {
        Nan::ThrowTypeError("Incorrect arguments - killAll(exeName: string): boolean");
        return;
    }

    info.GetReturnValue().Set(false);

    String::Utf8Value exeName(info[0]->ToString());
    CStringA givenExeName((char*)*exeName);
    CString name = Helpers::UTF8toUTF16(givenExeName);

    HRESULT hr = Helpers::KillAllProcessByExe(name);
    if (hr == S_OK)
    {
        info.GetReturnValue().Set(true);
    }
    else
    {
        Log("ERROR: Failed to terminate processes");
    }
}

NAN_METHOD(serveChromeDevTools)
{
    EnsureInitialized();
    if (info.Length() < 1 || !info[0]->IsNumber())
    {
        Nan::ThrowTypeError("Incorrect arguments - serveChromeDevTools(port: number): boolean");
        return;
    }

    info.GetReturnValue().Set(false);

    int port = (info[0]->NumberValue());

    // Find the chrome install location via the registry
    CString chromePath;

    // First see if we can find where Chrome is installed from the registry. This will only succeed if Chrome is installed for all users
    CString keyPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe";
    CRegKey regKey;
    if (regKey.Open(HKEY_LOCAL_MACHINE, keyPath, KEY_READ) == ERROR_SUCCESS)
    {
        ULONG bufferSize = MAX_PATH;
        CString path;
        LRESULT result = regKey.QueryStringValue(nullptr, path.GetBufferSetLength(bufferSize), &bufferSize);
        path.ReleaseBufferSetLength(bufferSize);
        if (result == ERROR_SUCCESS)
        {
            chromePath = path;
        }
    }

    if (chromePath.GetLength() == 0)
    {
        // If Chrome is only installed for the current user, look in \AppData\Local\Google\Chrome\Application\ for Chrome.exe
        CString appPath;
        ::SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appPath.GetBuffer(MAX_PATH + 1));
        appPath.ReleaseBuffer();
        chromePath = appPath + L"\\Google\\Chrome\\Application\\chrome.exe";
    }

    // Get a temp location
    CString temp;
    Helpers::ExpandEnvironmentString(L"%Temp%", temp);

    // Set arguments for the chrome that we launch
    CString arguments;
    arguments.Format(L"about:blank --remote-debugging-port=%d --window-size=0,0 --silent-launch --no-first-run --no-default-browser-check --user-data-dir=\"%s\"", port, temp);

    // Launch the process
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);
    si.wShowWindow = SW_MINIMIZE;

    BOOL result = ::CreateProcess(
        chromePath,
        arguments.GetBuffer(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        temp,
        &si,
        &pi);
    arguments.ReleaseBuffer();

    if (result)
    {
        // Store the handles
        CHandle hThread(pi.hThread);
        m_hChromeProcess.Attach(pi.hProcess);
        DWORD waitResult = ::WaitForInputIdle(m_hChromeProcess, 30000);

        info.GetReturnValue().Set(true);
    }
    else
    {
        Log("Could not open Chrome. Please ensure that Chrome is installed.");
    }
}

NAN_METHOD(connectTo)
{
    EnsureInitialized();
    if (info.Length() < 1 || !info[0]->IsString())
    {
        Nan::ThrowTypeError("Incorrect arguments - connectTo(id: string): string");
        return;
    }

    String::Utf8Value id(info[0]->ToString());
    #pragma warning(disable: 4312) // truncation to int
    HWND hwnd = (HWND)::strtol((const char*)(*id), NULL, 16);
    #pragma warning(default: 4312)

    info.GetReturnValue().Set(Nan::Null());

    CComPtr<IHTMLDocument2> spDocument;
    HRESULT hr = Helpers::GetDocumentFromHwnd(hwnd, spDocument);
    if (hr == S_OK)
    {
        SYSTEM_INFO sys;
        ::GetNativeSystemInfo(&sys);
        bool is64BitOS = PROCESSOR_ARCHITECTURE_AMD64 == sys.wProcessorArchitecture;
        BOOL isWoWTab = FALSE;
        ::IsWow64Process(GetCurrentProcess(), &isWoWTab);
        bool is64BitTab = is64BitOS && !isWoWTab;

        CString path = Helpers::UTF8toUTF16(m_rootPath);
        path.Append(L"\\..\\..\\lib\\");
        if (is64BitTab)
        {
            path.Append(L"Proxy64.dll");
        }
        else
        {
            path.Append(L"Proxy.dll");
        }

        CComPtr<IOleWindow> spSite;
        hr = Helpers::StartDiagnosticsMode(spDocument, __uuidof(ProxySite), path, __uuidof(spSite), reinterpret_cast<void**>(&spSite.p));
        if (hr == E_ACCESSDENIED && is64BitTab && ::IsWindows8Point1OrGreater())
        {
            Log("ERROR: Access denied while attempting to connect to a 64 bit tab. The most common solution to this problem is to open an Administrator command prompt, navigate to the folder containing this adapter, and type \"icacls proxy64.dll /grant \"ALL APPLICATION PACKAGES\":(RX)\"");
        }
        else if (hr == ::HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND) && is64BitTab)
        {
            Log("ERROR: Module could not be found. Ensure Proxy64.dll exists in the out\\lib\\ folder");
        }
        else if (hr == ::HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND) && !is64BitTab)
        {
            Log("ERROR: Module could not be found. Ensure Proxy.dll exists in the out\\lib\\ folder");
        }
        else if (hr != S_OK)
        {
            EXIT_IF_NOT_S_OK(hr);
        }
        else
        {            
            // Success, return the new hwnd as an id to this instance
            HWND instanceHwnd;
            hr = spSite->GetWindow(&instanceHwnd);
            EXIT_IF_NOT_S_OK(hr);

            CStringA newId;
            newId.Format("%p", instanceHwnd);
            info.GetReturnValue().Set(Nan::New<String>(newId).ToLocalChecked());

            if (!isMessageReceiverCreated)
            {
                isMessageReceiverCreated = true;
                Isolate* isolate = Isolate::GetCurrent();
                Local<Function> progress = Local<Function>::New(isolate, messageCallbackHandle);

                MessageReceiver* pMessageReceiver = new MessageReceiver(new Nan::Callback(progress), new Nan::Callback(progress), instanceHwnd, &m_messageReceiverHwnd);
                AsyncQueueWorker(pMessageReceiver);
            }
        }
    }
}

NAN_METHOD(injectScriptTo)
{
    EnsureInitialized();
    if (info.Length() < 4 || !info[0]->IsString() || !info[1]->IsString() || !info[2]->IsString() || !info[3]->IsString())
    {
        Nan::ThrowTypeError("Incorrect arguments - injectScriptTo(instanceId: string, engine: string, filename: string, script: string): void");
        return;
    }

    String::Utf8Value edgeInstanceId(info[0]->ToString());
    #pragma warning(disable: 4312) // truncation to int
    HWND instanceHwnd = (HWND)::strtol((const char*)(*edgeInstanceId), NULL, 16);
    #pragma warning(default: 4312)

    String::Utf8Value engine(info[1]->ToString());
    String::Utf8Value filename(info[2]->ToString());
    String::Utf8Value script(info[3]->ToString());

    CStringA message;
    message.Format("inject:%s:%s:%s", (const char*)(*engine), (const char*)(*filename), (const char*)(*script));
    SendMessageToInstance(instanceHwnd, message);
}

NAN_METHOD(forwardTo)
{
    EnsureInitialized();
    if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsString())
    {
        Nan::ThrowTypeError("Incorrect arguments - forwardTo(instanceId: string, message: string): void");
        return;
    }

    String::Utf8Value edgeInstanceId(info[0]->ToString());
    #pragma warning(disable: 4312) // truncation to int
    HWND instanceHwnd = (HWND)::strtol((const char*)(*edgeInstanceId), NULL, 16);
    #pragma warning(default: 4312)

    String::Utf8Value message(info[1]->ToString());
    CStringA givenMessage((const char*)(*message));
    SendMessageToInstance(instanceHwnd, givenMessage);
}

//TODO: move this helper method to the Common library
BOOL EnumThreadWindowsHelper(_In_ DWORD threadId, _In_ const function<BOOL(HWND)>& callbackFunc)
{
    return ::EnumThreadWindows(threadId, [](HWND hwnd, LPARAM lparam) -> BOOL {
        return (*(function<BOOL(HWND)>*)lparam)(hwnd);
    }, (LPARAM)&callbackFunc);
}

NAN_METHOD(createNetworkProxyFor)
{
    EnsureInitialized();
    if (info.Length() < 1 || !info[0]->IsString())
    {
        Nan::ThrowTypeError("Incorrect arguments - createNetworkProxyFor(id: string): string");
        return;
    }

    // TODO: the handler passed as parameter identifies the Edge page to monitor. We have to pass 
    // this handler to the created proxy, maybe as parameter, so it can find the correct edge page.
    // Probably instead of the process we will nedd the process Id, in case we do not have it use GetWindowThreadProcessId()
    String::Utf8Value id(info[0]->ToString());
#pragma warning(disable: 4312) // truncation to int
    HWND hwnd = (HWND)::strtol((const char*)(*id), NULL, 16);
#pragma warning(default: 4312)

    info.GetReturnValue().Set(Nan::Null());

    TCHAR localPath[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, localPath);
    CString networkProxyPath;
    //TODO: put final relative path
    networkProxyPath.Format(L"%s\\out\\lib\\CppHttpDiagnosticProviderPoC.exe", localPath);

    LPDWORD processId;
    GetWindowThreadProcessId(hwnd, processId);    

    CString arguments;    
    arguments.Format(L"--process-id=%d", *processId);

    // Launch the process
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);
    si.wShowWindow = SW_MINIMIZE;

    BOOL result = ::CreateProcess(
        networkProxyPath,
        arguments.GetBuffer(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi);
    arguments.ReleaseBuffer();

    // give time to the process to start the windows
    Sleep(1000);

    if (result)
    {
        DWORD networkProcessId = pi.dwProcessId;
        HWND proxyHwnd = nullptr;

        EnumThreadWindowsHelper(pi.dwThreadId, [&](HWND hwndTop) -> BOOL
        {
            // TODO: to be changed the criteria when the windows is modified to be invisible
            // Currently 3 windows appear for the thread, only one is visible
            if (IsWindowVisible(hwndTop))
            {
                proxyHwnd = hwndTop;
            }
            return TRUE;
        });

        if (proxyHwnd != nullptr)
        {
            CStringA newId;
            newId.Format("%p", proxyHwnd);
            info.GetReturnValue().Set(Nan::New<String>(newId).ToLocalChecked());            
        }
        else
        {
            Log("NetworkProxy hwnd not found.");
        }        
    }
    else
    {
        Log("Could not open NetworkProxy.");        
    }        
}
