#include "stdafx.h"
#include "MessageManager.h"
#include <string>
#include <fstream>
#include <iostream>
#include <locale>
#include <codecvt>

#include <collection.h>
#include <ppltasks.h>

using namespace std;
using namespace Concurrency;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Streams;
using namespace Windows::Web::Http;
using namespace Platform;
using namespace NetworkProxyLibrary;

MessageManager::MessageManager(unsigned int processId)
{
    _processId = processId;
    _currentMessageCounter = 1;    
    _requestSentDictionary = ref new Map<Guid, JsonObject^>();
    _httpMessages = ref new Vector<Message^>(); 
}


MessageManager::~MessageManager()
{
}

String^ MessageManager::GetNextSequenceId(IdTypes counterType)
{
    return _processId.ToString() + "." + _idCounters[counterType]++;
}

void InsertString(JsonObject^ json, String^ key, String^ value)
{
    json->Insert(key, JsonValue::CreateStringValue(value));
}

void InsertNumber(JsonObject^ json, String^ key, double  value)
{
    json->Insert(key, JsonValue::CreateNumberValue(value));
}

void MessageManager::SendToProcess(Message^ message) 
{
    try
    {
        OutputDebugStringW(L"Enter SendToProcess \n");
        _vectorMutex.lock();        
        _httpMessages->Append(message);        
        _vectorMutex.unlock();

        if (_httpMessages->Size == 1 || _httpMessages->Size > 2)
        {                      
            OutputDebugStringW(L"Execute from SendToProcess \n");
            this->ProcessNextMessage();                           
        }
        
        auto mes = std::wstring(L"Exit SendToProcess ") + std::to_wstring(_httpMessages->Size) + std::wstring(L"\n");

        OutputDebugStringW(mes.c_str());
    }
    catch (const std::exception& ex)
    {
        auto t = ex;
    }
}

void MessageManager::PostProcessMessage(JsonObject^ jsonObject)
{     
    OutputDebugStringW(L"Enter PostProcessMessage \n");        

    MessageProcessed(this, jsonObject);    

    OutputDebugStringW(L"Exit PostProcessMessage \n");
}

void MessageManager::ProcessNextMessage()
{
    OutputDebugStringW(L"Enter ProcessNextMessage \n");   
    Message^ message;
    try
    {
        _vectorMutex.lock();
        if (_httpMessages->Size > 0)
        {
            message = _httpMessages->GetAt(0);
            _httpMessages->RemoveAt(0);
            auto mes = std::wstring(L"ProcessNextMessage remove, total: ") + std::to_wstring(_httpMessages->Size) + std::wstring(L"\n");
            OutputDebugStringW(mes.c_str());
            _vectorMutex.unlock();
        }
        else
        {
            _vectorMutex.unlock();
            OutputDebugStringW(L"Exit ProcessNextMessage 2 \n"); 
            return;
        }
    }
    // TODO catch specific expeption, process "not found message" exception
    catch (const std::exception&)
    {  
        OutputDebugStringW(L"Exit ProcessNextMessage 3 \n"); 
        return;
    }

    switch (message->MessageType)
    {
        case MessageTypes::RequestSent:                
            ProcessRequestSentMessage(message);
            break;
        case MessageTypes::ResponseReceived:
            ProcessResponseReceivedMessage(message);            
            break;
        case MessageTypes::RequestResponseCompleted:
        default:
            break;
    }        

    OutputDebugStringW(L"Exit ProcessNextMessage \n");   
}

