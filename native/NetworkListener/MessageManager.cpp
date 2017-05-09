#include "stdafx.h"
#include "MessageManager.h"

#include <ppltasks.h>

using namespace std;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Web::Http;
using namespace Platform;

MessageManager::MessageManager(unsigned int processId)
{
    _processId = processId;
    _currentMessageCounter = 1;
    _requestSentDictionary = ref new Map<Guid, JsonObject^>();
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
