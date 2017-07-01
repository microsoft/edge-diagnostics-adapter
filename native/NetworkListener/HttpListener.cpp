#include "stdafx.h"
#include <windows.h>
#include <ppltasks.h>
#include <string>
#include <fstream>
#include <iostream>
#include <locale>
#include <codecvt>


#include "HttpListener.h"

using namespace std;
using namespace concurrency;
using namespace Windows::Data::Json;
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Diagnostics;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation;
using namespace Windows::Storage;

using namespace NetworkProxyLibrary;

HttpListener::HttpListener(HttpDiagnosticProvider^ provider, unsigned int processId)
    : _provider{ provider }
    , _processId{ processId }
    , _listenerStarted{ false }
    , _messageManager{ ref new MessageManager(processId) }
{
    _messageManager->MessageProcessed += ref new NetworkProxyLibrary::MessageProcessedEventHandler(this, &HttpListener::OnMessageProcessed);

    // commented code for printing in files the messages, as helper for development of the library
   //TCHAR localPath[MAX_PATH];
   //GetCurrentDirectory(MAX_PATH, localPath);
   //auto logsPath = localPath + std::wstring(L"\\logs");
   //// create file if already exists 
   //CreateDirectory(logsPath.c_str(), nullptr);

   //_requestSentFileName = std::wstring(L"logs\\OnRequestSent_") + std::to_wstring(processId) + std::wstring(L".txt");
   //_responseReceivedFileName = std::wstring(L"logs\\OnResponseReceived_") + std::to_wstring(processId) + std::wstring(L".txt");		
   //
   //CreateLogFile(_requestSentFileName.c_str());
   //CreateLogFile(_responseReceivedFileName.c_str());
}


HttpListener::~HttpListener()
{
    this->StopListening();
}

void HttpListener::StartListening(std::function<void(const wchar_t*)> callback)
{
    _callback = callback;
    if (_listenerStarted == false)
    {
        _provider->Start();
        _requestSentToken = _provider->RequestSent += ref new TypedEventHandler<HttpDiagnosticProvider^, HttpDiagnosticProviderRequestSentEventArgs^>(this, &HttpListener::OnRequestSent);
        _responseReceivedToken = _provider->ResponseReceived += ref new TypedEventHandler<HttpDiagnosticProvider^, HttpDiagnosticProviderResponseReceivedEventArgs^>(this, &HttpListener::OnResponseReceived);
        _requestResponseCompletedToken = _provider->RequestResponseCompleted += ref new TypedEventHandler<HttpDiagnosticProvider^, HttpDiagnosticProviderRequestResponseCompletedEventArgs^>(this, &HttpListener::OnRequestResponseCompleted);
        _listenerStarted = true;
    }
}

void NetworkProxyLibrary::HttpListener::StopListening()
{
    if (_provider != nullptr && _listenerStarted == true)
    {
        _provider->RequestSent -= _requestSentToken;
        _provider->ResponseReceived -= _responseReceivedToken;
        _provider->RequestResponseCompleted -= _requestSentToken;
        _provider->Stop();
        _listenerStarted = false;
    }
}

void HttpListener::ProcessRequest(JsonObject^ request)
{
    _messageManager->ProcessRequest(request);
}

void HttpListener::OnRequestSent(HttpDiagnosticProvider ^sender, HttpDiagnosticProviderRequestSentEventArgs ^args)
{
    _messageManager->SendToProcess(ref new Message(args));
}

void HttpListener::OnResponseReceived(HttpDiagnosticProvider ^sender, HttpDiagnosticProviderResponseReceivedEventArgs ^args)
{
    _messageManager->SendToProcess(ref new Message(args));
}

void HttpListener::OnRequestResponseCompleted(HttpDiagnosticProvider ^sender, HttpDiagnosticProviderRequestResponseCompletedEventArgs ^args)
{
}

void HttpListener::DoCallback(const wchar_t* notification)
{
    if (_callback != nullptr)
    {
        _callback(notification);
    }
}

