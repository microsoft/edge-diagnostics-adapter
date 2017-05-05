#include "stdafx.h"
#include "MessageManager.h"


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

JsonObject ^ MessageManager::GenerateRequestWilBeSendMessage(HttpDiagnosticProviderRequestSentEventArgs ^data)
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

    params->Insert("request", request);        

    InsertString(params,"timestamp", data->Timestamp.ToString());
    InsertNumber(params, "walltime", data->Timestamp.UniversalTime);


    String^ initiator = "{\"type\": \"" + data->Initiator.ToString() + "\"}";
    JsonValue^ initiatorValue = JsonValue::Parse(initiator);
    params->Insert("initiator", initiatorValue);
    InsertString(params, "type", "Document"); // TODO: compose the type, remove hardcoded value

    result->Insert("params", params);

    // compose params (to be extracted in another helper method)


    return result;
}
