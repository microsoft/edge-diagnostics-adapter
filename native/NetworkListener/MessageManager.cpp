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
}


MessageManager::~MessageManager()
{
}

String^ MessageManager::GetNextSequenceId(IdTypes counterType)
{
    /*int counter = _idCounters[counterType]++;
    String^t = _processId.ToString() + counter;*/
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

JsonObject^ SerializeHeaders(HttpDiagnosticProviderRequestSentEventArgs ^data) 
{
    JsonObject^ result = ref new JsonObject();
    auto iterator = data->Message->Headers->First();

    while (iterator->HasCurrent)
    {
        auto header = iterator->Current;
        InsertString(result, header->Key, header->Value);
        iterator->MoveNext();
    }

    return result;
}

JsonObject ^ MessageManager::GenerateRequestWilBeSendMessage(HttpDiagnosticProviderRequestSentEventArgs ^data, String^ postPayload)
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
    request->Insert("headers", SerializeHeaders(data));
    if (postPayload != nullptr && message->Method->Method == "POST")
    {
        InsertString(request, "postData", postPayload);
    }
    InsertString(request, "initialPriority", "");

    params->Insert("request", request);        
        
    // https://docs.microsoft.com/en-us/uwp/api/windows.foundation.datetime    
    // ULARGE_INTEGER ul_time = ULARGE_INTEGER();
    // ul_time.QuadPart = data->Timestamp.UniversalTime;
    // const FILETIME fileTime = { ul_time.LowPart, ul_time.HighPart };    
    auto timeInSecs = data->Timestamp.UniversalTime / (10000000);
    InsertNumber(params,"timestamp", timeInSecs);

    //TODO:  compose wall time, maybe not possible to calculate
    InsertNumber(params, "walltime", 0);


    String^ initiator = "{\"type\": \"" + data->Initiator.ToString() + "\"}";
    JsonValue^ initiatorValue = JsonValue::Parse(initiator);
    params->Insert("initiator", initiatorValue);
    InsertString(params, "type", "Document"); // TODO: compose the type, remove hardcoded value

    result->Insert("params", params);    

    return result;
}
