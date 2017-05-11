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
using namespace concurrency;
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
        if (_httpMessages->Size == 1)
        {
            create_task([this]()
            {
                OutputDebugStringW(L"Execute from SendToProcess \n");
                this->ProcessNextMessage();                
            });
        }
        _vectorMutex.unlock();

        auto mes = std::wstring(L"Exit SendToProcess ") + std::to_wstring(_httpMessages->Size) + std::wstring(L"\n");

        OutputDebugStringW(mes.c_str());
    }
    catch (const std::exception& ex)
    {
        auto t = ex;
    }
}

void MessageManager::ProcessRequestSentMessage(Message ^ message)
{
    auto eventArgs = message->RequestSentEventArgs;
    create_task([this, eventArgs]()
    {
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
                this->PostProcessMessage(seriealizedMessage, eventArgs->ActivityId, MessageTypes::RequestSent);
                OutputDebugStringW(L"Exit POST ProcessRequestSentMessage \n");
            });
        }
        else 
        {
            JsonObject^ seriealizedMessage = this->GenerateRequestWilBeSentMessage(eventArgs);
            this->PostProcessMessage(seriealizedMessage, eventArgs->ActivityId, MessageTypes::RequestSent);
            OutputDebugStringW(L"Exit ProcessRequestSentMessage \n");
        }
    });
}

void MessageManager::PostProcessMessage(JsonObject^ jsonObject, Guid messageId, MessageTypes messageType)
{   
    try
    {
        OutputDebugStringW(L"Enter PostProcessMessage \n");        
        create_task([this,jsonObject]()
        {
            MessageProcessed(this, jsonObject);
            OutputDebugStringW(L"Execute ProcessNextMessage from PostProcessMessage \n");
            this->ProcessNextMessage();
        });          

        OutputDebugStringW(L"Exit PostProcessMessage \n");
    }
    catch (const std::exception& ex)
    {
        auto t = ex;
    }
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
            //serializedObject = GenerateResponseReceivedMessage(message->ResponseReceivedEventArgs);
            break;
        case MessageTypes::RequestResponseCompleted:
        default:
            break;
    }        

    OutputDebugStringW(L"Exit ProcessNextMessage \n");
    
}

JsonObject^ SerializeHeaders(HttpRequestMessage^ message)
{
    JsonObject^ result = ref new JsonObject();
    auto iterator = message->Headers->First();

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
    request->Insert("headers", SerializeHeaders(message));
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
    InsertString(params, "type", "Document"); // TODO: compose the type, remove hardcoded value

    result->Insert("params", params);    

    _requestSentDictionary->Insert(data->ActivityId, result);

    return result;
}

JsonObject^ MessageManager::GenerateResponseReceivedMessage(HttpDiagnosticProviderResponseReceivedEventArgs^ data)
{   
    JsonObject^ result = ref new JsonObject();
    JsonObject^ requestMessage;

    try
    {
        requestMessage = _requestSentDictionary->Lookup(data->ActivityId);
    }
    catch (const  Platform::OutOfBoundsException^ ex)
    {
        //TODO: manage the non found Guid (so no message can be done probably)
        return nullptr;
    }
    auto t = data->Message->RequestMessage;
    auto e = t->Method->Method;
    HttpResponseMessage^ message = data->Message;
    
    InsertString(result, "method", "Network.responseReceived");

    JsonObject^ params = ref new JsonObject();
    auto sentParams = requestMessage->GetNamedObject("params");    
    InsertString(params, "requestId", sentParams->GetNamedString("requestId"));
    InsertString(params, "frameId", sentParams->GetNamedString("frameId"));
    InsertString(params, "loaderId", sentParams->GetNamedString("loaderId"));

    result->Insert("params", params);
    
    return result;
}

