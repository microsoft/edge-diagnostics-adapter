#include "stdafx.h"
#include "MessageManager.h"
#include <string>
#include <fstream>
#include <iostream>
#include <locale>
#include <codecvt>
#include <cwctype>
#include <collection.h>
#include <ppltasks.h>

#include "base64.h"


using namespace std;
using namespace Concurrency;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Streams;
using namespace Windows::Web::Http;
using namespace Platform;
using namespace NetworkProxyLibrary;

static const pair<wstring, String^> resourceTypes[8]
{
    // non-mapped resource types are TextTrack, XHR, Fetch, EventSource, WebSocket
    { L"javascript", "Script" },
    { L"css","Stylesheet" },
    { L"image","Image" },
    { L"audio","Media" },
    { L"video","Media" },
    { L"font","Font" },
    { L"html","Document" },
    { L"manifest","Manifest" },
};

MessageManager::MessageManager(unsigned int processId)
    :_processId{ processId }
    , _currentMessageCounter(1)
    , _requestSentDictionary{ ref new Map<Guid, JsonObject^>() }
    , _retryQueue{ ref new Vector<Message^>() }
    , _responsePayloadQueue{ ref new PayloadQueue(MaxResponsePayloadsStored) }
{ }

MessageManager::~MessageManager()
{
}

String^ MessageManager::GetNextSequenceId(IdTypes counterType)
{
    // restart counter when max number is achieved
    if (_idCounters[counterType] == INT_MAX)
    {
        _idCounters[counterType] = 1;
    }
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

void MessageManager::ProcessRequest(JsonObject^ request)
{
    int id = (int)request->GetNamedNumber("id", 0);
    auto method = request->GetNamedString("method", "");

    if (method == "Network.getResponseBody"
        && request->HasKey("params")
        && request->GetNamedObject("params")->HasKey("requestId"))
    {
        auto requestId = request->GetNamedObject("params")->GetNamedString("requestId");
        auto response = GenerateGetResponseBodyMessage(id, requestId);
        PostProcessMessage(response);
    }
}

JsonObject^ MessageManager::GenerateGetResponseBodyMessage(int id, String^ requestId)
{
    auto payloadContainer = _responsePayloadQueue->Get(requestId);
    auto response = ref new JsonObject();
    auto result = ref new JsonObject();
    InsertNumber(response, "id", id);
    if (payloadContainer == nullptr)
    {
        InsertString(result, "body", "");
    }
    else
    {
        InsertString(result, "body", payloadContainer->Payload);
    }
    result->Insert("base64Encoded", JsonValue::CreateBooleanValue(true));
    response->Insert("result", result);

    return response;
}

void MessageManager::SendToProcess(Message^ message)
{
    OutputDebugStringW(L"Enter SendToProcess \n");
    this->ProcessMessage(message);
    OutputDebugStringW(L"Exit SendToProcess \n");
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
            JsonObject^ seriealizedMessage = this->GenerateRequestWillBeSentMessage(eventArgs, payload);
            this->PostProcessMessage(seriealizedMessage);
            OutputDebugStringW(L"Exit POST ProcessRequestSentMessage \n");
        });
    }
    else
    {
        try
        {
            JsonObject^ serializedMessage = this->GenerateRequestWillBeSentMessage(eventArgs);
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
// - Network.loadingFinished
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
        create_task(message->ResponseReceivedEventArgs->Message->Content->ReadAsBufferAsync()).then([this, responseReceivedMessage](IBuffer^ content)
        {
            auto reader = ::Windows::Storage::Streams::DataReader::FromBuffer(content);

            auto chunkSize = reader->UnconsumedBufferLength;
            auto offset = 0;
            std::vector<unsigned char> data(chunkSize * 2);

            while (chunkSize > 0)
            {
                if (chunkSize + offset > data.capacity()) data.resize((chunkSize + offset) * 2);
                reader->ReadBytes(
                    ::Platform::ArrayReference<unsigned char>(
                        &data[offset], chunkSize));
                offset += chunkSize;
                chunkSize = reader->UnconsumedBufferLength;
            }

            auto pInput = reinterpret_cast<const char*>(const_cast<const unsigned char*>(data.data()));
            auto output = std::string(Base64::EncodedLength(offset), '\0');
            Base64::Encode(pInput, offset, &output[0], output.size());

            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            std::wstring wide = converter.from_bytes(output);
            String^ payload64 = ref new String(wide.data());

            String^ messageId = responseReceivedMessage->GetNamedObject("params")->GetNamedString("requestId");
            _responsePayloadQueue->Add(ref new PayloadContainer(messageId, payload64));
            auto dataReceivedMessage = GenerateDataReceivedMessage(responseReceivedMessage, offset);
            this->PostProcessMessage(dataReceivedMessage);
            auto loadingFinishedMessage = GenerateLoadingFinishedMessage(dataReceivedMessage);
            this->PostProcessMessage(loadingFinishedMessage);
        });
        // request message has been used for all the possible response messages --> can be deleted from the dictionary
        DeleteRequestMessage(message->ResponseReceivedEventArgs->ActivityId);
    }
    OutputDebugStringW(L"Exit ProcessResponseReceivedMessage \n");
}