void MessageManager::ProcessRequestSentMessage(Message ^ message)
{
    auto eventArgs = message->RequestSentEventArgs;

    OutputDebugStringW(L"Enter ProcessRequestSentMessage \n");
    if (eventArgs->Message->Method->Method == "POST")
    {
        auto contentTask = create_task(eventArgs->Message->Content->ReadAsBufferAsync()).then([this, eventArgs](IBuffer^ content)
        {
            // read from IBuffer: http://stackoverflow.com/questions/11853838/getting-an-array-of-bytes-out-of-windowsstoragestreamsibuffer
            auto reader = ::Windows::Storage::Streams::DataReader::FromBuffer(content);
            auto payloadLenght = reader->UnconsumedBufferLength;
            String^ payload = payloadLenght > 0 ? reader->ReadString(payloadLenght) : nullptr;
            JsonObject^ seriealizedMessage = this->GenerateRequestWilBeSentMessage(eventArgs, payload);
            this->PostProcessMessage(seriealizedMessage);            
            OutputDebugStringW(L"Exit POST ProcessRequestSentMessage \n");                
        }).then([this]()
        {                
            this->ProcessNextMessage();
        });
    }
    else
    {
        try
        {
            JsonObject^ serializedMessage = this->GenerateRequestWilBeSentMessage(eventArgs);
            this->PostProcessMessage(serializedMessage);
        }
        catch (const std::exception&)
        {
            OutputDebugStringW(L"Exception calling GenerateRequestWilBeSentMessage \n");
        }
        OutputDebugStringW(L"Exit ProcessRequestSentMessage \n");
        ProcessNextMessage();
    }
}

void MessageManager::ProcessResponseReceivedMessage(Message^ message)
{   
    OutputDebugStringW(L"Enter ProcessResponseReceivedMessage \n");
   
    JsonObject^ responseReceivedMessage;    

    try
    {
        responseReceivedMessage = this->GenerateResponseReceivedMessage(message->ResponseReceivedEventArgs);
    }
    catch (const std::exception&)
    {
        OutputDebugStringW(L"Exception calling GenerateResponseReceivedMessage \n");
    }
        
    if (responseReceivedMessage == nullptr)
    {
        unsigned int pendingMessages = 0;
        if (++message->ProcessingRetries <= 3)
        {
            _vectorMutex.lock();
            _httpMessages->Append(message);
            pendingMessages = _httpMessages->Size;
            _vectorMutex.unlock();
        }
        else
        {
            auto infoMessage = std::wstring(L"WARNING: Message not processed ") + message->MessageId.ToString()->Data() + std::wstring(L"\n");
            OutputDebugStringW(infoMessage.c_str());
        }

        // it is useless to process the message again if it is the only one in the queue (no new data has been received)
        if (pendingMessages > 1)
        {
            ProcessNextMessage();
        }
        return;
    }
    else
    {
        // forced to do a task to calculate the content lenght of the message because the syncron method data->Message->Content->TryComputeLength(&contentLenght)
        // is not returning anything
        create_task(message->ResponseReceivedEventArgs->Message->Content->ReadAsBufferAsync()).then([this, responseReceivedMessage, message](IBuffer^ content)
        {
            auto reader = ::Windows::Storage::Streams::DataReader::FromBuffer(content);
            auto payloadLenght = reader->UnconsumedBufferLength;
            auto dataReceivedMessage = GenerateDataReceivedMessage(responseReceivedMessage, payloadLenght);
            this->PostProcessMessage(dataReceivedMessage);            
        });        
    }

    this->PostProcessMessage(responseReceivedMessage);
    
    OutputDebugStringW(L"Exit ProcessResponseReceivedMessage \n");
    ProcessNextMessage();
       
}

JsonObject^ SerializeHeaders(IIterator<IKeyValuePair<String^, String^>^>^ iterator)
{
    JsonObject^ result = ref new JsonObject();
    //auto iterator = message->Headers->First();

    while (iterator->HasCurrent)
    {
        auto header = iterator->Current;
        InsertString(result, header->Key, header->Value);
        iterator->MoveNext();
    }

    return result;
}

