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
    _requestSentDictionary->MapChanged += ref new MapChangedEventHandler<Guid,JsonObject ^>(this, &MessageManager::OnMapChanged);
    _retryQueue = ref new Vector<Message^>();
}

MessageManager::~MessageManager()
{
}

String^ MessageManager::GetNextSequenceId(IdTypes counterType)
{
    // TODO: restart counter when high number is achieved
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
        this->ProcessMessage(message);                                          
        OutputDebugStringW(L"Exit SendToProcess \n");
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

void MessageManager::ProcessMessage(Message^ message)
{
    OutputDebugStringW(L"Enter ProcessNextMessage \n");           

    switch (message->MessageType)
    {
        case MessageTypes::RequestSent:                
            ProcessRequestSentMessage(message);
            break;
        case MessageTypes::ResponseReceived:
            ProcessResponseReceivedMessage(message);            
            break;
        case MessageTypes::RequestResponseCompleted:
            ProcessRequestResponseCompletedMessage(message);
        default:
            break;
    }        

    OutputDebugStringW(L"Exit ProcessNextMessage \n");   
}

// Edge RequestSent message is mapped to Chrome: 
// - Network.requestWillBeSent
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
    }
}

// Edge ResponseReceived message is mapped to Chrome:
// - Network.responseReceived
// - Network.dataReceived
void MessageManager::ProcessResponseReceivedMessage(Message^ message)
{   
    OutputDebugStringW(L"Enter ProcessResponseReceivedMessage \n");
   
    JsonObject^ responseReceivedMessage;    
    try
    {
        responseReceivedMessage = this->GenerateResponseReceivedMessage(message->ResponseReceivedEventArgs);        
    }
    catch (const std::exception& x)
    {        
        OutputDebugStringW(L"Exception calling GenerateResponseReceivedMessage \n");
    }
        
    if (responseReceivedMessage == nullptr)
    {
        AddMessageToQueueForRetry(message);        
    }
    else
    {        
        this->PostProcessMessage(responseReceivedMessage);
        // forced to do a task to calculate the content lenght of the message because the synchronous method data->Message->Content->TryComputeLength(&contentLenght)
        // is not returning anything
        create_task(message->ResponseReceivedEventArgs->Message->Content->ReadAsBufferAsync()).then([this, responseReceivedMessage, message](IBuffer^ content)
        {
            auto reader = ::Windows::Storage::Streams::DataReader::FromBuffer(content);
            auto payloadLenght = reader->UnconsumedBufferLength;
            auto dataReceivedMessage = GenerateDataReceivedMessage(responseReceivedMessage, payloadLenght);
            this->PostProcessMessage(dataReceivedMessage);
        });        
    }          
    OutputDebugStringW(L"Exit ProcessResponseReceivedMessage \n");        
}

// Edge RequestResponseComplete message is mapped to Chrome:
// - Network.loadingFinished
void MessageManager::ProcessRequestResponseCompletedMessage(Message ^ message)
{
    OutputDebugStringW(L"Enter ProcessRequestResponseCompletedMessage \n");
    JsonObject^ requestMessage = GetRequestMessage(message->RequestResponseCompletedEventArgs->ActivityId);    
    if (requestMessage == nullptr)
    {
        AddMessageToQueueForRetry(message);
        OutputDebugStringW(L"Exit ProcessRequestResponseCompletedMessage 1 \n");
        return;
    }

    JsonObject^ requestResponseCompleted = this->GenerateLoadingFinishedMessage(message->RequestResponseCompletedEventArgs, requestMessage);
    this->PostProcessMessage(requestResponseCompleted);
    OutputDebugStringW(L"Exit ProcessRequestResponseCompletedMessage 2 \n");    
}

void MessageManager::AddMessageToQueueForRetry(Message^ message)
{    
    if (message->ProcessingRetries == 0)
    {
        message->ProcessingRetries++;
        _retryMutex.lock();
        _retryQueue->Append(message);
        _retryMutex.unlock();
    }
    else
    {
        auto infoMessage = std::wstring(L"WARNING: Message not processed ") + message->MessageId.ToString()->Data() + std::wstring(L"\n");
        OutputDebugStringW(infoMessage.c_str());
    }
}

JsonObject^ SerializeHeaders(IIterator<IKeyValuePair<String^, String^>^>^ iterator)
{
    JsonObject^ result = ref new JsonObject();

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
    JsonObject^ result = nullptr;

    JsonObject^ requestMessage = GetRequestMessage(data->ActivityId);
    if (requestMessage != nullptr) 
    {
        result = ref new JsonObject();
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
    }
    
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

JsonObject^ MessageManager::GetRequestMessage(Guid id)
{    
    JsonObject^ result = nullptr;

    if (_requestSentDictionary->HasKey(id))
    {
        result = _requestSentDictionary->Lookup(id);        
    }    
    
    return result;    
}

JsonObject^ MessageManager::GenerateLoadingFinishedMessage(HttpDiagnosticProviderRequestResponseCompletedEventArgs^ data, JsonObject^ requestMessage )
{
    JsonObject^ result = nullptr;
       
    if (requestMessage != nullptr)
    {
        result = ref new JsonObject();
        InsertString(result, "method", "Network.loadingFinished");
        JsonObject^ params = ref new JsonObject();
        InsertString(params, "requestId", requestMessage->GetNamedObject("params")->GetNamedString("requestId"));
        auto timeInSecs = data->Timestamps->ResponseCompletedTimestamp->Value.UniversalTime / (10000000);
        InsertNumber(params, "timestamp", timeInSecs);
        // information not provided by the message
        InsertNumber(params, "encodedDataLength", 0);
        result->Insert("params", params);
    }
      
    return result;
}

bool IsOutdated(Message^ message, long long now)
{
    auto timeSpan = now - message->TimeStamp;
    // 10 secs
    if (timeSpan > 100000000)
    {
        return true;
    }

    return false;
}

void MessageManager::OnMapChanged(IObservableMap<Platform::Guid, JsonObject ^> ^sender, IMapChangedEventArgs<Platform::Guid> ^event)
{   
    Vector<Message^>^ messagesToProcess = ref new Vector<Message^>();
    Vector<int>^ messagesToDelete = ref new Vector<int>();

    if (event->CollectionChange == CollectionChange::ItemInserted)
    {
        auto calendar = ref new Windows::Globalization::Calendar();
        calendar->SetToNow();
        auto now = calendar->GetDateTime().UniversalTime;                

        _retryMutex.lock();        
        int i = 0;
        while(i < _retryQueue->Size)
        {     
            auto message = _retryQueue->GetAt(i);
            bool isProcessed = false;

            if ( _requestSentDictionary->HasKey(message->MessageId))
            {
                messagesToProcess->Append(message);
                isProcessed = true;
            }

            if (isProcessed || IsOutdated(message, now))
            {
                _retryQueue->RemoveAt(i);
            }           
            else 
            {
                i++;
            }
        }        
        _retryMutex.unlock();

        for each (auto message in messagesToProcess)
        {
            if (message->MessageType == MessageTypes::ResponseReceived)
            {
                ProcessResponseReceivedMessage(message);
            }
            else if (message->MessageType == MessageTypes::RequestResponseCompleted)
            {
                ProcessRequestResponseCompletedMessage(message);
            }
        }
    }

    
}