void HttpListener::OnMessageProcessed(NetworkProxyLibrary::MessageManager ^sender, Windows::Data::Json::JsonObject ^message)
{
    // commented code for testing purposes (write to a file to see the results)
    /*if (message->GetNamedString("method") == "Network.requestWillBeSent")
    {
        fileWriteMutex.lock();
        WriteLogFile(_requestSentFileName.c_str(), message->Stringify()->Data());
        fileWriteMutex.unlock();
    }
    else
    {
        fileWriteMutex.lock();
        WriteLogFile(_responseReceivedFileName.c_str(), message->Stringify()->Data());
        fileWriteMutex.unlock();
    }    */

    DoCallback(message->Stringify()->Data());
}

// the next methods are for printing the messages as helper for the development

string HttpListener::UTF16toUTF8(const wchar_t* utf16, int &outputSize)
{
    const int bufferSize = 2000;
    char buff[bufferSize];
    int length = ::WideCharToMultiByte(CP_UTF8, 0, utf16, -1, nullptr, 0, 0, 0);

    if (length > 1 && length < bufferSize)
    {
        int size = 0;
        size = length - 1;

        ::WideCharToMultiByte(CP_UTF8, 0, utf16, -1, buff, size, 0, 0);

        outputSize = size;
    }

    string result = std::string(buff);

    return result;
}

void HttpListener::CreateLogFile(const wchar_t* fileName)
{
    HANDLE hFile = CreateFile(fileName, // open file .txt
        FILE_GENERIC_WRITE,         // open for writing
        FILE_SHARE_READ,          // allow multiple readers
        NULL,                     // no security
        CREATE_ALWAYS,              // overwrite if already exists
        FILE_ATTRIBUTE_NORMAL,    // normal file
        NULL);

    wchar_t str[] = L"Start \r\n";
    DWORD bytesWritten;

    int outputSize;
    string message = UTF16toUTF8(str, outputSize);
    WriteFile(hFile, message.c_str(), outputSize, &bytesWritten, NULL);

    CloseHandle(hFile);
}

void HttpListener::WriteLogFile(const wchar_t* fileName, const wchar_t* message)
{
    HANDLE hFile = CreateFile(fileName, // open file
        FILE_APPEND_DATA,         // open for writing
        FILE_SHARE_READ,          // allow multiple readers
        NULL,                     // no security
        OPEN_ALWAYS,              // open or create
        FILE_ATTRIBUTE_NORMAL,    // normal file
        NULL);


    DWORD bytesWritten;
    int outputSize = 0;
    string convertedMessage = UTF16toUTF8(message, outputSize);
    if (outputSize > 0)
    {
        WriteFile(hFile, convertedMessage.c_str(), outputSize, &bytesWritten, NULL);
    }

    wchar_t str[] = L"  End message \r\n";
    int outputSize2 = 0;
    string convertedMessage2 = UTF16toUTF8(str, outputSize2);
    WriteFile(hFile, convertedMessage2.c_str(), outputSize2, &bytesWritten, NULL);

    CloseHandle(hFile);
}

void HttpListener::WriteLogFile(const wchar_t* fileName, unsigned char* message, unsigned int messageLength)
{
    HANDLE hFile = CreateFile(fileName, // open file
        FILE_APPEND_DATA,         // open for writing
        FILE_SHARE_READ,          // allow multiple readers
        NULL,                     // no security
        OPEN_ALWAYS,              // open or create
        FILE_ATTRIBUTE_NORMAL,    // normal file
        NULL);


    DWORD bytesWritten;
    if (messageLength > 0)
    {
        WriteFile(hFile, message, messageLength, &bytesWritten, NULL);
    }

    wchar_t str[] = L"  End message \r\n";
    int outputSize2 = 0;
    string convertedMessage2 = UTF16toUTF8(str, outputSize2);
    WriteFile(hFile, convertedMessage2.c_str(), outputSize2, &bytesWritten, NULL);

    CloseHandle(hFile);
}