JsonObject ^ MessageManager::GenerateRequestWilBeSentMessage(HttpDiagnosticProviderRequestSentEventArgs ^data, String^ postPayload)
{
    HttpRequestMessage^ message = data->Message;
    JsonObject^ result = ref new JsonObject();    
    InsertString(result, "method", "Network.requestWillBeSent");

    JsonObject^ params = ref new JsonObject();    
    InsertString(params, "requestId", GetNextSequenceId(IdTypes::RequestId));
    InsertString(params, "frameId", GetNextSequenceId(IdTypes::FrameId));
    InsertString(params, "loaderId", GetNextSequenceId(IdTypes::LoaderId));
    InsertString(params, "documentURL", message->RequestUri->AbsoluteUri);

    JsonObject^ request = ref new JsonObject();
    InsertString(request, "url", message->RequestUri->AbsoluteUri);

    InsertString(request, "method", message->Method->Method);    
    request->Insert("headers", SerializeHeaders(message->Headers->First()));
    if (postPayload != nullptr && message->Method->Method == "POST")
    {
        InsertString(request, "postData", postPayload);
    }
    InsertString(request, "initialPriority", "");

    params->Insert("request", request);        
                
    auto timeInSecs = data->Timestamp.UniversalTime / (10000000);
    InsertNumber(params,"timestamp", timeInSecs);

    //TODO:  compose wall time, maybe not possible to calculate
    InsertNumber(params, "walltime", 0);


    String^ initiator = "{\"type\": \"" + data->Initiator.ToString() + "\"}";
    JsonValue^ initiatorValue = JsonValue::Parse(initiator);
    params->Insert("initiator", initiatorValue);
    // TODO: compose the type, remove hardcoded value
    // allowed values: Document, Stylesheet, Image, Media, Font, Script, TextTrack, XHR, Fetch, EventSource, WebSocket, Manifest, Other
    InsertString(params, "type", "Document");

    result->Insert("params", params);    
    _dictionaryMutex.lock();
    _requestSentDictionary->Insert(data->ActivityId, result);
    _dictionaryMutex.unlock();

    return result;
}

JsonObject^ MessageManager::GenerateResponseReceivedMessage(HttpDiagnosticProviderResponseReceivedEventArgs^ data)
{   
    JsonObject^ result = ref new JsonObject();
    JsonObject^ requestMessage;

    Guid id = data->ActivityId;
    _dictionaryMutex.lock();
    if (_requestSentDictionary->HasKey(id))
    {
        requestMessage = _requestSentDictionary->Lookup(id);
        _dictionaryMutex.unlock();
    }
    else
    {
        _dictionaryMutex.unlock();
        return nullptr;
    }
    
    HttpResponseMessage^ message = data->Message;
    
    InsertString(result, "method", "Network.responseReceived");

    JsonObject^ params = ref new JsonObject();
    auto sentParams = requestMessage->GetNamedObject("params");    
    InsertString(params, "requestId", sentParams->GetNamedString("requestId"));
    InsertString(params, "frameId", sentParams->GetNamedString("frameId"));
    InsertString(params, "loaderId", sentParams->GetNamedString("loaderId"));
    auto timeInSecs = data->Timestamp.UniversalTime / (10000000);
    InsertNumber(params, "timestamp", timeInSecs);
    // TODO: compose the type, remove hardcoded value
    // allowed values: Document, Stylesheet, Image, Media, Font, Script, TextTrack, XHR, Fetch, EventSource, WebSocket, Manifest, Other
    InsertString(params, "type", "Document");

    JsonObject^ response = ref new JsonObject();
    InsertString(response, "url", sentParams->GetNamedString("documentURL"));
    InsertNumber(response, "status", static_cast<int>(message->StatusCode));    
    InsertString(response, "statusText", message->ReasonPhrase);
    response->Insert("headers", SerializeHeaders(message->Headers->First()));
    String^ mimeType = message->Content->Headers->HasKey("Content-Type") ? message->Content->Headers->Lookup("Content-Type") : "";
    InsertString(response, "mimeType", mimeType);      
    response->Insert("requestHeaders", sentParams->GetNamedObject("request")->GetNamedObject("headers"));


    params->Insert("response", response);
    result->Insert("params", params);
    
    return result;
}

JsonObject^ MessageManager::GenerateDataReceivedMessage(JsonObject^ responseReceivedMessage, double contentLenght)
{
    JsonObject^ result = ref new JsonObject();
    InsertString(result, "method", "Network.dataReceived");

    JsonObject^ params = ref new JsonObject();
    InsertString(params, "requestId", responseReceivedMessage->GetNamedObject("params")->GetNamedString("requestId"));
    InsertNumber(params, "timestamp", responseReceivedMessage->GetNamedObject("params")->GetNamedNumber("timestamp"));

    InsertNumber(params, "dataLength", contentLenght);
    

    //TODO: investigate if we can have this field
    InsertNumber(params, "encodedDataLength", 0);

    result->Insert("params", params);

    return result;

}

