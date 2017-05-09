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

HttpListener::HttpListener(HttpDiagnosticProvider^ provider, unsigned int processId)
{
	_provider = provider;
	_processId = processId;
    _messageManager = new MessageManager(processId);


	TCHAR localPath[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, localPath);
	auto logsPath = localPath + std::wstring(L"\\logs");
	// create file if already exists 
	CreateDirectory(logsPath.c_str(), nullptr);

	_requestSentFileName = std::wstring(L"logs\\OnRequestSent_") + std::to_wstring(processId) + std::wstring(L".txt");
	_responseReceivedFileName = std::wstring(L"logs\\OnResponseReceived_") + std::to_wstring(processId) + std::wstring(L".txt");		
	
	CreateLogFile(_requestSentFileName.c_str());
	CreateLogFile(_responseReceivedFileName.c_str());
}


HttpListener::~HttpListener()
{
	if (_provider != nullptr)
	{		
		// TODO: verify that the unsubscribe works correctly and if so extend to the other events
		_provider->RequestSent -= _requestSent_token;
		// _provider->ResponseReceived += ref new TypedEventHandler<HttpDiagnosticProvider ^, HttpDiagnosticProviderResponseReceivedEventArgs ^>(&OnResponseReceived);
		// _provider->RequestResponseCompleted += ref new TypedEventHandler<HttpDiagnosticProvider ^, HttpDiagnosticProviderRequestResponseCompletedEventArgs ^>(&OnRequestResponseCompleted);	
		_provider->Stop();
	}
}

void HttpListener::StartListening(std::function<void(const wchar_t*)> callback)
{
	_callback = callback;    
	_provider->Start();
	EventRegistrationToken _requestSent_token = _provider->RequestSent += ref new TypedEventHandler<HttpDiagnosticProvider^, HttpDiagnosticProviderRequestSentEventArgs^>(this, &HttpListener::OnRequestSent);
	_provider->ResponseReceived += ref new TypedEventHandler<HttpDiagnosticProvider^, HttpDiagnosticProviderResponseReceivedEventArgs^>(this, &HttpListener::OnResponseReceived);
	_provider->RequestResponseCompleted += ref new TypedEventHandler<HttpDiagnosticProvider^, HttpDiagnosticProviderRequestResponseCompletedEventArgs^>(this, &HttpListener::OnRequestResponseCompleted);
}

void HttpListener::OnRequestSent(HttpDiagnosticProvider ^sender, HttpDiagnosticProviderRequestSentEventArgs ^args)
{
 	OutputDebugStringW(L"OnRequestSent");   
           
    // Forced to user ReadAsBufferAsync because data->Message->Content->ReadAsStringAsync() does not work
    // Forced to use .then because .wait and .get do not work (aplication goes to unstable state)
    IAsyncOperationWithProgress<IBuffer^, unsigned long long>^ readOp = args->Message->Content->ReadAsBufferAsync();
    
    auto contentTask = create_task(readOp).then([this, args](IBuffer^ content)
    {
        // read from IBuffer: http://stackoverflow.com/questions/11853838/getting-an-array-of-bytes-out-of-windowsstoragestreamsibuffer
        auto reader = ::Windows::Storage::Streams::DataReader::FromBuffer(content);
        auto messageLenght = reader->UnconsumedBufferLength;
            
        String^ payloadString = messageLenght > 0 ? reader->ReadString(messageLenght) : nullptr;            
        JsonObject^ serializedMessage = _messageManager->GenerateRequestWilBeSendMessage(args, payloadString);
        WriteLogFile(_requestSentFileName.c_str(), serializedMessage->Stringify()->Data());            
        auto notification = wstring(L"OnRequestSent::Process Id: ") + to_wstring(_processId) + wstring(L" AbsoluteUri: ") + wstring(args->Message->RequestUri->AbsoluteUri->Data());
        DoCallback(notification.data());
    });    
}

void HttpListener::OnResponseReceived(HttpDiagnosticProvider ^sender, HttpDiagnosticProviderResponseReceivedEventArgs ^args)
{
	OutputDebugStringW(L"OnResponseReceived");	 

	// Content->ReadAsStringAsync() seems to fail as for the C# project 
 	// IAsyncOperationWithProgress<Platform::String^, unsigned long long>^ readOp = args->Message->Content->ReadAsStringAsync();
	IAsyncOperationWithProgress<IBuffer^, unsigned long long>^ readOp = args->Message->Content->ReadAsBufferAsync();	
	create_task(readOp).then([this](IBuffer^ content)
	{
		// read from IBuffer: http://stackoverflow.com/questions/11853838/getting-an-array-of-bytes-out-of-windowsstoragestreamsibuffer
		auto reader = ::Windows::Storage::Streams::DataReader::FromBuffer(content);
		
		auto messageLenght = reader->UnconsumedBufferLength;

		std::vector<unsigned char> data(messageLenght);

		if (!data.empty())
			reader->ReadBytes(
				::Platform::ArrayReference<unsigned char>(
					&data[0], data.size()));		
		
		WriteLogFile(_responseReceivedFileName.data(), data.data(), messageLenght);

		/*std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::string narrow = converter.to_bytes(wide_utf16_source_string);
		std::wstring wide = converter.from_bytes(narrow_utf8_source_string);
		auto notification = wstring(L"OnResponseReceived::Process Id: ") + to_wstring(_processId) + wstring(L" AbsoluteUri: ") + wstring(data.data());
		DoCallback(notification.data());*/
	});
}

void HttpListener::OnRequestResponseCompleted(HttpDiagnosticProvider ^sender, HttpDiagnosticProviderRequestResponseCompletedEventArgs ^args)
{
	OutputDebugStringW(L"OnRequestResponseCompleted");
}

void HttpListener::DoCallback(const wchar_t* notification)
{
	if (_callback != nullptr)
	{
		_callback(notification);
	}
}

char* HttpListener::UTF16toUTF8(const wchar_t* utf16, int &outputSize)
{		
	// TODO modify this buff size, maybe use String to make it of variable size
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
	
	return buff;
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
	char *message = UTF16toUTF8(str, outputSize);
	WriteFile(hFile, message, outputSize, &bytesWritten, NULL);

	CloseHandle(hFile);
}

void HttpListener::WriteLogFile(const wchar_t* fileName, const wchar_t* message)
{
	OutputDebugStringW(L"OnRequestSent");

	HANDLE hFile = CreateFile(fileName, // open file
		FILE_APPEND_DATA,         // open for writing
		FILE_SHARE_READ,          // allow multiple readers
		NULL,                     // no security
		OPEN_ALWAYS,              // open or create
		FILE_ATTRIBUTE_NORMAL,    // normal file
		NULL);


	DWORD bytesWritten;
	int outputSize = 0;
	char *convertedMessage = UTF16toUTF8(message, outputSize);
	if (outputSize > 0)
	{
		WriteFile(hFile, convertedMessage, outputSize, &bytesWritten, NULL);
	}

	wchar_t str[] = L"  End message \r\n";
	int outputSize2 = 0;
	char *convertedMessage2 = UTF16toUTF8(str, outputSize2);
	WriteFile(hFile, convertedMessage2, outputSize2, &bytesWritten, NULL);

	CloseHandle(hFile);
}

void HttpListener::WriteLogFile(const wchar_t* fileName, unsigned char* message, unsigned int messageLength)
{
	OutputDebugStringW(L"OnRequestSent");

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
	char *convertedMessage2 = UTF16toUTF8(str, outputSize2);
	WriteFile(hFile, convertedMessage2, outputSize2, &bytesWritten, NULL);

	CloseHandle(hFile);
}