void MessageManager::AddMessageToQueueForRetry(Message^ message)
{
    if (message->ProcessingRetries < MessageManager::MessageProcessingRetries)
    {
        message->ProcessingRetries++;
        _retryMutex.lock();
        _retryQueue->Append(message);
        _retryMutex.unlock();
    }
    else
    {
        auto infoMessage = std::wstring(L"WARNING: Message not processed after ") + std::to_wstring(message->ProcessingRetries) + std::wstring(L" retries.") + message->MessageId.ToString()->Data() + std::wstring(L"\n");
        OutputDebugStringW(infoMessage.c_str());
    }
}

void AppendHeaders(JsonObject^ headersJson, IIterator<IKeyValuePair<String^, String^>^>^ iterator)
{
    while (iterator->HasCurrent)
    {
        auto header = iterator->Current;
        InsertString(headersJson, header->Key, header->Value);
        iterator->MoveNext();
    }
}

JsonObject^ SerializeHeaders(IIterator<IKeyValuePair<String^, String^>^>^ iterator)
{
    JsonObject^ result = ref new JsonObject();
    AppendHeaders(result, iterator);
    return result;
}

String^ ParseInitiator(wstring initiator)
{
    vector<wstring> scriptList{ L"CrossOriginPreFlight", L"Fetch", L"Prefetch", L"XmlHttpRequest" };
    vector<wstring> parserList{ L"HtmlDownload", L"Image", L"Link", L"Media", L"ParsedElement" };
    if (std::find(std::begin(scriptList), std::end(scriptList), initiator) != std::end(scriptList))
    {
        return "script";
    }
    else if (std::find(std::begin(parserList), std::end(parserList), initiator) != std::end(parserList))
    {
        return "parser";
    }
    else
    {
        return "other";
    }
}

bool StringContainsSubstring(wstring p_string, wstring p_substring)
{
    auto it = std::search(
        p_string.begin(), p_string.end(),
        p_substring.begin(), p_substring.end());
    return (it != p_string.end());
}

wstring ToLower(wstring text)
{
    wstring result = text;
    transform(result.begin(), result.end(), result.begin(), ::towlower);
    return result;
}

String^ ParseResourceTypeFromContentType(String^ contentType)
{
    wstring contentTypeLC = ToLower(contentType->Data());

    for (int i = 0; i < 8; i++)
    {
        if (StringContainsSubstring(contentTypeLC, resourceTypes[i].first))
        {
            return resourceTypes[i].second;
        }
    }

    return "Other";
}

JsonObject ^ MessageManager::GenerateRequestWillBeSentMessage(HttpDiagnosticProviderRequestSentEventArgs ^data, String^ postPayload)
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
    InsertNumber(params, "timestamp", timeInSecs);
    InsertNumber(params, "walltime", 0);

    JsonObject^ initiator = ref new JsonObject();
    InsertString(initiator, "type", ParseInitiator(data->Initiator.ToString()->Data()));
    params->Insert("initiator", initiator);

    InsertString(params, "type", "Other");

    result->Insert("params", params);
    _dictionaryMutex.lock();
    _requestSentDictionary->Insert(data->ActivityId, result);
    _dictionaryMutex.unlock();

    OnRequestInsertedToMap(data->ActivityId);

    return result;
}

String^ TryToGetContentTypeHeaderValue(IIterator<IKeyValuePair<String^, String^>^>^ iterator)
{
    String^ result = "";
    while (iterator->HasCurrent)
    {
        auto header = iterator->Current;
        wstring headerKeyLC = ToLower(header->Key->Data());
        if (headerKeyLC == L"content-type")
        {
            result = header->Value;
            break;
        }
        iterator->MoveNext();
    }

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
        String^ mimeType = TryToGetContentTypeHeaderValue(message->Content->Headers->First());
        if (mimeType != "")
        {
            auto resourceType = ParseResourceTypeFromContentType(mimeType);
            InsertString(params, "type", resourceType);
        }

        JsonObject^ response = ref new JsonObject();
        InsertString(response, "url", sentParams->GetNamedString("documentURL"));
        InsertNumber(response, "status", static_cast<int>(message->StatusCode));
        InsertString(response, "statusText", message->ReasonPhrase);

        JsonObject^ headers = SerializeHeaders(message->Headers->First());
        AppendHeaders(headers, message->Content->Headers->First());
        response->Insert("headers", headers);

        InsertString(response, "mimeType", mimeType);
        response->Insert("requestHeaders", sentParams->GetNamedObject("request")->GetNamedObject("headers"));
        InsertString(response, "protocol", message->Version.ToString());

        params->Insert("response", response);
        result->Insert("params", params);
    }

    return result;
}

