#pragma once
#include <string>
#include <functional>

using namespace std;

using namespace Windows::Web::Http::Diagnostics;
using namespace Windows::Foundation;

// ref sealed to define the class as C++/CX https://docs.microsoft.com/en-us/cpp/cppcx/ref-classes-and-structs-c-cx
// we need a CX class to use the handlers with TypedEventHandler template https://docs.microsoft.com/en-us/cpp/cppcx/delegates-c-cx
ref class HttpListener sealed
{

public:
	HttpListener(Windows::Web::Http::Diagnostics::HttpDiagnosticProvider^ provider, unsigned int processId);	

// internal because Non-RT types are not allowed in public signature https://voidnish.wordpress.com/2012/04/11/visual-c-winrt-faq-non-rt-types-in-public-signature/  
internal:
	void StartListening(function<void(const wchar_t*)> callback);

private:
	~HttpListener();
	HttpDiagnosticProvider^ _provider;	
	std::wstring _requestSentFileName;
	std::wstring _responseReceivedFileName;
	EventRegistrationToken _requestSent_token;
	std::function<void(const wchar_t*)> _callback;
	unsigned int _processId;

private:
	//HANDLE hFile;	
	char* UTF16toUTF8(const wchar_t* utf16, int &outputSize);
	void CreateLogFile(const wchar_t* fileName);
	void WriteLogFile(const wchar_t* fileName, const wchar_t* message);
	void WriteLogFile(const wchar_t* fileName, unsigned char* message, unsigned int messageLength);
	void OnRequestSent(HttpDiagnosticProvider^ sender, HttpDiagnosticProviderRequestSentEventArgs^ args);
	void OnResponseReceived(HttpDiagnosticProvider^ sender,HttpDiagnosticProviderResponseReceivedEventArgs^ args);
	void OnRequestResponseCompleted(HttpDiagnosticProvider^ sender, HttpDiagnosticProviderRequestResponseCompletedEventArgs^ args);
	void DoCallback(const wchar_t* notification);
};