bool ContainsAnyEncodingHeader(JsonObject^ headers)
{
    bool headerFound = false;
    auto iterator = headers->First();
    while (iterator->HasCurrent)
    {
        auto header = iterator->Current;
        wstring headerKeyLC = ToLower(header->Key->Data());

        if (StringContainsSubstring(headerKeyLC, L"encoding"))
        {
            headerFound = true;
            break;
        }
        iterator->MoveNext();
    }

    return headerFound;
}

double TryGetContentLengthHeaderValue(JsonObject^ headers)
{
    double result = 0;
    auto iterator = headers->First();
    //Search content-lenght header
    while (iterator->HasCurrent)
    {
        auto header = iterator->Current;
        wstring headerKeyLC = ToLower(header->Key->Data());
        if (headerKeyLC == L"content-length")
        {
            result = _wtol((header->Value->GetString()->Data()));
            break;
        }
        iterator->MoveNext();
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

    auto headers = responseReceivedMessage->GetNamedObject("params")->GetNamedObject("response")->GetNamedObject("headers");
    double encodedBytes = 0;
    if (ContainsAnyEncodingHeader(headers))
    {
        // "Content-Length"
        encodedBytes = TryGetContentLengthHeaderValue(headers);
    }
    InsertNumber(params, "encodedDataLength", encodedBytes);

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

bool IsOutdated(long long timeStamp, long long now, int timeoutSecs)
{
    auto timeSpan = now - timeStamp;
    // timestamps are in 10^-7 secs format
    if (timeSpan > (timeoutSecs * 10000000))
    {
        return true;
    }

    return false;
}

void MessageManager::DeleteRequestMessage(Guid id)
{
    _dictionaryMutex.lock();
    if (_requestSentDictionary->HasKey(id))
    {
        _requestSentDictionary->Remove(id);
    }

    // clean old requests in case they have not been explicitly called for being removed
    auto calendar = ref new Windows::Globalization::Calendar();
    calendar->SetToNow();
    auto now = calendar->GetDateTime().UniversalTime;
    Vector<Guid>^ messagesToDelete = ref new Vector<Guid>();
    auto iterator = _requestSentDictionary->First();
    while (iterator->HasCurrent)
    {
        auto item = iterator->Current->Value;
        // convert timestamp from secs to the standard date time format, 10^-7 secs  
        long long timeStamp = item->GetNamedObject("params")->GetNamedNumber("timestamp") * 10000000;
        if (IsOutdated(timeStamp, now, 60))
        {
            messagesToDelete->Append(iterator->Current->Key);
        }
        iterator->MoveNext();
    }
    for each (auto id in messagesToDelete)
    {
        _requestSentDictionary->Remove(id);
    }
    _dictionaryMutex.unlock();
}

JsonObject^ MessageManager::GenerateLoadingFinishedMessage(JsonObject^ dataReceivedMessage)
{
    JsonObject^ result = nullptr;

    JsonObject^ dataReceivedParams = dataReceivedMessage->GetNamedObject("params");

    result = ref new JsonObject();
    InsertString(result, "method", "Network.loadingFinished");
    JsonObject^ params = ref new JsonObject();
    InsertString(params, "requestId", dataReceivedParams->GetNamedString("requestId"));
    InsertNumber(params, "timestamp", dataReceivedParams->GetNamedNumber("timestamp"));
    InsertNumber(params, "encodedDataLength", dataReceivedParams->GetNamedNumber("encodedDataLength"));
    result->Insert("params", params);

    return result;
}

void MessageManager::OnRequestInsertedToMap(Guid id)
{
    Vector<Message^>^ messagesToProcess = ref new Vector<Message^>();
    Vector<int>^ messagesToDelete = ref new Vector<int>();

    auto calendar = ref new Windows::Globalization::Calendar();
    calendar->SetToNow();
    auto now = calendar->GetDateTime().UniversalTime;

    _retryMutex.lock();
    int i = 0;
    while (i < _retryQueue->Size)
    {
        auto message = _retryQueue->GetAt(i);
        bool isProcessed = false;

        if (_requestSentDictionary->HasKey(message->MessageId))
        {
            messagesToProcess->Append(message);
            isProcessed = true;
        }

        if (isProcessed || IsOutdated(message->TimeStamp, now, 10))
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
    }
}
